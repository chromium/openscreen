// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_JSON_JSON_SERIALIZATION_SERDE_INTERNAL_H_
#define UTIL_JSON_JSON_SERIALIZATION_SERDE_INTERNAL_H_

#if !defined(USE_JSON_SERDE)
#error "USE_JSON_SERDE not defined for json_serialization_serde_internal.h"
#endif

#include <cstdint>
#include <string>
#include <vector>

#include "third_party/rust/chromium_crates_io/vendor/cxx-v1/include/cxx.h"
#include "util/json/json_value.h"

namespace openscreen::json::serde {

struct SerdeJsonWriter;

// Builder used by Serde deserializer visitor to construct json::Value AST.
class ValueBuilder {
 public:
  ValueBuilder();
  ~ValueBuilder();

  void set_null();
  void set_bool(bool val);
  void set_i64(int64_t val);
  void set_u64(uint64_t val);
  void set_f64(double val);
  void set_string(::rust::Str val);

  void begin_array();
  void end_array();

  void begin_object();
  void set_object_key(::rust::Str key);
  void end_object();

  Value TakeValue();

 private:
  struct StackEntry {
    enum class Type { kArray, kObject };
    Type type;
    Value* container;
    std::string pending_key;
  };

  Value* GetNextTarget();

  Value root_;
  std::vector<StackEntry> stack_;
};

// Visitor used by Serde serializer to traverse json::Value AST.
class ValueVisitor {
 public:
  explicit ValueVisitor(const Value& root);
  ~ValueVisitor();

  bool visit_node(SerdeJsonWriter& writer);

 private:
  static bool VisitValue(const Value& val, SerdeJsonWriter& writer);

  const Value& root_;
};

}  // namespace openscreen::json::serde

#endif  // UTIL_JSON_JSON_SERIALIZATION_SERDE_INTERNAL_H_
