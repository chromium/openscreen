// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_CHANNEL_MESSAGE_UTIL_H_
#define CAST_SENDER_CHANNEL_MESSAGE_UTIL_H_

#include <string_view>

#include "cast/common/channel/message_util.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "platform/base/error.h"

namespace openscreen::json {
class Value;
}

namespace openscreen::cast {

class AuthContext;

// Returns true if `app_id` is a valid Cast application ID, i.e. an 8-digit
// hexadecimal string.
bool IsValidAppId(std::string_view app_id);

proto::CastMessage CreateAuthChallengeMessage(const AuthContext& auth_context);

// `request_id` must be unique for `sender_id`. Returns an error if `app_id`
// is not a valid app ID.
ErrorOr<proto::CastMessage> CreateAppAvailabilityRequest(
    std::string_view sender_id,
    int request_id,
    std::string_view app_id);

// `request_id` must be unique for `sender_id`. `app_params` may be null if the
// application does not require any launch parameters. Returns an error if
// `app_id` is not a valid app ID.
ErrorOr<proto::CastMessage> CreateLaunchRequest(std::string_view sender_id,
                                                int request_id,
                                                std::string_view app_id,
                                                json::Value app_params);

// `request_id` must be unique for `sender_id`.
ErrorOr<proto::CastMessage> CreateStopRequest(std::string_view sender_id,
                                              int request_id,
                                              std::string_view session_id);

}  // namespace openscreen::cast

#endif  // CAST_SENDER_CHANNEL_MESSAGE_UTIL_H_
