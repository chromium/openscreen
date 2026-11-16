// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_JSON_JSON_SERIALIZATION_H_
#define UTIL_JSON_JSON_SERIALIZATION_H_

#include <string>
#include <string_view>

#include "platform/base/error.h"
#include "util/json/json_value.h"

namespace openscreen::json {

// Parses a JSON document into a json::Value AST.
ErrorOr<Value> Parse(std::string_view value);

// Serializes a json::Value AST into a JSON string.
ErrorOr<std::string> Stringify(const Value& value);

}  // namespace openscreen::json

#endif  // UTIL_JSON_JSON_SERIALIZATION_H_
