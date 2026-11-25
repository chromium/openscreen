// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_MESSAGE_CHUNKER_H_
#define CAST_COMMON_CHANNEL_MESSAGE_CHUNKER_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "cast/common/channel/proto/cast_channel.pb.h"
#include "platform/base/byte_span.h"
#include "platform/base/error.h"

namespace openscreen::cast {

// Maximum size of the binary payload in a single CastMessage chunk.
// This is chosen to be well within the 64 KiB limit of a single CastMessage.
constexpr size_t kMaxChunkPayloadSize = 64000;

// Returns a ByteView pointing to the payload data of the given message (either
// binary or utf8). If neither is set, an empty ByteView is returned.
ByteView GetPayloadView(const proto::CastMessage& message);

// Splits a large CastMessage into multiple smaller CastMessage chunks if
// necessary. If the message's payload is within kMaxChunkPayloadSize, it
// returns a single chunk (the original message). Returns an error if the
// message exceeds the maximum size and the protocol version does not support
// chunking.
ErrorOr<std::vector<proto::CastMessage>> ChunkMessage(
    const proto::CastMessage& message,
    proto::CastMessage_ProtocolVersion protocol_version);

// Reassembles incoming CastMessage chunks.
class MessageReassembler {
 public:
  MessageReassembler();
  ~MessageReassembler();
  MessageReassembler(const MessageReassembler&) = delete;
  MessageReassembler& operator=(const MessageReassembler&) = delete;
  MessageReassembler(MessageReassembler&&) noexcept;
  MessageReassembler& operator=(MessageReassembler&&) noexcept;

  // Processes an incoming CastMessage.
  // If the message is not chunked, it is returned as a complete message.
  // If it is an intermediate chunk, it is stored and std::nullopt is
  // returned until all chunks are received.
  // When all chunks are received, the complete reassembled message is returned.
  // Returns an error if the chunk sequence is invalid or corrupted.
  ErrorOr<std::optional<proto::CastMessage>> ProcessMessage(
      int socket_id,
      proto::CastMessage message);

  // Removes all incomplete messages associated with the given socket.
  void DropMessagesForSocket(int socket_id);

 private:
  struct IncompleteMessage {
    proto::CastMessage_PayloadType payload_type =
        proto::CastMessage_PayloadType_STRING;
    std::string source_id;
    std::string destination_id;
    std::string namespace_;
    std::vector<uint8_t> data;
    uint32_t expected_remaining_length = 0;
  };

  // Unique identifier for a message stream: (socket_id, source_id,
  // destination_id, namespace).
  struct MessageKey {
    int socket_id = 0;
    std::string source_id;
    std::string destination_id;
    std::string namespace_;

    bool operator<(const MessageKey& other) const;
  };

  std::map<MessageKey, IncompleteMessage> incomplete_messages_;
};

}  // namespace openscreen::cast

#endif  // CAST_COMMON_CHANNEL_MESSAGE_CHUNKER_H_
