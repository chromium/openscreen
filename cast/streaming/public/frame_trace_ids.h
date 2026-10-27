// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_PUBLIC_FRAME_TRACE_IDS_H_
#define CAST_STREAMING_PUBLIC_FRAME_TRACE_IDS_H_

#include <cstdint>

#include "cast/streaming/public/frame_id.h"
#include "cast/streaming/rtp_time.h"
#include "cast/streaming/ssrc.h"
#include "platform/base/trace_logging_types.h"

namespace openscreen::cast {

// Helpers for computing the flow IDs used by frame lifecycle trace events
// (TRACE_FLOW_*). See cast/docs/frame_flow_tracing.md.
//
// Tracing backends such as Perfetto treat flow IDs as global to the whole
// trace, but the natural per-frame identifiers in Cast Streaming are not
// globally unique:
//   * Every stream's RTP timestamps start at zero, so audio and video frames
//     (and the frames of concurrent sessions) collide.
//   * Every stream's FrameIds start at FrameId::first(), so FrameIds collide
//     across streams, and also with RTP timestamps of the same value.
//
// These helpers namespace both identifiers by the stream's sender SSRC and by
// identifier kind, so that each frame of each stream gets a distinct flow ID.
// The sender and receiver of a stream both know the sender SSRC
// (SessionConfig::sender_ssrc), so RTP-keyed flow IDs computed on either side
// of the network match, allowing sender and receiver traces to be joined.
//
// Bit layout of the returned TraceId:
//   [63]     Identifier kind: 0 = RTP timestamp, 1 = FrameId.
//   [62]     Always one, so a flow ID can never equal kEmptyTraceId (zero).
//   [61]     Always zero, so a flow ID can never equal kUnsetTraceId.
//   [60:32]  Lower 29 bits of the sender SSRC. Open Screen's generated SSRCs
//            fit in 17 bits (see ssrc.h), so this only truncates SSRCs from
//            other implementations.
//   [31:0]   Lower 32 bits of the RTP timestamp or FrameId.
//
// Only the lower 32 bits of the RTP timestamp are used, which matches what is
// sent on the wire, so these IDs repeat after the RTP timestamp wraps (roughly
// 13 hours for 90 kHz video). That is acceptable for trace correlation.

namespace internal {

enum class FrameTraceIdKind : uint64_t {
  kRtpTimestamp = 0,
  kFrameId = 1,
};

inline constexpr int kFrameTraceIdKindShift = 63;
inline constexpr uint64_t kFrameTraceIdMarkerBit = uint64_t{1} << 62;
inline constexpr int kFrameTraceIdSsrcShift = 32;
inline constexpr uint64_t kFrameTraceIdSsrcMask = (uint64_t{1} << 29) - 1;
inline constexpr uint64_t kFrameTraceIdValueMask = 0xFFFFFFFF;

constexpr TraceId MakeFrameTraceId(FrameTraceIdKind kind,
                                   Ssrc sender_ssrc,
                                   int64_t value) {
  return (static_cast<uint64_t>(kind) << kFrameTraceIdKindShift) |
         kFrameTraceIdMarkerBit |
         ((uint64_t{sender_ssrc} & kFrameTraceIdSsrcMask)
          << kFrameTraceIdSsrcShift) |
         (static_cast<uint64_t>(value) & kFrameTraceIdValueMask);
}

}  // namespace internal

// Returns the flow ID for trace events keyed by a frame's RTP timestamp. Used
// by sender-side events, and by the receiver-side "Frame.Complete.Rtp" event
// that bridges the receiver's flow to the sender's flow.
constexpr TraceId GetRtpTimestampFlowId(Ssrc sender_ssrc,
                                        RtpTimeTicks rtp_timestamp) {
  return internal::MakeFrameTraceId(internal::FrameTraceIdKind::kRtpTimestamp,
                                    sender_ssrc, rtp_timestamp.value());
}

// Returns the flow ID for trace events keyed by a frame's FrameId. Used by
// receiver-side events (e.g. "Frame.Complete", "Frame.Consumed", decode and
// render events, and "Frame.PlayedOut").
constexpr TraceId GetFrameIdFlowId(Ssrc sender_ssrc, FrameId frame_id) {
  return internal::MakeFrameTraceId(internal::FrameTraceIdKind::kFrameId,
                                    sender_ssrc, frame_id.value());
}

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_PUBLIC_FRAME_TRACE_IDS_H_
