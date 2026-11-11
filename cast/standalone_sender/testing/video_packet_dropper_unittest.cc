// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/testing/video_packet_dropper.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

#include "gtest/gtest.h"

namespace openscreen::cast {
namespace {

using std::chrono::milliseconds;

constexpr std::array<uint8_t, 18> kVideoRtpPacket = {0x80, 0x80 | 96};

struct DropperHarness {
  explicit DropperHarness(PacketDropConfig config) : dropper(config) {}

  Clock::time_point At(int64_t ms) const { return t0 + milliseconds(ms); }
  bool DropAt(int64_t ms) {
    return dropper.ShouldDrop(kVideoRtpPacket, StreamType::kVideo, At(ms));
  }

  const Clock::time_point t0 = Clock::time_point{} + milliseconds(10000);
  VideoPacketDropper dropper;
};

TEST(VideoPacketDropperTest, ShouldDropFiltersNonVideoRtpAndMalformedPackets) {
  DropperHarness h({.start_delay = milliseconds(100),
                    .total_duration = milliseconds(600),
                    .drop_ratio = 1.0,
                    .burst_duration = milliseconds(250)});
  const std::array<uint8_t, 28> rtcp_sr = {0x80, 200};
  const std::array<uint8_t, 17> short_pkt = {0x80, 96};
  auto bad_version = kVideoRtpPacket;
  bad_version[0] = 0x40;

  // Non-video and malformed packets at t=0 must not anchor the active window.
  for (const int64_t ms : {0, 260}) {
    EXPECT_FALSE(
        h.dropper.ShouldDrop(kVideoRtpPacket, StreamType::kAudio, h.At(ms)));
    EXPECT_FALSE(h.dropper.ShouldDrop(rtcp_sr, StreamType::kVideo, h.At(ms)));
    EXPECT_FALSE(h.dropper.ShouldDrop(short_pkt, StreamType::kVideo, h.At(ms)));
    EXPECT_FALSE(
        h.dropper.ShouldDrop(bad_version, StreamType::kVideo, h.At(ms)));
  }

  // First valid video RTP packet at t=200ms anchors active_start at t=300ms.
  EXPECT_FALSE(h.DropAt(200));
  EXPECT_FALSE(h.DropAt(299));
  EXPECT_TRUE(h.DropAt(300));
}

TEST(VideoPacketDropperTest,
     PassesAllPacketsBeforeStartDelayAndAfterTotalDuration) {
  DropperHarness h({.start_delay = milliseconds(1000),
                    .total_duration = milliseconds(4000),
                    .drop_ratio = 1.0,
                    .burst_duration = milliseconds(250)});

  EXPECT_FALSE(h.DropAt(0));
  EXPECT_FALSE(h.DropAt(999));
  EXPECT_TRUE(h.DropAt(1000));
  EXPECT_TRUE(h.DropAt(4999));
  EXPECT_FALSE(h.DropAt(5000));
  EXPECT_FALSE(h.DropAt(6400));
}

TEST(VideoPacketDropperTest,
     DropsAllVideoSendsAndRetransmitsDuringBurstWindow) {
  DropperHarness h({.start_delay = milliseconds(0),
                    .total_duration = milliseconds(10000),
                    .drop_ratio = 0.5,
                    .burst_duration = milliseconds(250),
                    .seed = 7});

  std::optional<int64_t> burst_start_ms;
  for (int64_t ms = 0; ms < 5000; ++ms) {
    if (h.DropAt(ms)) {
      burst_start_ms = ms;
      break;
    }
  }
  ASSERT_TRUE(burst_start_ms.has_value());

  for (int64_t offset_ms = 1; offset_ms < 250; offset_ms += 25) {
    EXPECT_TRUE(h.DropAt(*burst_start_ms + offset_ms));
  }
  EXPECT_TRUE(h.DropAt(*burst_start_ms + 249));
  EXPECT_FALSE(h.DropAt(*burst_start_ms + 250));
}

TEST(VideoPacketDropperTest, ReleasesInFlightBurstImmediatelyAtActiveEnd) {
  DropperHarness h({.start_delay = milliseconds(100),
                    .total_duration = milliseconds(200),
                    .drop_ratio = 0.99,
                    .burst_duration = milliseconds(500),
                    .seed = 1});

  EXPECT_FALSE(h.DropAt(0));
  EXPECT_TRUE(h.DropAt(250));
  EXPECT_TRUE(h.DropAt(299));
  // Active window ends at 100 + 200 = 300ms even though the 500ms burst has not
  // finished yet.
  EXPECT_FALSE(h.DropAt(300));
}

TEST(VideoPacketDropperTest,
     MatchesConfiguredDropRatioAndReplaysIdenticalTimelineAcrossPacketRates) {
  const PacketDropConfig config{
      .start_delay = milliseconds(0),
      .total_duration = milliseconds(120000),
      .drop_ratio = 0.2,
      .burst_duration = milliseconds(250),
      .seed = 42,
  };
  DropperHarness sparse(config);
  DropperHarness dense(config);

  int dropped_probes = 0;
  int total_probes = 0;
  for (int64_t ms = 0; ms < 120000; ms += 20) {
    // `sparse` sends 1 packet per step; `dense` sends a 15-packet burst plus
    // asymmetric retransmits in between. Both must see the exact same Good/Bad
    // state.
    const bool sparse_dropped = sparse.DropAt(ms);
    for (int i = 0; i < 15; ++i) {
      EXPECT_EQ(sparse_dropped, dense.DropAt(ms));
    }
    if (sparse_dropped) {
      ++dropped_probes;
      // Inject asymmetric probes at intermediate timestamps; even if `bad_end`
      // falls inside `(ms, ms + 10]`, both harnesses must remain synchronized
      // on every subsequent probe.
      sparse.DropAt(ms + 5);
      for (int rtx = 0; rtx < 4; ++rtx) {
        dense.DropAt(ms + 10);
      }
    }
    ++total_probes;
  }

  const double observed_ratio =
      static_cast<double>(dropped_probes) / static_cast<double>(total_probes);
  EXPECT_GE(observed_ratio, 0.15);
  EXPECT_LE(observed_ratio, 0.25);
}

}  // namespace
}  // namespace openscreen::cast
