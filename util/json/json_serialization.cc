// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/json/json_serialization.h"

#include "platform/api/runtime_options.h"
#include "util/json/json_serialization_internal.h"

namespace openscreen::json {

namespace internal {

JsonBackend GetDefaultJsonBackend() {
#if defined(USE_JSON_SERDE)
  if (GetRuntimeOptions().use_rust_json) {
    return JsonBackend::kSerde;
  }
#endif  // defined(USE_JSON_SERDE)
  return JsonBackend::kJsoncpp;
}

ErrorOr<Value> Parse(std::string_view document, JsonBackend backend) {
#if defined(USE_JSON_SERDE)
  if (backend == JsonBackend::kSerde) {
    return ParseWithSerde(document);
  }
#endif  // defined(USE_JSON_SERDE)
  return ParseWithJsoncpp(document);
}

ErrorOr<std::string> Stringify(const Value& value, JsonBackend backend) {
#if defined(USE_JSON_SERDE)
  if (backend == JsonBackend::kSerde) {
    return StringifyWithSerde(value);
  }
#endif  // defined(USE_JSON_SERDE)
  return StringifyWithJsoncpp(value);
}

}  // namespace internal

ErrorOr<Value> Parse(std::string_view document) {
  return internal::Parse(document, internal::GetDefaultJsonBackend());
}

ErrorOr<std::string> Stringify(const Value& value) {
  return internal::Stringify(value, internal::GetDefaultJsonBackend());
}

}  // namespace openscreen::json
