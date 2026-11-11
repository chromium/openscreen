// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/testing/packet_drop_environment.h"

namespace openscreen::cast {

PacketDropEnvironment::PacketDropEnvironment(ClockNowFunctionPtr now_function,
                                             TaskRunner& task_runner,
                                             const IPEndpoint& local_endpoint,
                                             PacketDropConfig drop_config)
    : Environment(now_function, task_runner, local_endpoint),
      dropper_(drop_config) {}

PacketDropEnvironment::~PacketDropEnvironment() = default;

void PacketDropEnvironment::SendPacket(ByteView packet,
                                       PacketMetadata metadata) {
  if (!dropper_.ShouldDrop(packet, metadata.stream_type, now())) {
    Environment::SendPacket(packet, metadata);
  }
}

}  // namespace openscreen::cast
