// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_JSON_JSON_VALUE_H_
#define UTIL_JSON_JSON_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "json/value.h"  // Temporary shim for embedder (Chromium) compatibility

#define JSON_EXPAND_FIND_CONSTANT_ARGS(s) (s), ((s) + sizeof(s) - 1)

namespace openscreen::json {

enum class ValueType {
  kNull = 0,
  kInt,
  kUInt,
  kReal,
  kString,
  kBoolean,
  kArray,
  kObject,

  nullValue = kNull,
  intValue = kInt,
  uintValue = kUInt,
  realValue = kReal,
  stringValue = kString,
  booleanValue = kBoolean,
  arrayValue = kArray,
  objectValue = kObject,
};

inline constexpr ValueType arrayValue = ValueType::kArray;
inline constexpr ValueType objectValue = ValueType::kObject;

using ArrayIndex = uint32_t;
using Int = int32_t;
using UInt = uint32_t;
using Int64 = int64_t;
using UInt64 = uint64_t;

// A JSON value. The API mirrors the parts of JsonCpp's Json::Value that Open
// Screen uses, so code can migrate by changing the type name. Known
// differences from Json::Value:
//
//  * Arrays are backed by std::vector. Growing an array (through append(),
//    resize(), or the non-const operator[] with an index past the end) can
//    reallocate it, which invalidates references, pointers, and iterators to
//    its elements. For example, `a[5] = a[0]` is not safe when it grows `a`.
//    `a.append(a[0])` is safe, because append() takes its argument by value.
//  * Iterating an object is not supported. Use getMemberNames() instead.
//
// Like JsonCpp, the asX() accessors abort when the value cannot be converted,
// for example asUInt() on -1 or asInt() on a string. For untrusted input, check
// the matching isX() first, or use the GetIfX() accessors, which return
// std::nullopt instead of aborting.
class Value {
 public:
  // NOTE: std::map (in Object) and the variant below are instantiated while
  // Value is still incomplete. The standard only guarantees this works for
  // std::vector, std::list, and std::forward_list, but every standard library
  // that Open Screen supports (libc++, libstdc++) accepts it.
  using Array = std::vector<Value>;
  using Object = std::map<std::string, Value, std::less<>>;
  using ArrayIndex = uint32_t;
  using Int = int32_t;
  using UInt = uint32_t;
  using Int64 = int64_t;
  using UInt64 = uint64_t;

  using iterator = Array::iterator;
  using const_iterator = Array::const_iterator;

  // The single-argument constructors are implicit on purpose, to match
  // Json::Value.
  Value();
  Value(ValueType type);          // NOLINT(runtime/explicit)
  Value(::Json::ValueType type);  // NOLINT(runtime/explicit)
  Value(std::nullptr_t);          // NOLINT(runtime/explicit)
  Value(bool val);                // NOLINT(runtime/explicit)
  Value(int val);                 // NOLINT(runtime/explicit)
  Value(unsigned int val);        // NOLINT(runtime/explicit)
  Value(long val);                // NOLINT
  Value(unsigned long val);       // NOLINT
  Value(long long val);           // NOLINT
  Value(unsigned long long val);  // NOLINT
  Value(double val);              // NOLINT(runtime/explicit)
  Value(const char* val);         // NOLINT(runtime/explicit)
  Value(const char* begin, const char* end);
  Value(std::string val);       // NOLINT(runtime/explicit)
  Value(std::string_view val);  // NOLINT(runtime/explicit)
  Value(Array val);             // NOLINT(runtime/explicit)
  Value(Object val);            // NOLINT(runtime/explicit)

  // Temporary conversion shims for JsonCpp / Chromium compatibility. Both
  // conversions copy the whole tree.
  Value(const ::Json::Value& other);  // NOLINT(runtime/explicit)
  operator ::Json::Value() const;

  // Compares against a Json::Value without converting it. These only accept
  // an exact Json::Value, so comparisons such as `value == 1` or
  // `value["type"] == "OFFER"` are not ambiguous.
  template <typename T,
            typename = std::enable_if_t<std::is_same_v<T, ::Json::Value>>>
  bool operator==(const T& other) const {
    return EqualsJsonCpp(other);
  }
  template <typename T,
            typename = std::enable_if_t<std::is_same_v<T, ::Json::Value>>>
  bool operator!=(const T& other) const {
    return !EqualsJsonCpp(other);
  }
  template <typename T,
            typename = std::enable_if_t<std::is_same_v<T, ::Json::Value>>>
  friend bool operator==(const T& lhs, const Value& rhs) {
    return rhs.EqualsJsonCpp(lhs);
  }
  template <typename T,
            typename = std::enable_if_t<std::is_same_v<T, ::Json::Value>>>
  friend bool operator!=(const T& lhs, const Value& rhs) {
    return !rhs.EqualsJsonCpp(lhs);
  }

  // Comparison with primitives:
  bool operator==(const char* other) const {
    return isString() && asString() == other;
  }
  bool operator==(std::string_view other) const {
    return isString() && asString() == other;
  }
  bool operator==(int other) const { return isInt() && asInt() == other; }
  bool operator==(int64_t other) const {
    return isInt64() && asInt64() == other;
  }
  bool operator==(uint32_t other) const {
    return isUInt() && asUInt() == other;
  }
  bool operator==(uint64_t other) const {
    return isUInt64() && asUInt64() == other;
  }
  bool operator==(double other) const {
    return isDouble() && asDouble() == other;
  }
  bool operator==(bool other) const { return isBool() && asBool() == other; }

  Value(const Value& other);
  Value(Value&& other) noexcept;
  // Safe when `other` is owned by this value, for example `v = v["key"]`.
  Value& operator=(const Value& other);
  Value& operator=(Value&& other) noexcept;
  ~Value();

  ValueType type() const;

  bool isNull() const;
  bool isBool() const;
  bool isInt() const;
  bool isUInt() const;
  bool isDouble() const;
  bool isString() const;
  bool isArray() const;
  bool isObject() const;

  // These match Json::Value: they convert between compatible types (for
  // example asInt() on true returns 1, and asString() on 1.5 returns "1.5"),
  // and they abort when the value is out of range or has an incompatible type.
  bool asBool() const;
  int asInt() const;
  int64_t asInt64() const;
  unsigned int asUInt() const;
  uint64_t asUInt64() const;
  double asDouble() const;
  std::string asString() const;
  bool getString(const char** begin, const char** end) const;

  // Array / Object indexing. The non-const overloads turn a null value into an
  // array or object, insert missing elements, and abort on any other type. The
  // const overloads never insert, and return a null value when the element is
  // missing.
  Value& operator[](ArrayIndex index);
  const Value& operator[](ArrayIndex index) const;
  Value& operator[](int index);
  const Value& operator[](int index) const;
  Value& operator[](std::string_view key);
  const Value& operator[](std::string_view key) const;
  Value& operator[](const std::string& key);
  const Value& operator[](const std::string& key) const;
  Value& operator[](const char* key);
  const Value& operator[](const char* key) const;

  // These return a copy of the element. Prefer find() or the const operator[]
  // when a reference is enough.
  Value get(std::string_view key, const Value& default_value) const;
  Value get(ArrayIndex index, const Value& default_value) const;

  bool isMember(std::string_view key) const;
  const Value* find(const char* begin, const char* end) const;
  const Value* find(std::string_view key) const;
  std::vector<std::string> getMemberNames() const;

  ArrayIndex size() const;
  bool empty() const;
  // Abort unless the value is null, an array, or an object (clear()), or null
  // or an array (resize()), like Json::Value.
  void clear();
  void resize(ArrayIndex new_size);
  Value& append(Value value);

  // Iterate over the elements of an array. Null and scalar values are empty
  // ranges. Iterating an object aborts.
  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator end() const;

  static const Value& nullSingleton();

  bool operator!() const { return isNull(); }
  explicit operator bool() const { return !isNull(); }
  bool operator==(const Value& other) const;
  bool operator!=(const Value& other) const;

  friend std::ostream& operator<<(std::ostream& os, const Value& val);

 private:
  bool isInt64() const;
  bool isUInt64() const;
  bool isNumeric() const;
  bool EqualsJsonCpp(const ::Json::Value& other) const;

  using Storage = std::variant<std::monostate,
                               int64_t,
                               uint64_t,
                               double,
                               std::string,
                               bool,
                               Array,
                               Object>;
  Storage storage_;
};

}  // namespace openscreen::json

namespace openscreen {

std::optional<int> MaybeGetInt(const json::Value& message,
                               const char* first,
                               const char* last);
std::optional<int> MaybeGetInt(const ::Json::Value& message,
                               const char* first,
                               const char* last);

std::optional<std::string_view> MaybeGetString(const json::Value& message);
std::optional<std::string_view> MaybeGetString(const ::Json::Value& message);

std::optional<std::string_view> MaybeGetString(const json::Value& message,
                                               const char* first,
                                               const char* last);
std::optional<std::string_view> MaybeGetString(const ::Json::Value& message,
                                               const char* first,
                                               const char* last);

}  // namespace openscreen

#endif  // UTIL_JSON_JSON_VALUE_H_
