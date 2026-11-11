// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/bindings/python/sender_bridge.h"

#include <csignal>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

#include "cast/common/public/trust_store.h"
#include "cast/standalone_sender/connection_settings.h"
#include "cast/standalone_sender/looping_file_cast_agent.h"
#include "cast/standalone_sender/testing/packet_drop_environment.h"
#include "cast/streaming/message_fields.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/base/ip_address.h"
#include "platform/impl/logging.h"

namespace openscreen::cast {

CastSenderBridge::CastSenderBridge(TaskRunner* task_runner,
                                   IPAddress ip_address,
                                   int port,
                                   std::string_view cert_path)
    : ip_address_(ip_address),
      port_(port),
      cert_path_(cert_path),
      task_runner_(task_runner) {}

CastSenderBridge::~CastSenderBridge() {
  Teardown();
}

bool CastSenderBridge::StreamVideo(const StreamConfig& config, bool is_debug) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::kTearingDown) {
      OSP_LOG_ERROR << "StreamVideo called during active teardown.";
      return false;
    }
    if (state_ != State::kInitializing) {
      OSP_LOG_ERROR << "StreamVideo called while streaming is already active.";
      return false;
    }
    state_ = State::kStreaming;
  }

  const auto log_level =
      is_debug ? openscreen::LogLevel::kInfo : openscreen::LogLevel::kWarning;
  openscreen::SetLogLevel(log_level);
  std::signal(SIGPIPE, SIG_IGN);

  const auto parsed_codec = StringToVideoCodec(config.codec_name);
  if (parsed_codec.is_error()) {
    OSP_LOG_ERROR << "Unsupported video codec requested: " << config.codec_name;
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::kInitializing;
    return false;
  }

  if (config.packet_drop_config.has_value() &&
      !config.packet_drop_config->IsValid()) {
    const auto& drop = *config.packet_drop_config;
    OSP_LOG_ERROR << "Invalid packet drop settings: start_delay_ms="
                  << drop.start_delay.count()
                  << ", total_duration_ms=" << drop.total_duration.count()
                  << ", drop_ratio=" << drop.drop_ratio
                  << ", burst_duration_ms=" << drop.burst_duration.count();
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::kInitializing;
    return false;
  }

  // Return false if vp8/vp9 codec is requested but LibVPX is not supported
#if !defined(CAST_STANDALONE_SENDER_HAVE_LIBVPX)
  if (parsed_codec.value() == VideoCodec::kVp8 ||
      parsed_codec.value() == VideoCodec::kVp9) {
    OSP_LOG_ERROR
        << "VP8/VP9 codec requested but LibVPX is not installed in this build.";
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::kInitializing;
    return false;
  }
#endif

  // Return false if av1 codec is requested but not supported
#if !defined(CAST_STANDALONE_SENDER_HAVE_LIBAOM)
  if (parsed_codec.value() == VideoCodec::kAv1) {
    OSP_LOG_ERROR
        << "AV1 codec requested but LibAOM is not installed in this build.";
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::kInitializing;
    return false;
  }
#endif

  // Return false if h264/hevc codec is requested but FFmpeg is not supported
#if !defined(CAST_STANDALONE_SENDER_HAVE_FFMPEG)
  if (parsed_codec.value() == VideoCodec::kH264 ||
      parsed_codec.value() == VideoCodec::kHevc) {
    OSP_LOG_ERROR << "H264/HEVC codec requested but FFmpeg is not installed in "
                     "this build.";
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::kInitializing;
    return false;
  }
#endif

  const std::optional<ConnectionSettings::PreconfiguredSessionInfo>
      preconfigured_session_info =
          config.remote_transport_id.empty()
              ? std::nullopt
              : std::make_optional<
                    ConnectionSettings::PreconfiguredSessionInfo>(
                    ConnectionSettings::PreconfiguredSessionInfo{
                        .remote_transport_id = config.remote_transport_id,
                        .app_session_id = config.session_id,
                    });

  const ConnectionSettings settings{
      .receiver_endpoint =
          IPEndpoint{ip_address_, static_cast<uint16_t>(port_)},
      .path_to_file = config.file_path,
      .max_bitrate = config.max_bitrate,
      .should_include_video = true,
      .should_include_audio = config.should_include_audio,
      .should_loop_video = false,
      .codec = parsed_codec.value(),
      .preconfigured_session_info = preconfigured_session_info,
  };

  LoopingFileCastAgent::EnvironmentFactory env_factory;
  if (config.packet_drop_config.has_value()) {
    const PacketDropConfig drop_config = *config.packet_drop_config;
    env_factory = [drop_config](ClockNowFunctionPtr now_function,
                                TaskRunner& task_runner,
                                const IPEndpoint& local_endpoint) {
      return std::make_unique<PacketDropEnvironment>(
          now_function, task_runner, local_endpoint, drop_config);
    };
  }

  std::unique_ptr<TrustStore> cast_trust_store;
  if (!cert_path_.empty()) {
    cast_trust_store = TrustStore::CreateInstanceFromPemFile(cert_path_);
  }
  if (!cast_trust_store) {
    cast_trust_store = CastTrustStore::Create();
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::kTearingDown) {
      state_ = State::kInitializing;
      return false;
    }
    task_runner_->PostTask([this, settings, store = std::move(cast_trust_store),
                            env_factory = std::move(env_factory)]() mutable {
      std::lock_guard<std::mutex> lock(mutex_);
      if (state_ != State::kStreaming) {
        return;
      }
      cast_agent_ = std::make_unique<LoopingFileCastAgent>(
          *task_runner_, std::move(store), [this] { OnAgentShutdown(); },
          std::move(env_factory));
      cast_agent_->Connect(settings);
    });
  }

  return true;
}

std::optional<Clock::time_point> CastSenderBridge::GetStreamingStartTime()
    const {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != State::kStreaming) {
      return std::nullopt;
    }
  }

  OSP_CHECK(!task_runner_->IsRunningOnTaskRunner());

  std::promise<std::optional<Clock::time_point>> start_time_promise;
  std::future<std::optional<Clock::time_point>> start_time_future =
      start_time_promise.get_future();

  task_runner_->PostTask(
      [this, promise = std::move(start_time_promise)]() mutable {
        std::optional<Clock::time_point> result;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          if (cast_agent_) {
            result = cast_agent_->GetStreamingStartTime();
          }
        }
        promise.set_value(result);
      });

  return start_time_future.get();
}

void CastSenderBridge::Teardown() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::kInitializing || state_ == State::kTearingDown) {
      return;
    }
    state_ = State::kTearingDown;
  }

  OSP_CHECK(!task_runner_->IsRunningOnTaskRunner());

  std::promise<void> destroy_promise;
  std::future<void> destroy_future = destroy_promise.get_future();

  task_runner_->PostTask(
      [this, promise = std::move(destroy_promise)]() mutable {
        ResetAgentOnTaskRunner();
        task_runner_->PostTask(
            [promise = std::move(promise)]() mutable { promise.set_value(); });
      });

  destroy_future.wait();
}

void CastSenderBridge::OnAgentShutdown() {
  // Defer reset to the next task runner turn so the agent does not delete
  // itself on the active call stack or relock the mutex during teardown.
  task_runner_->PostTask([this] { ResetAgentOnTaskRunner(); });
}

void CastSenderBridge::ResetAgentOnTaskRunner() {
  std::lock_guard<std::mutex> lock(mutex_);
  cast_agent_.reset();
  state_ = State::kInitializing;
}

}  // namespace openscreen::cast
