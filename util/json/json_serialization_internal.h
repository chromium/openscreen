// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_JSON_JSON_SERIALIZATION_INTERNAL_H_
#define UTIL_JSON_JSON_SERIALIZATION_INTERNAL_H_

#include <string>
#include <string_view>

#include "platform/base/error.h"
#include "util/json/json_value.h"

namespace openscreen::json::internal {

// The implementations behind json::Parse() and json::Stringify().
enum class JsonBackend {
  kJsoncpp,
  // Only available when Open Screen is built with `use_json_serde = true`.
  kSerde,
};

// Returns the backend used by json::Parse() and json::Stringify(), based on
// RuntimeOptions::use_rust_json and the backends built into this binary.
JsonBackend GetDefaultJsonBackend();

// Same as json::Parse() and json::Stringify(), but use `backend`. If `backend`
// is not available in this build, the jsoncpp backend is used.
ErrorOr<Value> Parse(std::string_view document, JsonBackend backend);
ErrorOr<std::string> Stringify(const Value& value, JsonBackend backend);

// Backend implementations.
ErrorOr<Value> ParseWithJsoncpp(std::string_view document);
ErrorOr<std::string> StringifyWithJsoncpp(const Value& value);

#if defined(USE_JSON_SERDE)
ErrorOr<Value> ParseWithSerde(std::string_view document);
ErrorOr<std::string> StringifyWithSerde(const Value& value);
#endif  // defined(USE_JSON_SERDE)

}  // namespace openscreen::json::internal

#endif  // UTIL_JSON_JSON_SERIALIZATION_INTERNAL_H_
