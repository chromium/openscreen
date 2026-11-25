// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/dummy_player.h"

#include <chrono>
#include <string>

#include "cast/streaming/public/encoded_frame.h"
#include "platform/base/byte_span.h"
#include "platform/base/trivial_clock_traits.h"
#include "util/chrono_helpers.h"
#include "util/osp_logging.h"

namespace openscreen::cast {

constexpr size_t kSyntheticFrameWidth = 64;
constexpr size_t kSyntheticFrameHeight = 48;
constexpr size_t kSyntheticFrameSizeBytes =
    kSyntheticFrameWidth * kSyntheticFrameHeight;

using clock_operators::operator<<;

DummyPlayer::DummyPlayer(Receiver& receiver) : receiver_(receiver) {
  receiver_->SetConsumer(this);
}

DummyPlayer::~DummyPlayer() {
  receiver_->SetConsumer(nullptr);
}

void DummyPlayer::OnFramesReady(size_t buffer_size) {
  // Consume the next frame.
  buffer_.resize(buffer_size);
  const EncodedFrame frame = receiver_->ConsumeNextFrame(buffer_);
  receiver_->ReportPlayoutEvent(frame.frame_id, frame.rtp_timestamp,
                                Clock::now());

  // Convert the RTP timestamp to a human-readable timestamp (in µs) and log
  // some short information about the frame.
  const auto media_timestamp =
      frame.rtp_timestamp.ToTimeSinceOrigin<microseconds>(
          receiver_->config().rtp_timebase);
  OSP_LOG_INFO << "[SSRC " << receiver_->config().receiver_ssrc << "] "
               << (frame.dependency == EncodedFrame::Dependency::kKeyFrame
                       ? "KEY "
                       : "")
               << frame.frame_id << " at " << media_timestamp << ", "
               << buffer_size << " bytes";

  // If synthetic 64x48 frame received on keyframe, render ASCII art animation
  if (buffer_size == kSyntheticFrameSizeBytes &&
      frame.dependency == EncodedFrame::Dependency::kKeyFrame) {
    static constexpr char kRamp[] = " .:-=+*#%@";
    static constexpr int kRampLen = sizeof(kRamp) - 1;

    const std::string border =
        "+" + std::string(kSyntheticFrameWidth, '-') + "+";
    const size_t expected_size = (border.size() + 2) +
                                 kSyntheticFrameHeight * (border.size() + 1) +
                                 border.size();

    std::string ascii_art;
    ascii_art.reserve(expected_size);
    ascii_art += "\n" + border + "\n";
    for (size_t y = 0; y < kSyntheticFrameHeight; ++y) {
      ascii_art += '|';
      for (size_t x = 0; x < kSyntheticFrameWidth; ++x) {
        uint8_t pixel = buffer_[y * kSyntheticFrameWidth + x];
        ascii_art += kRamp[pixel * (kRampLen - 1) / 255];
      }
      ascii_art += "|\n";
    }
    ascii_art += border;
    OSP_LOG_INFO << ascii_art;
  }
}

}  // namespace openscreen::cast
