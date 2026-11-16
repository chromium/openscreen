// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/channel/message_util.h"

#include <string>

#include "cast/sender/testing/test_helpers.h"
#include "gtest/gtest.h"
#include "platform/base/error.h"
#include "util/json/json_value.h"

namespace openscreen::cast {

namespace {

constexpr char kValidAppId[] = "CC1AD845";
constexpr char kSenderId[] = "sender-17";

}  // namespace

TEST(IsValidAppIdTest, AcceptsValidHexIds) {
  EXPECT_TRUE(IsValidAppId("CC1AD845"));
  EXPECT_TRUE(IsValidAppId("233637DE"));
  EXPECT_TRUE(IsValidAppId("00000000"));
  EXPECT_TRUE(IsValidAppId("abcdef01"));
}

TEST(IsValidAppIdTest, RejectsWrongLength) {
  EXPECT_FALSE(IsValidAppId(""));
  EXPECT_FALSE(IsValidAppId("CC1AD84"));    // 7 characters.
  EXPECT_FALSE(IsValidAppId("CC1AD8450"));  // 9 characters.
}

TEST(IsValidAppIdTest, RejectsNonHexCharacters) {
  EXPECT_FALSE(IsValidAppId("CC1AD84G"));  // 'G' is not a hex digit.
  EXPECT_FALSE(IsValidAppId("CC1AD84 "));  // Trailing space.
  EXPECT_FALSE(IsValidAppId("CC1AD-45"));  // '-' is not a hex digit.
}

TEST(CreateAppAvailabilityRequestTest, ValidAppIdSucceeds) {
  ErrorOr<proto::CastMessage> message =
      CreateAppAvailabilityRequest(kSenderId, 1, kValidAppId);
  ASSERT_TRUE(message.is_value());

  int request_id;
  std::string sender_id;
  VerifyAppAvailabilityRequest(message.value(), kValidAppId, &request_id,
                               &sender_id);
  EXPECT_EQ(request_id, 1);
}

TEST(CreateAppAvailabilityRequestTest, InvalidAppIdFails) {
  ErrorOr<proto::CastMessage> message =
      CreateAppAvailabilityRequest(kSenderId, 1, "not-hex!");
  ASSERT_TRUE(message.is_error());
  EXPECT_EQ(message.error().code(), Error::Code::kParameterInvalid);
}

TEST(CreateLaunchRequestTest, ValidAppIdSucceeds) {
  ErrorOr<proto::CastMessage> message =
      CreateLaunchRequest(kSenderId, 1, kValidAppId, json::Value());
  ASSERT_TRUE(message.is_value());

  int request_id;
  std::string sender_id;
  VerifyLaunchRequest(message.value(), kValidAppId, &request_id, &sender_id);
  EXPECT_EQ(request_id, 1);
}

TEST(CreateLaunchRequestTest, InvalidAppIdFails) {
  ErrorOr<proto::CastMessage> message =
      CreateLaunchRequest(kSenderId, 1, "short", json::Value());
  ASSERT_TRUE(message.is_error());
  EXPECT_EQ(message.error().code(), Error::Code::kParameterInvalid);
}

}  // namespace openscreen::cast
