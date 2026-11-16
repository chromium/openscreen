// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/channel/message_util.h"

#include <ranges>
#include <string>
#include <utility>

#include "cast/sender/channel/cast_auth_util.h"
#include "util/json/json_serialization.h"
#include "util/json/json_value.h"
#include "util/string_util.h"

namespace openscreen::cast {

using proto::AuthChallenge;
using proto::CastMessage;
using proto::DeviceAuthMessage;

bool IsValidAppId(std::string_view app_id) {
  constexpr size_t kAppIdLength = 8;
  return app_id.size() == kAppIdLength &&
         std::ranges::all_of(app_id, ascii_ishex<char>);
}

ErrorOr<CastMessage> CreateReceiverMessage(std::string_view sender_id,
                                           const json::Value& dict) {
  CastMessage message;
  message.set_payload_type(proto::CastMessage_PayloadType_STRING);
  ErrorOr<std::string> serialized = json::Stringify(dict);
  if (serialized.is_error()) {
    return serialized.error();
  }
  message.set_payload_utf8(serialized.value());

  message.set_protocol_version(proto::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_source_id(sender_id);
  message.set_destination_id(kPlatformReceiverId);
  message.set_namespace_(kReceiverNamespace);

  return message;
}

CastMessage CreateAuthChallengeMessage(const AuthContext& auth_context) {
  CastMessage message;
  DeviceAuthMessage auth_message;

  AuthChallenge* challenge = auth_message.mutable_challenge();
  challenge->set_sender_nonce(auth_context.nonce());
  challenge->set_hash_algorithm(proto::SHA256);

  std::string auth_message_string;
  auth_message.SerializeToString(&auth_message_string);

  message.set_protocol_version(CastMessage::CASTV2_1_0);
  message.set_source_id(kPlatformSenderId);
  message.set_destination_id(kPlatformReceiverId);
  message.set_namespace_(kAuthNamespace);
  message.set_payload_type(proto::CastMessage_PayloadType_BINARY);
  message.set_payload_binary(auth_message_string);

  return message;
}

ErrorOr<CastMessage> CreateAppAvailabilityRequest(std::string_view sender_id,
                                                  int request_id,
                                                  std::string_view app_id) {
  if (!IsValidAppId(app_id)) {
    return Error(Error::Code::kParameterInvalid, "Invalid app ID");
  }

  json::Value dict(json::ValueType::objectValue);
  dict[kMessageKeyType] = json::Value(
      CastMessageTypeToString(CastMessageType::kGetAppAvailability));
  json::Value app_id_value(json::ValueType::arrayValue);
  app_id_value.append(json::Value(app_id));
  dict[kMessageKeyAppId] = std::move(app_id_value);
  dict[kMessageKeyRequestId] = json::Value(request_id);

  return CreateReceiverMessage(sender_id, dict);
}

ErrorOr<CastMessage> CreateLaunchRequest(std::string_view sender_id,
                                         int request_id,
                                         std::string_view app_id,
                                         json::Value app_params) {
  if (!IsValidAppId(app_id)) {
    return Error(Error::Code::kParameterInvalid, "Invalid app ID");
  }

  json::Value dict(json::ValueType::objectValue);
  dict[kMessageKeyType] =
      json::Value(CastMessageTypeToString(CastMessageType::kLaunch));
  dict[kMessageKeyAppId] = json::Value(app_id);
  dict[kMessageKeyRequestId] = json::Value(request_id);
  if (!app_params.isNull()) {
    dict[kMessageKeyAppParams] = std::move(app_params);
  }

  return CreateReceiverMessage(sender_id, dict);
}

ErrorOr<CastMessage> CreateStopRequest(std::string_view sender_id,
                                       int request_id,
                                       std::string_view session_id) {
  json::Value dict(json::ValueType::objectValue);
  dict[kMessageKeyType] =
      json::Value(CastMessageTypeToString(CastMessageType::kStop));
  dict[kMessageKeySessionId] = json::Value(session_id);
  dict[kMessageKeyRequestId] = json::Value(request_id);

  return CreateReceiverMessage(sender_id, dict);
}
}  // namespace openscreen::cast
