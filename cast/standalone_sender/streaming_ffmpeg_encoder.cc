// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/streaming_ffmpeg_encoder.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <string_view>
#include <utility>

#include "cast/standalone_sender/streaming_encoder_util.h"
#include "cast/streaming/message_fields.h"
#include "cast/streaming/public/constants.h"
#include "cast/streaming/public/encoded_frame.h"
#include "cast/streaming/public/frame_trace_ids.h"
#include "cast/streaming/public/sender.h"
#include "util/chrono_helpers.h"
#include "util/osp_logging.h"
#include "util/trace_logging.h"

namespace openscreen::cast {

namespace {

// Lower and upper bounds to the frame duration passed to the encoder, to ensure
// sanity. Note that the upper-bound is especially important in cases where the
// video paused for some lengthy amount of time.
constexpr Clock::duration kMinFrameDuration = milliseconds(1);
constexpr Clock::duration kMaxFrameDuration = milliseconds(125);

// Represents the recognized encoder backend implementation.
enum class FfmpegEncoderBackend {
  kLibx264_Libx265,
  kVideoToolbox,
  kNvidiaNvenc,
  kIntelQsv,
  kVaapi,
  kGeneric,
};

// Identifies the encoder backend from the AVCodec.
FfmpegEncoderBackend GetEncoderBackend(const AVCodec* codec) {
  if (!codec || !codec->name) {
    return FfmpegEncoderBackend::kGeneric;
  }
  const std::string_view name(codec->name);
  if (name.find("x264") != std::string_view::npos ||
      name.find("x265") != std::string_view::npos) {
    return FfmpegEncoderBackend::kLibx264_Libx265;
  }
  if (name.find("videotoolbox") != std::string_view::npos) {
    return FfmpegEncoderBackend::kVideoToolbox;
  }
  if (name.find("nvenc") != std::string_view::npos) {
    return FfmpegEncoderBackend::kNvidiaNvenc;
  }
  if (name.find("qsv") != std::string_view::npos) {
    return FfmpegEncoderBackend::kIntelQsv;
  }
  if (name.find("vaapi") != std::string_view::npos) {
    return FfmpegEncoderBackend::kVaapi;
  }
  return FfmpegEncoderBackend::kGeneric;
}

// Applies universal low-latency context flags and codec-specific tuning
// options for real-time Cast streaming across various FFmpeg encoders.
void ApplyLowLatencyOptions(const AVCodec* codec,
                            AVCodecContext* context,
                            AVDictionary** opts) {
  OSP_CHECK(codec);
  OSP_CHECK(context);
  OSP_CHECK(opts);

  // Universal FFmpeg low-latency flags supported across encoders:
  context->flags |= AV_CODEC_FLAG_LOW_DELAY;
  context->max_b_frames = 0;
  context->thread_type = FF_THREAD_SLICE;

  // Codec-specific private AVOptions:
  switch (GetEncoderBackend(codec)) {
    case FfmpegEncoderBackend::kLibx264_Libx265:
      av_dict_set(opts, "preset", "ultrafast", 0);
      av_dict_set(opts, "tune", "zerolatency", 0);
      // Ensure forced keyframes produce IDR frames with parameter sets.
      av_dict_set(opts, "forced-idr", "1", 0);
      // Ensure zero-frame latency in libx265 by restricting frame-threads to 1.
      av_dict_set(opts, "x265-params",
                  "frame-threads=1:log-level=none:no-info=1", 0);
      break;

    case FfmpegEncoderBackend::kVideoToolbox:
      av_dict_set(opts, "realtime", "1", 0);
      av_dict_set(opts, "prio_speed", "1", 0);
      break;

    case FfmpegEncoderBackend::kNvidiaNvenc:
      av_dict_set(opts, "preset", "p1", 0);
      av_dict_set(opts, "tune", "ull", 0);
      av_dict_set(opts, "delay", "0", 0);
      break;

    case FfmpegEncoderBackend::kIntelQsv:
      av_dict_set(opts, "preset", "veryfast", 0);
      av_dict_set(opts, "async_depth", "1", 0);
      break;

    case FfmpegEncoderBackend::kVaapi:
      av_dict_set(opts, "low_power", "1", 0);
      break;

    case FfmpegEncoderBackend::kGeneric:
      // Generic/unknown encoders rely entirely on the universal AVCodecContext
      // flags. No private dictionary options are set.
      break;
  }
}

// Allocates an AVFrame with 32-byte alignment and copies the content from
// `frame` to it.
AVFrameUniquePtr CloneAsAVFrame(
    const StreamingVideoEncoder::VideoFrame& frame) {
  OSP_CHECK_GT(frame.width, 0);
  OSP_CHECK_GT(frame.height, 0);
  OSP_CHECK_EQ(frame.width % 2, 0) << "H.264/HEVC YUV420P requires even width.";
  OSP_CHECK_EQ(frame.height % 2, 0)
      << "H.264/HEVC YUV420P requires even height.";
  OSP_CHECK_GE(frame.yuv_strides[0], frame.width);
  OSP_CHECK_GE(frame.yuv_strides[1], frame.width / 2);
  OSP_CHECK_GE(frame.yuv_strides[2], frame.width / 2);
  OSP_CHECK(frame.yuv_planes[0]);
  OSP_CHECK(frame.yuv_planes[1]);
  OSP_CHECK(frame.yuv_planes[2]);

  AVFrameUniquePtr image = MakeUniqueAVFrame();
  image->format = AV_PIX_FMT_YUV420P;
  image->width = frame.width;
  image->height = frame.height;
  if (av_frame_get_buffer(image.get(), 32) < 0) {
    OSP_LOG_ERROR << "Failed to allocate AVFrame buffer in CloneAsAVFrame.";
    return nullptr;
  }

  CopyPlane(frame.yuv_planes[0], frame.yuv_strides[0], frame.height,
            image->data[0], image->linesize[0]);
  CopyPlane(frame.yuv_planes[1], frame.yuv_strides[1], frame.height / 2,
            image->data[1], image->linesize[1]);
  CopyPlane(frame.yuv_planes[2], frame.yuv_strides[2], frame.height / 2,
            image->data[2], image->linesize[2]);

  return image;
}

}  // namespace

bool ShouldReinitHevcEncoder(int64_t current_bitrate,
                             int64_t target_bitrate,
                             int stable_target_frames) {
  OSP_CHECK_GT(current_bitrate, 0);
  OSP_CHECK_GT(target_bitrate, 0);
  if (target_bitrate >= current_bitrate * kHevcBitrateReinitRatio ||
      current_bitrate >= target_bitrate * kHevcBitrateReinitRatio) {
    return true;
  }
  const int64_t bitrate_diff = target_bitrate > current_bitrate
                                   ? target_bitrate - current_bitrate
                                   : current_bitrate - target_bitrate;
  return stable_target_frames >= kHevcBitrateNumFramesBeforeSettling &&
         bitrate_diff >= current_bitrate * kHevcBitrateSettlingFactor;
}

StreamingFfmpegEncoder::StreamingFfmpegEncoder(const Parameters& params,
                                               TaskRunner& task_runner,
                                               std::unique_ptr<Sender> sender)
    : StreamingVideoEncoder(params, task_runner, std::move(sender)) {
  ideal_speed_setting_ = 10.0;
  current_speed_setting_ = 10;
  encode_thread_ = std::thread([this] { ProcessWorkUnitsUntilTimeToQuit(); });
}

StreamingFfmpegEncoder::~StreamingFfmpegEncoder() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    target_bitrate_ = 0;
    cv_.notify_one();
  }
  encode_thread_.join();
}

int StreamingFfmpegEncoder::GetTargetBitrate() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return target_bitrate_;
}

void StreamingFfmpegEncoder::SetTargetBitrate(int new_bitrate) {
  // Ensure bitrate will not be zero.
  new_bitrate = std::max(new_bitrate, kBytesPerKilobyte);

  std::lock_guard<std::mutex> lock(mutex_);
  // Only assign the new target bitrate if `target_bitrate_` has not yet been
  // used to signal the `encode_thread_` to end.
  if (target_bitrate_ > 0) {
    target_bitrate_ = new_bitrate;
  }
}

void StreamingFfmpegEncoder::EncodeAndSend(
    const VideoFrame& frame,
    Clock::time_point reference_time,
    std::function<void(Stats)> stats_callback) {
  OSP_DCHECK(main_task_runner_.IsRunningOnTaskRunner());
  WorkUnit work_unit;
  work_unit.capture_begin_time = frame.capture_begin_time;
  work_unit.capture_end_time = frame.capture_end_time;

  if (start_time_ == Clock::time_point::min()) {
    start_time_ = reference_time;
    work_unit.rtp_timestamp = RtpTimeTicks();
  } else {
    work_unit.rtp_timestamp = RtpTimeTicks::FromTimeSinceOrigin(
        reference_time - start_time_, sender_->config().rtp_timebase);
    if (work_unit.rtp_timestamp <= last_enqueued_rtp_timestamp_) {
      OSP_LOG_WARN << "VIDEO[" << sender_->config().sender_ssrc
                   << "] Dropping: RTP timestamp not monotonically increasing.";
      return;
    }
  }

  if (sender_->GetInFlightMediaDuration(work_unit.rtp_timestamp) >
      sender_->GetMaxInFlightMediaDuration()) {
    OSP_LOG_WARN << "VIDEO[" << sender_->config().sender_ssrc
                 << "] Dropping: In-flight media duration would be too high.";
    return;
  }

  Clock::duration frame_duration = frame.duration;
  if (frame_duration <= Clock::duration::zero()) {
    if (reference_time == start_time_) {
      // Use the max for the first frame for the quality.
      frame_duration = kMaxFrameDuration;
    } else {
      // Use the actual amount of time between the current and previous frame as
      // a prediction for the next frame's duration.
      frame_duration =
          (work_unit.rtp_timestamp - last_enqueued_rtp_timestamp_)
              .ToDuration<Clock::duration>(sender_->config().rtp_timebase);
    }
  }
  work_unit.duration =
      std::max(std::min(frame_duration, kMaxFrameDuration), kMinFrameDuration);

  last_enqueued_rtp_timestamp_ = work_unit.rtp_timestamp;
  work_unit.image = CloneAsAVFrame(frame);
  if (!work_unit.image) {
    OSP_LOG_ERROR << "Failed to clone video frame into AVFrame.";
    return;
  }

  // Note: The "Frame.Capture" trace events are emitted by the Sender, from the
  // capture times in the EncodedFrame.
  work_unit.flow_id = GetRtpTimestampFlowId(sender_->config().sender_ssrc,
                                            work_unit.rtp_timestamp);

  work_unit.reference_time = reference_time;
  work_unit.stats_callback = std::move(stats_callback);

  const bool force_key_frame = sender_->NeedsKeyFrame();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    needs_key_frame_ |= force_key_frame;
    encode_queue_.push(std::move(work_unit));
    cv_.notify_one();
  }
}

void StreamingFfmpegEncoder::DestroyEncoder() {
  OSP_DCHECK_EQ(std::this_thread::get_id(), encode_thread_.get_id());
  encoder_.reset();
}

void StreamingFfmpegEncoder::ProcessWorkUnitsUntilTimeToQuit() {
  for (;;) {
    WorkUnitWithResults work_unit{};
    bool force_key_frame;
    int target_bitrate;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (target_bitrate_ <= 0) {
        break;  // Time to end this thread.
      }
      if (encode_queue_.empty()) {
        cv_.wait(lock);
        if (encode_queue_.empty()) {
          continue;
        }
      }
      work_unit = std::move(encode_queue_.front());
      encode_queue_.pop();
      force_key_frame = needs_key_frame_;
      needs_key_frame_ = false;
      target_bitrate = target_bitrate_;
    }

    // Clock::now() is being called directly, instead of using a
    // dependency-injected "now function," since actual wall time is being
    // measured.
    const Clock::time_point encode_start_time = Clock::now();
    TRACE_FLOW_STEP_WITH_TIME(TraceCategory::kStandaloneSender,
                              "Frame.Encode.Begin", work_unit.flow_id,
                              encode_start_time);
    PrepareEncoder(work_unit.image->width, work_unit.image->height,
                   target_bitrate);
    EncodeFrame(force_key_frame, work_unit);
    const Clock::time_point encode_end_time = Clock::now();
    TRACE_FLOW_STEP_WITH_TIME(TraceCategory::kStandaloneSender,
                              "Frame.Encode.End", work_unit.flow_id,
                              encode_end_time);
    const int effective_bitrate =
        encoder_ ? static_cast<int>(encoder_->context->bit_rate)
                 : target_bitrate;
    ComputeFrameEncodeStats(encode_end_time - encode_start_time,
                            effective_bitrate, work_unit);
    UpdateSpeedSettingForNextFrame(work_unit.stats);

    main_task_runner_.PostTask([weak_this = weak_factory_.GetWeakPtr(),
                                results = std::move(work_unit)]() mutable {
      if (weak_this) {
        weak_this->SendEncodedFrame(std::move(results));
      }
    });
  }

  DestroyEncoder();
}

void StreamingFfmpegEncoder::PrepareEncoder(int width,
                                            int height,
                                            int target_bitrate) {
  OSP_CHECK_GT(width, 0);
  OSP_CHECK_GT(height, 0);
  OSP_CHECK_GT(target_bitrate, 0);
  OSP_DCHECK_EQ(std::this_thread::get_id(), encode_thread_.get_id());

  // If the frame size changed, destroy the existing encoder instance and
  // re-create it below.
  if (encoder_ && (encoder_->context->width != width ||
                   encoder_->context->height != height)) {
    DestroyEncoder();
  }

  if (!encoder_) {
    const AVCodecID codec_id = (params_.codec == VideoCodec::kHevc)
                                   ? AV_CODEC_ID_HEVC
                                   : AV_CODEC_ID_H264;
    // Prefer software libx264 / libx265 for consistent software encoding, with
    // fallback to the default system encoder if not found.
    const char* const preferred_name =
        (params_.codec == VideoCodec::kHevc) ? "libx265" : "libx264";
    const AVCodec* codec = avcodec_find_encoder_by_name(preferred_name);
    if (!codec) {
      codec = avcodec_find_encoder(codec_id);
    }
    if (!codec) {
      OSP_LOG_ERROR << "FFmpeg video encoder not found for "
                    << CodecToString(params_.codec);
      return;
    }

    auto context = MakeUniqueAVCodecContext(codec);
    context->width = width;
    context->height = height;
    context->pix_fmt = AV_PIX_FMT_YUV420P;
    context->time_base = AVRational{1, sender_->config().rtp_timebase};
    context->framerate = AVRational{30, 1};
    context->bit_rate = target_bitrate;
    context->rc_max_rate = target_bitrate;
    // Set VBV buffer to 500ms of target bitrate for real-time Cast rate
    // control.
    context->rc_buffer_size = target_bitrate / 2;
    // Disable periodic keyframes; keyframes are generated on-demand by Cast.
    context->gop_size = 999999;
    context->keyint_min = 999999;
    context->thread_count = params_.num_encode_threads;

    AVDictionary* opts = nullptr;
    ApplyLowLatencyOptions(codec, context.get(), &opts);
    const int ret = avcodec_open2(context.get(), codec, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
      OSP_LOG_ERROR << "Failed to open FFmpeg video encoder (" << codec->name
                    << "): " << AvErrorToString(ret);
      return;
    }

    auto packet = MakeUniqueAVPacket();
    encoder_ = std::make_unique<EncoderContext>(EncoderContext{
        std::move(context), std::move(packet), target_bitrate, 1});
  } else if (params_.codec == VideoCodec::kHevc) {
    if (target_bitrate == encoder_->last_target_bitrate) {
      ++encoder_->stable_target_frames;
    } else {
      encoder_->last_target_bitrate = target_bitrate;
      encoder_->stable_target_frames = 1;
    }
    if (ShouldReinitHevcEncoder(encoder_->context->bit_rate, target_bitrate,
                                encoder_->stable_target_frames)) {
      DestroyEncoder();
      PrepareEncoder(width, height, target_bitrate);
      return;
    }
  } else {
    // libx264 dynamically handles bitrate updates via x264_encoder_reconfig.
    encoder_->context->bit_rate = target_bitrate;
    encoder_->context->rc_max_rate = target_bitrate;
    encoder_->context->rc_buffer_size = target_bitrate / 2;
  }
}

void StreamingFfmpegEncoder::EncodeFrame(bool force_key_frame,
                                         WorkUnitWithResults& work_unit) {
  OSP_DCHECK_EQ(std::this_thread::get_id(), encode_thread_.get_id());
  if (!encoder_ || !work_unit.image) {
    return;
  }

  AVFrame* frame = work_unit.image.get();
  frame->pts =
      (work_unit.rtp_timestamp - RtpTimeTicks()) / RtpTimeDelta::FromTicks(1);
  if (force_key_frame) {
    frame->pict_type = AV_PICTURE_TYPE_I;
#if defined(AV_FRAME_FLAG_KEY)
    frame->flags |= AV_FRAME_FLAG_KEY;
#else
    frame->key_frame = 1;
#endif
  } else {
    // Set dependent frames as forward predictive P-frames.
    frame->pict_type = AV_PICTURE_TYPE_P;
#if defined(AV_FRAME_FLAG_KEY)
    frame->flags &= ~AV_FRAME_FLAG_KEY;
#else
    frame->key_frame = 0;
#endif
  }

  const int send_result = avcodec_send_frame(encoder_->context.get(), frame);
  if (send_result < 0) {
    OSP_LOG_ERROR << "avcodec_send_frame failed: "
                  << AvErrorToString(send_result);
    return;
  }

  // Drain all packets generated for this frame.
  while (avcodec_receive_packet(encoder_->context.get(),
                                encoder_->packet.get()) == 0) {
    work_unit.payload.insert(work_unit.payload.end(), encoder_->packet->data,
                             encoder_->packet->data + encoder_->packet->size);
    if (encoder_->packet->flags & AV_PKT_FLAG_KEY) {
      work_unit.is_key_frame = true;
    }
    av_packet_unref(encoder_->packet.get());
  }
}

void StreamingFfmpegEncoder::ComputeFrameEncodeStats(
    Clock::duration encode_wall_time,
    int target_bitrate,
    WorkUnitWithResults& work_unit) {
  Stats& stats = work_unit.stats;
  stats.rtp_timestamp = work_unit.rtp_timestamp;
  stats.encode_wall_time = encode_wall_time;
  stats.frame_duration = work_unit.duration;
  stats.encoded_size = work_unit.payload.size();

  const double duration_in_seconds =
      std::chrono::duration<double>(work_unit.duration).count();
  stats.target_size = (target_bitrate * duration_in_seconds) / CHAR_BIT;

  // In H.264 and H.265, the Quantization Parameter (QP) is logarithmic:
  // increasing QP by 6 doubles the quantization step size and approximately
  // halves the generated bitrate. We scale the baseline QP to the [0, 63] range
  // and compute the target quantizer using a logarithmic delta.
  constexpr double kH264ToTargetScale = 63.0 / 51.0;
  constexpr double kBaseQp = 23.0;

  // Base QP scaled to the [0, 63] range.
  const double base_scaled_qp = kBaseQp * kH264ToTargetScale;

  const double utilization =
      (stats.encoded_size > 0 && stats.space_utilization() > 0.0)
          ? stats.space_utilization()
          : 1.0;

  // Logarithmic delta: +6 in QP space per doubling of frame size.
  const double qp_delta = 6.0 * std::log2(utilization) * kH264ToTargetScale;

  // Estimated effective quantizer.
  stats.quantizer = std::clamp(static_cast<int>(std::round(base_scaled_qp)),
                               kMinQuantizer, kMaxQuantizer);

  // Quantizer needed to hit the target budget relative to the current frame's
  // quantizer.
  stats.perfect_quantizer =
      std::clamp(static_cast<int>(std::round(stats.quantizer + qp_delta)),
                 kMinQuantizer, kMaxQuantizer);
}

void StreamingFfmpegEncoder::SendEncodedFrame(WorkUnitWithResults results) {
  OSP_CHECK(main_task_runner_.IsRunningOnTaskRunner());

  if (results.payload.empty()) {
    std::lock_guard<std::mutex> lock(mutex_);
    needs_key_frame_ = true;
    if (results.stats_callback) {
      results.stats.frame_id = FrameId::first();
      results.stats_callback(results.stats);
    }
    return;
  }

  EncodedFrame frame;
  frame.frame_id = sender_->GetNextFrameId();
  if (results.is_key_frame) {
    frame.dependency = EncodedFrame::Dependency::kKeyFrame;
    frame.referenced_frame_id = frame.frame_id;
  } else {
    frame.dependency = EncodedFrame::Dependency::kDependent;
    frame.referenced_frame_id = frame.frame_id - 1;
  }
  frame.rtp_timestamp = results.rtp_timestamp;
  frame.capture_begin_time = results.capture_begin_time;
  frame.capture_end_time = results.capture_end_time;
  frame.reference_time = results.reference_time;
  frame.data = results.payload;

  if (sender_->EnqueueFrame(frame) != Sender::OK) {
    std::lock_guard<std::mutex> lock(mutex_);
    needs_key_frame_ = true;
  }

  if (results.stats_callback) {
    results.stats.frame_id = frame.frame_id;
    results.stats_callback(results.stats);
  }
}

}  // namespace openscreen::cast
