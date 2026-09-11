// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "discovery/mdns/public/mdns_wire.rs.h"
#include "gtest/gtest.h"

namespace openscreen::discovery {

TEST(MdnsWireTest, ParseSimpleQuery) {
  // Standard mDNS query for "foo.local" (A IN, unicast query), ID = 0x1234.
  const uint8_t kQueryPacket[] = {
      0x12, 0x34,                      // ID = 0x1234
      0x00, 0x00,                      // Flags = standard query
      0x00, 0x01,                      // QDCOUNT = 1
      0x00, 0x00,                      // ANCOUNT = 0
      0x00, 0x00,                      // NSCOUNT = 0
      0x00, 0x00,                      // ARCOUNT = 0
      0x03, 'f',  'o', 'o',            // Label: foo
      0x05, 'l',  'o', 'c', 'a', 'l',  // Label: local
      0x00,                            // Null terminator
      0x00, 0x01,                      // QTYPE = A (1)
      0x00, 0x01,                      // QCLASS = IN (1)
  };

  DnsHeader header;
  EXPECT_TRUE(parse_header(
      rust::Slice<const uint8_t>(kQueryPacket, sizeof(kQueryPacket)), header));
  EXPECT_EQ(header.id, 0x1234);
  EXPECT_FALSE(header.is_response);
  EXPECT_FALSE(header.is_truncated);
  EXPECT_EQ(header.question_count, 1);
  EXPECT_EQ(header.answer_count, 0);

  DnsMessage message;
  EXPECT_TRUE(parse_message(
      rust::Slice<const uint8_t>(kQueryPacket, sizeof(kQueryPacket)), message));
  EXPECT_EQ(message.header.id, 0x1234);
  EXPECT_FALSE(message.header.is_response);
  ASSERT_EQ(message.questions.size(), 1u);
  EXPECT_EQ(message.questions[0].qtype, 1);
  EXPECT_EQ(message.questions[0].qclass, 1);
  ASSERT_EQ(message.questions[0].name_labels.size(), 2u);
  EXPECT_EQ(std::string(message.questions[0].name_labels[0]), "foo");
  EXPECT_EQ(std::string(message.questions[0].name_labels[1]), "local");
}

TEST(MdnsWireTest, ParseResponseWithARecord) {
  // Response packet: ID = 0x0000, Flags = 0x8400 (Response, Auth), 1 answer
  // "host.local" A 192.168.1.1, TTL 120.
  const uint8_t kResponsePacket[] = {
      0x00, 0x00,                        // ID
      0x84, 0x00,                        // Flags: Response, AA
      0x00, 0x00,                        // QDCOUNT = 0
      0x00, 0x01,                        // ANCOUNT = 1
      0x00, 0x00,                        // NSCOUNT = 0
      0x00, 0x00,                        // ARCOUNT = 0
      0x04, 'h',  'o',  's',  't',       // host
      0x05, 'l',  'o',  'c',  'a', 'l',  // local
      0x00,                              // Null terminator
      0x00, 0x01,                        // TYPE = A (1)
      0x80, 0x01,              // CLASS = IN (1) + flush cache bit (0x8000)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120s
      0x00, 0x04,              // RDLENGTH = 4
      192,  168,  1,    1,     // RDATA = 192.168.1.1
  };

  DnsHeader header;
  EXPECT_TRUE(parse_header(
      rust::Slice<const uint8_t>(kResponsePacket, sizeof(kResponsePacket)),
      header));
  EXPECT_TRUE(header.is_response);
  EXPECT_TRUE(header.is_authoritative);
  EXPECT_EQ(header.answer_count, 1);

  DnsMessage message;
  EXPECT_TRUE(parse_message(
      rust::Slice<const uint8_t>(kResponsePacket, sizeof(kResponsePacket)),
      message));
  EXPECT_TRUE(message.header.is_response);
  ASSERT_EQ(message.answers.size(), 1u);
  EXPECT_EQ(message.answers[0].rtype, 1);
  EXPECT_EQ(message.answers[0].rclass, 1);
  EXPECT_TRUE(message.answers[0].is_cache_flush);
  EXPECT_EQ(message.answers[0].ttl_seconds, 120u);
  ASSERT_EQ(message.answers[0].rdata_bytes.size(), 4u);
  EXPECT_EQ(message.answers[0].rdata_bytes[0], 192);
  EXPECT_EQ(message.answers[0].rdata_bytes[1], 168);
  EXPECT_EQ(message.answers[0].rdata_bytes[2], 1);
  EXPECT_EQ(message.answers[0].rdata_bytes[3], 1);
}

TEST(MdnsWireTest, InvalidPacketFailsGracefully) {
  const uint8_t kTruncated[] = {0x12, 0x34, 0x00};
  DnsHeader header;
  EXPECT_FALSE(parse_header(
      rust::Slice<const uint8_t>(kTruncated, sizeof(kTruncated)), header));

  DnsMessage message;
  EXPECT_FALSE(parse_message(
      rust::Slice<const uint8_t>(kTruncated, sizeof(kTruncated)), message));
}

}  // namespace openscreen::discovery
