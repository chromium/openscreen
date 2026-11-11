// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_SENDER_TESTING_PACKET_DROP_ENVIRONMENT_H_
#define CAST_STANDALONE_SENDER_TESTING_PACKET_DROP_ENVIRONMENT_H_

#include "cast/standalone_sender/testing/video_packet_dropper.h"
#include "cast/streaming/public/environment.h"

namespace openscreen::cast {

// An Environment that drops outgoing video RTP packets as decided by a
// `VideoPacketDropper`, to exercise receiver recovery from packet loss.
class PacketDropEnvironment final : public Environment {
 public:
  PacketDropEnvironment(ClockNowFunctionPtr now_function,
                        TaskRunner& task_runner,
                        const IPEndpoint& local_endpoint,
                        PacketDropConfig drop_config);
  ~PacketDropEnvironment() final;

  void SendPacket(ByteView packet, PacketMetadata metadata) final;

 private:
  VideoPacketDropper dropper_;
};

}  // namespace openscreen::cast

#endif  // CAST_STANDALONE_SENDER_TESTING_PACKET_DROP_ENVIRONMENT_H_
