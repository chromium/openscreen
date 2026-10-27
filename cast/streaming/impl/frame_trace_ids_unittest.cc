// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/public/frame_trace_ids.h"

#include <cstdint>
#include <limits>
#include <set>

#include "gtest/gtest.h"

namespace openscreen::cast {
namespace {

constexpr Ssrc kAudioSsrc = 1;
constexpr Ssrc kVideoSsrc = 50001;

TEST(FrameTraceIdsTest, RtpTimestampFlowIdsAreDistinctAcrossStreams) {
  // Audio and video RTP timestamps both start at zero, but should still be distinct due to
  // having different ssrcs.
  EXPECT_NE(GetRtpTimestampFlowId(kAudioSsrc, RtpTimeTicks()),
            GetRtpTimestampFlowId(kVideoSsrc, RtpTimeTicks()));
  EXPECT_NE(GetRtpTimestampFlowId(kAudioSsrc, RtpTimeTicks(3000)),
            GetRtpTimestampFlowId(kVideoSsrc, RtpTimeTicks(3000)));
}

TEST(FrameTraceIdsTest, FrameIdFlowIdsAreDistinctAcrossStreams) {
  EXPECT_NE(GetFrameIdFlowId(kAudioSsrc, FrameId::first()),
            GetFrameIdFlowId(kVideoSsrc, FrameId::first()));
  EXPECT_NE(GetFrameIdFlowId(kAudioSsrc, FrameId::first() + 7),
            GetFrameIdFlowId(kVideoSsrc, FrameId::first() + 7));
}

TEST(FrameTraceIdsTest, RtpAndFrameIdFlowIdsNeverCollide) {
  // An RTP timestamp and a FrameId with the same numeric value, on the same
  // stream, must still produce different flow IDs.
  for (int64_t value : {0, 1, 42, 90000}) {
    EXPECT_NE(GetRtpTimestampFlowId(kVideoSsrc, RtpTimeTicks(value)),
              GetFrameIdFlowId(kVideoSsrc, FrameId(value)))
        << "value=" << value;
  }
}

TEST(FrameTraceIdsTest, FlowIdsAreUniqueWithinAStream) {
  std::set<TraceId> ids;
  for (int i = 0; i < 1000; ++i) {
    EXPECT_TRUE(
        ids.insert(GetRtpTimestampFlowId(kVideoSsrc, RtpTimeTicks(i * 3000)))
            .second);
    EXPECT_TRUE(
        ids.insert(GetFrameIdFlowId(kVideoSsrc, FrameId::first() + i)).second);
  }
}

TEST(FrameTraceIdsTest, FlowIdsAreNeverReservedValues) {
  for (Ssrc ssrc : {Ssrc{0}, kAudioSsrc, kVideoSsrc, Ssrc{0xFFFFFFFF}}) {
    for (int64_t value : {int64_t{0}, int64_t{-1}, int64_t{0xFFFFFFFF},
                          std::numeric_limits<int64_t>::max()}) {
      for (TraceId id : {GetRtpTimestampFlowId(ssrc, RtpTimeTicks(value)),
                         GetFrameIdFlowId(ssrc, FrameId(value))}) {
        EXPECT_NE(id, kEmptyTraceId);
        EXPECT_NE(id, kUnsetTraceId);
      }
    }
  }
}

TEST(FrameTraceIdsTest, UsesLower32BitsOfRtpTimestamp) {
  // Only the lower 32 bits of the RTP timestamp are sent on the wire, and the
  // Receiver may expand them to 64 bits with different upper bits than the
  // Sender used. The flow ID must only depend on the lower 32 bits so that the
  // Sender's and Receiver's flow IDs match.
  constexpr int64_t kWrapped = int64_t{1} << 32;
  EXPECT_EQ(GetRtpTimestampFlowId(kVideoSsrc, RtpTimeTicks(5)),
            GetRtpTimestampFlowId(kVideoSsrc, RtpTimeTicks(kWrapped + 5)));
}

}  // namespace
}  // namespace openscreen::cast
