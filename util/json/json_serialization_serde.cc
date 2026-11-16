// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(USE_JSON_SERDE)
#error "USE_JSON_SERDE not defined for json_serialization_serde.cc"
#endif

#include <utility>

#include "util/json/json_serialization_internal.h"
#include "util/json/json_serialization_serde_internal.h"
#include "util/json/serde_json.rs.h"

namespace openscreen::json {

namespace internal {

ErrorOr<Value> ParseWithSerde(std::string_view document) {
  if (document.empty()) {
    return ErrorOr<Value>(Error::Code::kJsonParseError, "empty document");
  }

  serde::ValueBuilder builder;
  ::rust::String error_msg;
  ::rust::Slice<const uint8_t> slice(
      reinterpret_cast<const uint8_t*>(document.data()), document.size());

  if (!serde::decode_json_to_builder(slice, builder, error_msg)) {
    return ErrorOr<Value>(Error::Code::kJsonParseError, std::string(error_msg));
  }

  return builder.TakeValue();
}

ErrorOr<std::string> StringifyWithSerde(const Value& value) {
  serde::ValueVisitor visitor(value);
  ::rust::String output;
  ::rust::String error_msg;
#if defined(_DEBUG)
  constexpr bool kPretty = true;
#else
  constexpr bool kPretty = false;
#endif

  if (!serde::encode_json(visitor, kPretty, output, error_msg)) {
    return ErrorOr<std::string>(Error::Code::kJsonWriteError,
                                error_msg.empty() ? "JSON serialization failed"
                                                  : std::string(error_msg));
  }

  return std::string(output);
}

}  // namespace internal

namespace serde {

ValueBuilder::ValueBuilder() = default;
ValueBuilder::~ValueBuilder() = default;

Value* ValueBuilder::GetNextTarget() {
  if (stack_.empty()) {
    return &root_;
  }
  auto& top = stack_.back();
  if (top.type == StackEntry::Type::kArray) {
    return &top.container->append(Value());
  } else {
    // kObject
    auto* target = &((*top.container)[top.pending_key]);
    top.pending_key.clear();
    return target;
  }
}

void ValueBuilder::set_null() {
  *GetNextTarget() = Value();
}

void ValueBuilder::set_bool(bool val) {
  *GetNextTarget() = Value(val);
}

void ValueBuilder::set_i64(int64_t val) {
  *GetNextTarget() = Value(val);
}

void ValueBuilder::set_u64(uint64_t val) {
  *GetNextTarget() = Value(val);
}

void ValueBuilder::set_f64(double val) {
  *GetNextTarget() = Value(val);
}

void ValueBuilder::set_string(::rust::Str val) {
  *GetNextTarget() = Value(val.data(), val.data() + val.size());
}

void ValueBuilder::begin_array() {
  Value* target = GetNextTarget();
  *target = Value(ValueType::kArray);
  stack_.push_back(StackEntry{StackEntry::Type::kArray, target, ""});
}

void ValueBuilder::end_array() {
  if (!stack_.empty() && stack_.back().type == StackEntry::Type::kArray) {
    stack_.pop_back();
  }
}

void ValueBuilder::begin_object() {
  Value* target = GetNextTarget();
  *target = Value(ValueType::kObject);
  stack_.push_back(StackEntry{StackEntry::Type::kObject, target, ""});
}

void ValueBuilder::set_object_key(::rust::Str key) {
  if (!stack_.empty() && stack_.back().type == StackEntry::Type::kObject) {
    stack_.back().pending_key = std::string(key.data(), key.size());
  }
}

void ValueBuilder::end_object() {
  if (!stack_.empty() && stack_.back().type == StackEntry::Type::kObject) {
    stack_.pop_back();
  }
}

Value ValueBuilder::TakeValue() {
  return std::move(root_);
}

ValueVisitor::ValueVisitor(const Value& root) : root_(root) {}
ValueVisitor::~ValueVisitor() = default;

bool ValueVisitor::visit_node(SerdeJsonWriter& writer) {
  return VisitValue(root_, writer);
}

bool ValueVisitor::VisitValue(const Value& val, SerdeJsonWriter& writer) {
  switch (val.type()) {
    case ValueType::kNull:
      write_null(writer);
      return true;
    case ValueType::kInt:
      write_i64(writer, val.asInt64());
      return true;
    case ValueType::kUInt:
      write_u64(writer, val.asUInt64());
      return true;
    case ValueType::kReal:
      write_f64(writer, val.asDouble());
      return true;
    case ValueType::kString: {
      const char* begin = nullptr;
      const char* end = nullptr;
      val.getString(&begin, &end);
      write_str(writer, ::rust::Str(begin, end - begin));
      return true;
    }
    case ValueType::kBoolean:
      write_bool(writer, val.asBool());
      return true;
    case ValueType::kArray: {
      write_array_start(writer);
      for (Value::ArrayIndex i = 0; i < val.size(); ++i) {
        if (!VisitValue(val[i], writer)) {
          return false;
        }
      }
      write_array_end(writer);
      return true;
    }
    case ValueType::kObject: {
      write_object_start(writer);
      const auto member_names = val.getMemberNames();
      for (const auto& name : member_names) {
        write_object_key(writer, ::rust::Str(name.data(), name.size()));
        if (!VisitValue(val[name], writer)) {
          return false;
        }
      }
      write_object_end(writer);
      return true;
    }
  }
  return false;
}

}  // namespace serde
}  // namespace openscreen::json
