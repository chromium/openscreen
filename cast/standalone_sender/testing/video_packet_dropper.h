// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_SENDER_TESTING_VIDEO_PACKET_DROPPER_H_
#define CAST_STANDALONE_SENDER_TESTING_VIDEO_PACKET_DROPPER_H_

#include <chrono>
#include <cstdint>
#include <optional>
#include <random>

#include "cast/streaming/public/constants.h"
#include "platform/api/time.h"
#include "platform/base/span.h"

namespace openscreen::cast {

// Settings for `VideoPacketDropper`. Starting `start_delay` after the first
// video RTP packet, outage bursts of `burst_duration` occur for
// `total_duration`, covering a `drop_ratio` fraction of that time on average.
struct PacketDropConfig {
  static constexpr double kDefaultDropRatio = 0.2;
  static constexpr std::chrono::milliseconds kDefaultBurstDuration{250};
  static constexpr uint32_t kDefaultSeed = 1;

  std::chrono::milliseconds start_delay{0};
  std::chrono::milliseconds total_duration{0};
  double drop_ratio = kDefaultDropRatio;
  std::chrono::milliseconds burst_duration = kDefaultBurstDuration;
  uint32_t seed = kDefaultSeed;

  constexpr bool IsValid() const {
    return start_delay >= std::chrono::milliseconds(0) &&
           total_duration > std::chrono::milliseconds(0) && drop_ratio > 0.0 &&
           drop_ratio <= 1.0 && burst_duration > std::chrono::milliseconds(0);
  }
};

// Simulates bursty Wi-Fi outages on encoded video RTP streams in wall-clock
// time by alternating random Good intervals (exponentially distributed with
// mean `burst_duration * (1 - drop_ratio) / drop_ratio` seeded by `seed`) with
// Bad `burst_duration` windows that drop all video RTP sends and retransmits,
// while passing audio and RTCP packets through untouched.
class VideoPacketDropper {
 public:
  explicit VideoPacketDropper(PacketDropConfig config);
  VideoPacketDropper(const VideoPacketDropper&) = delete;
  VideoPacketDropper& operator=(const VideoPacketDropper&) = delete;

  bool ShouldDrop(ByteView packet,
                  StreamType stream_type,
                  Clock::time_point now);

 private:
  Clock::duration SampleGoodDuration();

  const PacketDropConfig config_;
  std::mt19937 rng_;
  std::optional<Clock::time_point> active_start_;
  Clock::time_point bad_start_;
  Clock::time_point bad_end_;
};

}  // namespace openscreen::cast

#endif  // CAST_STANDALONE_SENDER_TESTING_VIDEO_PACKET_DROPPER_H_
