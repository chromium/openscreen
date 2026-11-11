// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/testing/video_packet_dropper.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <random>

#include "util/osp_logging.h"

namespace openscreen::cast {
namespace {

// 12-byte RFC 3550 RTP header + 6-byte Cast RTP header; RTP v2 (0x80) with
// dynamic PT in [96, 127] (excludes RTCP such as SR PT=200 -> 72 < 96).
constexpr size_t kMinCastRtpHeaderSize = 18;
constexpr uint8_t kRtpRequiredFirstByte = 0x80;
constexpr uint8_t kRtpPayloadTypeMask = 0x7f;
constexpr uint8_t kMinDynamicRtpPayloadType = 96;

bool IsVideoRtpPacket(ByteView packet, StreamType stream_type) {
  return stream_type == StreamType::kVideo &&
         packet.size() >= kMinCastRtpHeaderSize &&
         packet[0] == kRtpRequiredFirstByte &&
         (packet[1] & kRtpPayloadTypeMask) >= kMinDynamicRtpPayloadType;
}

}  // namespace

VideoPacketDropper::VideoPacketDropper(PacketDropConfig config)
    : config_(config), rng_(config_.seed) {
  OSP_CHECK(config_.IsValid());
}

bool VideoPacketDropper::ShouldDrop(ByteView packet,
                                    StreamType stream_type,
                                    Clock::time_point now) {
  if (!IsVideoRtpPacket(packet, stream_type)) {
    return false;
  }
  if (!active_start_.has_value()) {
    active_start_ = now + Clock::to_duration(config_.start_delay);
    bad_end_ = *active_start_;
  }

  const Clock::time_point active_end =
      *active_start_ + Clock::to_duration(config_.total_duration);
  if (now < *active_start_ || now >= active_end) {
    return false;
  }
  if (config_.drop_ratio >= 1.0) {
    return true;
  }

  while (now >= bad_end_) {
    bad_start_ = bad_end_ + SampleGoodDuration();
    bad_end_ = bad_start_ + Clock::to_duration(config_.burst_duration);
  }
  return now >= bad_start_;
}

Clock::duration VideoPacketDropper::SampleGoodDuration() {
  const double burst_seconds =
      std::chrono::duration<double>(config_.burst_duration).count();
  const double mean_good_seconds =
      burst_seconds * (1.0 - config_.drop_ratio) / config_.drop_ratio;
  const double max_good_seconds =
      std::chrono::duration<double>(config_.total_duration).count();
  const double sampled_seconds = std::min(
      std::exponential_distribution<double>()(rng_) * mean_good_seconds,
      max_good_seconds);
  return std::chrono::duration_cast<Clock::duration>(
      std::chrono::duration<double>(sampled_seconds));
}

}  // namespace openscreen::cast
