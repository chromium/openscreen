// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_IMPL_RECEIVER_PACKET_ROUTER_H_
#define CAST_STREAMING_IMPL_RECEIVER_PACKET_ROUTER_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "cast/streaming/impl/rtp_defines.h"
#include "cast/streaming/public/environment.h"
#include "cast/streaming/ssrc.h"
#include "platform/base/byte_span.h"
#include "util/flat_map.h"
#include "util/raw_ptr.h"
#include "util/raw_ref.h"

namespace openscreen::cast {

class Receiver;

// Handles all network I/O among multiple Receivers meant for synchronized
// play-out (e.g., one Receiver for audio, one Receiver for video). Incoming
// traffic is dispatched to the appropriate Receiver, based on its corresponding
// sender's SSRC. Also, all traffic not coming from the same source is
// filtered-out.
class ReceiverPacketRouter final : public Environment::PacketConsumer {
 public:
  class PacketConsumer {
   public:
    virtual void OnReceivedRtpPacket(Clock::time_point arrival_time,
                                     std::vector<uint8_t> packet) = 0;
    virtual void OnReceivedRtcpPacket(Clock::time_point arrival_time,
                                      std::span<const uint8_t> packet) = 0;

   protected:
    virtual ~PacketConsumer() = default;
  };

  explicit ReceiverPacketRouter(Environment& environment);
  ~ReceiverPacketRouter() final;

  // Called from a PacketConsumer constructor/destructor to register/deregister
  // a PacketConsumer instance that processes RTP/RTCP packets from a Sender
  // having the given SSRC.
  void RegisterPacketConsumer(Ssrc sender_ssrc, PacketConsumer* consumer);
  void DeregisterPacketConsumer(Ssrc sender_ssrc);

  // Called by a PacketConsumer to send a RTCP packet back to the source from
  // which earlier packets were received, or does nothing if OnReceivedPacket()
  // has not been called yet.
  void SendRtcpPacket(ByteView packet);

 private:
  // Environment::PacketConsumer implementation.
  void OnReceivedPacket(const IPEndpoint& source,
                        Clock::time_point arrival_time,
                        std::vector<uint8_t> packet) final;

  const raw_ref<Environment> environment_;

  FlatMap<Ssrc, raw_ptr<PacketConsumer>> receivers_;

  // The parser used to route packets. Picked once, so that it does not change
  // during a session.
  const RtpParserVersion rtp_parser_version_ = GetDefaultRtpParserVersion();
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_IMPL_RECEIVER_PACKET_ROUTER_H_
