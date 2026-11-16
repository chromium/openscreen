// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/testing/test_helpers.h"

#include <optional>
#include <string_view>
#include <utility>

#include "cast/common/channel/message_util.h"
#include "cast/receiver/channel/message_util.h"
#include "cast/sender/channel/message_util.h"
#include "gtest/gtest.h"
#include "util/json/json_serialization.h"
#include "util/json/json_value.h"
#include "util/osp_logging.h"

namespace openscreen::cast {

using proto::CastMessage;

void VerifyAppAvailabilityRequest(const CastMessage& message,
                                  const std::string& expected_app_id,
                                  int* request_id_out,
                                  std::string* sender_id_out) {
  std::string app_id_out;
  VerifyAppAvailabilityRequest(message, &app_id_out, request_id_out,
                               sender_id_out);
  EXPECT_EQ(app_id_out, expected_app_id);
}

void VerifyAppAvailabilityRequest(const CastMessage& message,
                                  std::string* app_id_out,
                                  int* request_id_out,
                                  std::string* sender_id_out) {
  EXPECT_EQ(message.namespace_(), kReceiverNamespace);
  EXPECT_EQ(message.destination_id(), kPlatformReceiverId);
  EXPECT_EQ(message.payload_type(), proto::CastMessage_PayloadType_STRING);
  EXPECT_NE(message.source_id(), kPlatformSenderId);
  *sender_id_out = message.source_id();

  ErrorOr<json::Value> maybe_value = json::Parse(message.payload_utf8());
  ASSERT_TRUE(maybe_value);
  json::Value& value = maybe_value.value();

  std::optional<std::string_view> maybe_type =
      MaybeGetString(value, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyType));
  ASSERT_TRUE(maybe_type);
  EXPECT_EQ(maybe_type.value(),
            CastMessageTypeToString(CastMessageType::kGetAppAvailability));

  std::optional<int> maybe_id =
      MaybeGetInt(value, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyRequestId));
  ASSERT_TRUE(maybe_id);
  *request_id_out = maybe_id.value();

  const json::Value* maybe_app_ids =
      value.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyAppId));
  ASSERT_TRUE(maybe_app_ids);
  ASSERT_TRUE(maybe_app_ids->isArray());
  ASSERT_EQ(maybe_app_ids->size(), 1u);
  json::Value app_id_value = maybe_app_ids->get(0u, json::Value(""));
  std::optional<std::string_view> maybe_app_id = MaybeGetString(app_id_value);
  ASSERT_TRUE(maybe_app_id);
  *app_id_out =
      std::string(maybe_app_id.value().begin(), maybe_app_id.value().end());
}

CastMessage CreateAppAvailableResponseChecked(int request_id,
                                              const std::string& sender_id,
                                              const std::string& app_id) {
  ErrorOr<CastMessage> message =
      CreateAppAvailableResponse(request_id, sender_id, app_id);
  OSP_CHECK(message);
  return std::move(message.value());
}

CastMessage CreateAppUnavailableResponseChecked(int request_id,
                                                const std::string& sender_id,
                                                const std::string& app_id) {
  ErrorOr<CastMessage> message =
      CreateAppUnavailableResponse(request_id, sender_id, app_id);
  OSP_CHECK(message);
  return std::move(message.value());
}

void VerifyConnectMessage(const CastMessage& message) {
  std::string sender_id;
  VerifyConnectMessage(message, &sender_id);
}

void VerifyConnectMessage(const CastMessage& message,
                          std::string* sender_id_out) {
  EXPECT_EQ(message.namespace_(), kConnectionNamespace);
  EXPECT_EQ(message.destination_id(), kPlatformReceiverId);
  EXPECT_EQ(message.payload_type(), proto::CastMessage_PayloadType_STRING);
  EXPECT_NE(message.source_id(), kPlatformSenderId);
  *sender_id_out = message.source_id();

  ErrorOr<json::Value> maybe_value = json::Parse(message.payload_utf8());
  ASSERT_TRUE(maybe_value);
  json::Value& value = maybe_value.value();

  std::optional<std::string_view> maybe_type =
      MaybeGetString(value, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyType));
  ASSERT_TRUE(maybe_type);
  EXPECT_EQ(maybe_type.value(),
            CastMessageTypeToString(CastMessageType::kConnect));
}

CastMessage CreateConnectedResponse(const std::string& sender_id) {
  json::Value dict(json::ValueType::objectValue);
  dict[kMessageKeyType] = kMessageTypeConnected;
  dict[kMessageKeyProtocolVersion] =
      static_cast<int>(proto::CastMessage_ProtocolVersion_CASTV2_1_0);

  CastMessage message;
  message.set_source_id(kPlatformReceiverId);
  message.set_destination_id(sender_id);
  message.set_namespace_(kConnectionNamespace);
  message.set_protocol_version(proto::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_payload_utf8(json::Stringify(dict).value());
  message.set_payload_type(proto::CastMessage_PayloadType_STRING);
  return message;
}

void VerifyLaunchRequest(const CastMessage& message,
                         const std::string& expected_app_id,
                         int* request_id_out,
                         std::string* sender_id_out) {
  EXPECT_EQ(message.namespace_(), kReceiverNamespace);
  EXPECT_EQ(message.destination_id(), kPlatformReceiverId);
  EXPECT_EQ(message.payload_type(), proto::CastMessage_PayloadType_STRING);
  EXPECT_NE(message.source_id(), kPlatformSenderId);
  *sender_id_out = message.source_id();

  ErrorOr<json::Value> maybe_value = json::Parse(message.payload_utf8());
  ASSERT_TRUE(maybe_value);
  json::Value& value = maybe_value.value();

  std::optional<std::string_view> maybe_type =
      MaybeGetString(value, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyType));
  ASSERT_TRUE(maybe_type);
  EXPECT_EQ(maybe_type.value(),
            CastMessageTypeToString(CastMessageType::kLaunch));

  std::optional<int> maybe_id =
      MaybeGetInt(value, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyRequestId));
  ASSERT_TRUE(maybe_id);
  *request_id_out = maybe_id.value();

  std::optional<std::string_view> maybe_app_id =
      MaybeGetString(value, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyAppId));
  ASSERT_TRUE(maybe_app_id);
  EXPECT_EQ(maybe_app_id.value(), expected_app_id);
}

void VerifyStopRequest(const CastMessage& message,
                       const std::string& expected_session_id,
                       int* request_id_out,
                       std::string* sender_id_out) {
  EXPECT_EQ(message.namespace_(), kReceiverNamespace);
  EXPECT_EQ(message.destination_id(), kPlatformReceiverId);
  EXPECT_EQ(message.payload_type(), proto::CastMessage_PayloadType_STRING);
  EXPECT_NE(message.source_id(), kPlatformSenderId);
  *sender_id_out = message.source_id();

  ErrorOr<json::Value> maybe_value = json::Parse(message.payload_utf8());
  ASSERT_TRUE(maybe_value);
  json::Value& value = maybe_value.value();

  std::optional<std::string_view> maybe_type =
      MaybeGetString(value, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyType));
  ASSERT_TRUE(maybe_type);
  EXPECT_EQ(maybe_type.value(),
            CastMessageTypeToString(CastMessageType::kStop));

  std::optional<int> maybe_id =
      MaybeGetInt(value, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyRequestId));
  ASSERT_TRUE(maybe_id);
  *request_id_out = maybe_id.value();

  std::optional<std::string_view> maybe_session_id = MaybeGetString(
      value, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeySessionId));
  ASSERT_TRUE(maybe_session_id);
  EXPECT_EQ(maybe_session_id.value(), expected_session_id);
}

namespace {

// Builds a receiver->sender response the way receiver sends it, without
// depending on receiver code from this sender-side test helper. For LAUNCH and
// STOP rejections this also matches cast/receiver/application_agent.cc's
// HandleLaunch()/HandleStop()
// Unlike the other response types, `dict` must already contain whichever
// id field(s) apply -- callers must not assume `requestId` is always one of
// them (see CreateLaunchStatusResponse()).
CastMessage MakeReceiverNamespaceResponse(const std::string& sender_id,
                                          json::Value dict) {
  CastMessage message;
  message.set_source_id(kPlatformReceiverId);
  message.set_destination_id(sender_id);
  message.set_namespace_(kReceiverNamespace);
  message.set_protocol_version(proto::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_payload_utf8(json::Stringify(dict).value());
  message.set_payload_type(proto::CastMessage_PayloadType_STRING);
  return message;
}

}  // namespace

CastMessage CreateLaunchStatusResponse(int request_id,
                                       const std::string& sender_id) {
  // A successful launch response echoes the original request id as
  // `launchRequestId`, not `requestId` -- there is no top-level `requestId`
  // field on this particular response.
  json::Value dict(json::ValueType::objectValue);
  dict[kMessageKeyType] =
      CastMessageTypeToString(CastMessageType::kLaunchStatus);
  dict[kMessageKeyLaunchRequestId] = request_id;
  dict[kMessageKeyStatus] = kMessageValueUserAllowed;
  return MakeReceiverNamespaceResponse(sender_id, std::move(dict));
}

CastMessage CreateLaunchPendingUserAuthResponse(int request_id,
                                                const std::string& sender_id) {
  json::Value dict(json::ValueType::objectValue);
  dict[kMessageKeyType] =
      CastMessageTypeToString(CastMessageType::kLaunchStatus);
  dict[kMessageKeyLaunchRequestId] = request_id;
  dict[kMessageKeyStatus] = kMessageValueUserPendingAuthorization;
  return MakeReceiverNamespaceResponse(sender_id, std::move(dict));
}

CastMessage CreateLaunchErrorResponse(int request_id,
                                      const std::string& sender_id,
                                      const std::string& reason) {
  json::Value dict(json::ValueType::objectValue);
  dict[kMessageKeyType] =
      CastMessageTypeToString(CastMessageType::kLaunchError);
  dict[kMessageKeyRequestId] = request_id;
  dict[kMessageKeyReason] = reason;
  return MakeReceiverNamespaceResponse(sender_id, std::move(dict));
}

CastMessage CreateLaunchExtendedErrorResponse(
    int request_id,
    const std::string& sender_id,
    const std::string& extended_error) {
  json::Value dict(json::ValueType::objectValue);
  dict[kMessageKeyType] =
      CastMessageTypeToString(CastMessageType::kLaunchError);
  dict[kMessageKeyRequestId] = request_id;
  dict[kMessageKeyExtendedError] = extended_error;
  return MakeReceiverNamespaceResponse(sender_id, std::move(dict));
}

CastMessage CreateStopErrorResponse(int request_id,
                                    const std::string& sender_id,
                                    const std::string& reason) {
  json::Value dict(json::ValueType::objectValue);
  dict[kMessageKeyType] =
      CastMessageTypeToString(CastMessageType::kInvalidRequest);
  dict[kMessageKeyRequestId] = request_id;
  dict[kMessageKeyReason] = reason;
  return MakeReceiverNamespaceResponse(sender_id, std::move(dict));
}

CastMessage CreateReceiverStatusResponse(
    int request_id,
    const std::string& sender_id,
    const std::string& running_session_id) {
  json::Value dict(json::ValueType::objectValue);
  dict[kMessageKeyType] =
      CastMessageTypeToString(CastMessageType::kReceiverStatus);
  dict[kMessageKeyRequestId] = request_id;
  json::Value status(json::ValueType::objectValue);
  json::Value applications(json::ValueType::arrayValue);
  if (!running_session_id.empty()) {
    // Mirrors the fields ApplicationAgent::PopulateReceiverStatus() sends for
    // a running application.
    json::Value app(json::ValueType::objectValue);
    app[kMessageKeySessionId] = running_session_id;
    app[kMessageKeyAppId] = "AAAAAAAA";
    app[kMessageKeyTransportId] = running_session_id + "-transport";
    app[kMessageKeyDisplayName] = "Test App";
    applications.append(std::move(app));
  }
  status[kMessageKeyApplications] = std::move(applications);
  dict[kMessageKeyStatus] = std::move(status);
  return MakeReceiverNamespaceResponse(sender_id, std::move(dict));
}

}  // namespace openscreen::cast
