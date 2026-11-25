// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/json/json_value.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>
#include <utility>

#include "platform/api/logging.h"
#include "platform/base/error.h"
#include "util/json/json_serialization.h"
#include "util/no_destructor.h"
#include "util/osp_logging.h"

namespace openscreen::json {

// Value(::Json::ValueType) relies on the two enums having the same values.
static_assert(static_cast<int>(ValueType::kNull) == ::Json::nullValue);
static_assert(static_cast<int>(ValueType::kInt) == ::Json::intValue);
static_assert(static_cast<int>(ValueType::kUInt) == ::Json::uintValue);
static_assert(static_cast<int>(ValueType::kReal) == ::Json::realValue);
static_assert(static_cast<int>(ValueType::kString) == ::Json::stringValue);
static_assert(static_cast<int>(ValueType::kBoolean) == ::Json::booleanValue);
static_assert(static_cast<int>(ValueType::kArray) == ::Json::arrayValue);
static_assert(static_cast<int>(ValueType::kObject) == ::Json::objectValue);

namespace {

// 2^63 and 2^64. These are exact as doubles, unlike INT64_MAX and UINT64_MAX,
// which round up to these values.
constexpr double kTwoTo63 = 9223372036854775808.0;
constexpr double kTwoTo64 = 18446744073709551616.0;

const Value::Array& EmptyArray() {
  static const NoDestructor<Value::Array> kEmptyArray;
  return *kEmptyArray;
}

// Aborts like JsonCpp does when a value cannot be converted.
[[noreturn]] void FailConversion(const char* message) {
  OSP_LOG_FATAL << message;
  Break();
}

// Formats a double the same way as Json::Value::asString().
std::string DoubleToString(double value) {
  if (std::isnan(value)) {
    return "null";
  }
  if (std::isinf(value)) {
    return value < 0 ? "-1e+9999" : "1e+9999";
  }

  // "%.17g" needs at most 24 characters, for example
  // "-1.2345678901234567e-308".
  char buffer[32];
  const int length = std::snprintf(buffer, sizeof(buffer), "%.17g", value);
  OSP_CHECK_GT(length, 0);
  OSP_CHECK_LT(static_cast<size_t>(length), sizeof(buffer));
  std::string result(buffer, static_cast<size_t>(length));

  // snprintf uses the decimal separator of the current C locale.
  std::replace(result.begin(), result.end(), ',', '.');

  // Keep a marker that this was a double, like JsonCpp.
  if (result.find_first_of(".e") == std::string::npos) {
    result += ".0";
  }
  return result;
}

}  // namespace

Value::Value() : storage_(std::monostate{}) {}

Value::Value(ValueType type) {
  switch (type) {
    case ValueType::kNull:
      storage_ = std::monostate{};
      break;
    case ValueType::kInt:
      storage_ = int64_t{0};
      break;
    case ValueType::kUInt:
      storage_ = uint64_t{0};
      break;
    case ValueType::kReal:
      storage_ = double{0.0};
      break;
    case ValueType::kString:
      storage_ = std::string{};
      break;
    case ValueType::kBoolean:
      storage_ = bool{false};
      break;
    case ValueType::kArray:
      storage_ = Array{};
      break;
    case ValueType::kObject:
      storage_ = Object{};
      break;
  }
}

Value::Value(::Json::ValueType type) : Value(static_cast<ValueType>(type)) {}

Value::Value(std::nullptr_t) : storage_(std::monostate{}) {}

Value::Value(bool val) : storage_(val) {}

Value::Value(int val) : storage_(static_cast<int64_t>(val)) {}

Value::Value(unsigned int val) : storage_(static_cast<uint64_t>(val)) {}

Value::Value(long val) : storage_(static_cast<int64_t>(val)) {}  // NOLINT

Value::Value(unsigned long val)  // NOLINT(runtime/int)
    : storage_(static_cast<uint64_t>(val)) {}

Value::Value(long long val)  // NOLINT(runtime/int)
    : storage_(static_cast<int64_t>(val)) {}

Value::Value(unsigned long long val)  // NOLINT(runtime/int)
    : storage_(static_cast<uint64_t>(val)) {}

Value::Value(double val) : storage_(val) {}

Value::Value(const char* val)
    : storage_(val ? std::string(val) : std::string()) {}

Value::Value(const char* begin, const char* end)
    : storage_(begin && end >= begin ? std::string(begin, end - begin)
                                     : std::string()) {}

Value::Value(std::string val) : storage_(std::move(val)) {}

Value::Value(std::string_view val) : storage_(std::string(val)) {}

Value::Value(Array val) : storage_(std::move(val)) {}

Value::Value(Object val) : storage_(std::move(val)) {}

Value::Value(const Value& other) = default;
Value::Value(Value&& other) noexcept = default;

// `other` may be owned by this value, for example in `v = v["key"]`. Assigning
// it to `storage_` directly would destroy `other` before (or while) it is
// read, so copy or move it out first, like JsonCpp does.
Value& Value::operator=(const Value& other) {
  Storage copy(other.storage_);
  storage_.swap(copy);
  return *this;
}

Value& Value::operator=(Value&& other) noexcept {
  Storage moved(std::move(other.storage_));
  storage_.swap(moved);
  return *this;
}

Value::~Value() = default;

Value::Value(const ::Json::Value& other) {
  switch (other.type()) {
    case ::Json::nullValue:
      storage_ = std::monostate{};
      break;
    case ::Json::intValue:
      storage_ = static_cast<int64_t>(other.asInt64());
      break;
    case ::Json::uintValue:
      storage_ = static_cast<uint64_t>(other.asUInt64());
      break;
    case ::Json::realValue:
      storage_ = other.asDouble();
      break;
    case ::Json::stringValue: {
      const char* begin = nullptr;
      const char* end = nullptr;
      other.getString(&begin, &end);
      storage_ = (begin && end >= begin) ? std::string(begin, end - begin)
                                         : other.asString();
      break;
    }
    case ::Json::booleanValue:
      storage_ = other.asBool();
      break;
    case ::Json::arrayValue: {
      // Place each element by its index, so that missing elements of a sparse
      // JsonCpp array (older JsonCpp versions allow them) become null.
      Array arr(other.size());
      for (auto it = other.begin(); it != other.end(); ++it) {
        const ::Json::ArrayIndex index = it.index();
        OSP_CHECK_LT(index, arr.size());
        arr[index] = Value(*it);
      }
      storage_ = std::move(arr);
      break;
    }
    case ::Json::objectValue: {
      Object obj;
      for (auto it = other.begin(); it != other.end(); ++it) {
        const char* end = nullptr;
        const char* begin = it.memberName(&end);
        std::string key = (begin && end >= begin)
                              ? std::string(begin, end - begin)
                              : it.name();
        obj.emplace_hint(obj.end(), std::move(key), Value(*it));
      }
      storage_ = std::move(obj);
      break;
    }
  }
}

Value::operator ::Json::Value() const {
  switch (type()) {
    case ValueType::kNull:
      return ::Json::Value(::Json::nullValue);
    case ValueType::kInt:
      return ::Json::Value(
          static_cast<::Json::Value::Int64>(std::get<int64_t>(storage_)));
    case ValueType::kUInt:
      return ::Json::Value(
          static_cast<::Json::Value::UInt64>(std::get<uint64_t>(storage_)));
    case ValueType::kReal:
      return ::Json::Value(std::get<double>(storage_));
    case ValueType::kString: {
      const auto& s = std::get<std::string>(storage_);
      return ::Json::Value(s.data(), s.data() + s.size());
    }
    case ValueType::kBoolean:
      return ::Json::Value(std::get<bool>(storage_));
    case ValueType::kArray: {
      ::Json::Value arr(::Json::arrayValue);
      const auto& vec = std::get<Array>(storage_);
      for (const auto& item : vec) {
        arr.append(static_cast<::Json::Value>(item));
      }
      return arr;
    }
    case ValueType::kObject: {
      ::Json::Value obj(::Json::objectValue);
      const auto& map = std::get<Object>(storage_);
      for (const auto& [k, v] : map) {
        obj[k] = static_cast<::Json::Value>(v);
      }
      return obj;
    }
  }
  return ::Json::Value(::Json::nullValue);
}

bool Value::EqualsJsonCpp(const ::Json::Value& other) const {
  // This must give the same answer as `*this == Value(other)`, without
  // building the copy.
  switch (other.type()) {
    case ::Json::nullValue:
      return isNull();
    case ::Json::intValue: {
      const int64_t* val = std::get_if<int64_t>(&storage_);
      return val && *val == other.asInt64();
    }
    case ::Json::uintValue: {
      const uint64_t* val = std::get_if<uint64_t>(&storage_);
      return val && *val == other.asUInt64();
    }
    case ::Json::realValue: {
      const double* val = std::get_if<double>(&storage_);
      return val && *val == other.asDouble();
    }
    case ::Json::stringValue: {
      const std::string* val = std::get_if<std::string>(&storage_);
      if (!val) {
        return false;
      }
      const char* begin = nullptr;
      const char* end = nullptr;
      other.getString(&begin, &end);
      const std::string_view other_string =
          (begin && end >= begin) ? std::string_view(begin, end - begin)
                                  : std::string_view();
      return *val == other_string;
    }
    case ::Json::booleanValue: {
      const bool* val = std::get_if<bool>(&storage_);
      return val && *val == other.asBool();
    }
    case ::Json::arrayValue: {
      const Array* arr = std::get_if<Array>(&storage_);
      if (!arr || arr->size() != other.size()) {
        return false;
      }
      for (::Json::ArrayIndex i = 0; i < other.size(); ++i) {
        // The const Json::Value::operator[] returns null for missing elements
        // of a sparse array, which matches the conversion above.
        if (!(*arr)[i].EqualsJsonCpp(other[i])) {
          return false;
        }
      }
      return true;
    }
    case ::Json::objectValue: {
      const Object* obj = std::get_if<Object>(&storage_);
      if (!obj || obj->size() != other.size()) {
        return false;
      }
      for (auto it = other.begin(); it != other.end(); ++it) {
        const char* end = nullptr;
        const char* begin = it.memberName(&end);
        if (!begin || end < begin) {
          return false;
        }
        const auto found = obj->find(std::string_view(begin, end - begin));
        if (found == obj->end() || !found->second.EqualsJsonCpp(*it)) {
          return false;
        }
      }
      return true;
    }
  }
  return false;
}

ValueType Value::type() const {
  switch (storage_.index()) {
    case 0:
      return ValueType::kNull;
    case 1:
      return ValueType::kInt;
    case 2:
      return ValueType::kUInt;
    case 3:
      return ValueType::kReal;
    case 4:
      return ValueType::kString;
    case 5:
      return ValueType::kBoolean;
    case 6:
      return ValueType::kArray;
    case 7:
      return ValueType::kObject;
    default:
      OSP_NOTREACHED();
  }
}

bool Value::isNull() const {
  return std::holds_alternative<std::monostate>(storage_);
}

bool Value::isBool() const {
  return std::holds_alternative<bool>(storage_);
}

bool Value::isInt() const {
  if (std::holds_alternative<int64_t>(storage_)) {
    const int64_t val = std::get<int64_t>(storage_);
    return val >= INT32_MIN && val <= INT32_MAX;
  }
  if (std::holds_alternative<uint64_t>(storage_)) {
    return std::get<uint64_t>(storage_) <= static_cast<uint64_t>(INT32_MAX);
  }
  if (std::holds_alternative<double>(storage_)) {
    const double val = std::get<double>(storage_);
    return std::isfinite(val) && std::trunc(val) == val &&
           val >= static_cast<double>(INT32_MIN) &&
           val <= static_cast<double>(INT32_MAX);
  }
  return false;
}

bool Value::isInt64() const {
  if (std::holds_alternative<int64_t>(storage_)) {
    return true;
  }
  if (std::holds_alternative<uint64_t>(storage_)) {
    return std::get<uint64_t>(storage_) <= static_cast<uint64_t>(INT64_MAX);
  }
  if (std::holds_alternative<double>(storage_)) {
    // Like JsonCpp, exclude -2^63 itself: nearby doubles also round to it, so
    // the original value cannot be known.
    const double val = std::get<double>(storage_);
    return std::isfinite(val) && std::trunc(val) == val && val > -kTwoTo63 &&
           val < kTwoTo63;
  }
  return false;
}

bool Value::isUInt() const {
  if (std::holds_alternative<uint64_t>(storage_)) {
    return std::get<uint64_t>(storage_) <= UINT32_MAX;
  }
  if (std::holds_alternative<int64_t>(storage_)) {
    const int64_t val = std::get<int64_t>(storage_);
    return val >= 0 && val <= UINT32_MAX;
  }
  if (std::holds_alternative<double>(storage_)) {
    const double val = std::get<double>(storage_);
    return std::isfinite(val) && std::trunc(val) == val && val >= 0.0 &&
           val <= static_cast<double>(UINT32_MAX);
  }
  return false;
}

bool Value::isUInt64() const {
  if (std::holds_alternative<uint64_t>(storage_)) {
    return true;
  }
  if (std::holds_alternative<int64_t>(storage_)) {
    return std::get<int64_t>(storage_) >= 0;
  }
  if (std::holds_alternative<double>(storage_)) {
    const double val = std::get<double>(storage_);
    return std::isfinite(val) && std::trunc(val) == val && val >= 0.0 &&
           val < kTwoTo64;
  }
  return false;
}

bool Value::isDouble() const {
  return isNumeric();
}

bool Value::isNumeric() const {
  return std::holds_alternative<int64_t>(storage_) ||
         std::holds_alternative<uint64_t>(storage_) ||
         std::holds_alternative<double>(storage_);
}

bool Value::isString() const {
  return std::holds_alternative<std::string>(storage_);
}

bool Value::isArray() const {
  return std::holds_alternative<Array>(storage_);
}

bool Value::isObject() const {
  return std::holds_alternative<Object>(storage_);
}

bool Value::asBool() const {
  switch (type()) {
    case ValueType::kNull:
      return false;
    case ValueType::kBoolean:
      return std::get<bool>(storage_);
    case ValueType::kInt:
      return std::get<int64_t>(storage_) != 0;
    case ValueType::kUInt:
      return std::get<uint64_t>(storage_) != 0;
    case ValueType::kReal: {
      // Like JavaScript, zero and NaN are false.
      const double val = std::get<double>(storage_);
      return !std::isnan(val) && val != 0.0;
    }
    case ValueType::kString:
    case ValueType::kArray:
    case ValueType::kObject:
      break;
  }
  FailConversion("Value is not convertible to bool");
}

int Value::asInt() const {
  switch (type()) {
    case ValueType::kNull:
      return 0;
    case ValueType::kBoolean:
      return std::get<bool>(storage_) ? 1 : 0;
    case ValueType::kInt:
    case ValueType::kUInt:
      if (!isInt()) {
        FailConversion("Integer out of Int range");
      }
      return static_cast<int>(asInt64());
    case ValueType::kReal: {
      // Like JsonCpp, a fractional value in range is truncated.
      const double val = std::get<double>(storage_);
      if (!(val >= std::numeric_limits<int>::min() &&
            val <= std::numeric_limits<int>::max())) {
        FailConversion("Double out of Int range");
      }
      return static_cast<int>(val);
    }
    case ValueType::kString:
    case ValueType::kArray:
    case ValueType::kObject:
      break;
  }
  FailConversion("Value is not convertible to Int");
}

int64_t Value::asInt64() const {
  switch (type()) {
    case ValueType::kNull:
      return 0;
    case ValueType::kBoolean:
      return std::get<bool>(storage_) ? 1 : 0;
    case ValueType::kInt:
      return std::get<int64_t>(storage_);
    case ValueType::kUInt:
      if (!isInt64()) {
        FailConversion("UInt64 out of Int64 range");
      }
      return static_cast<int64_t>(std::get<uint64_t>(storage_));
    case ValueType::kReal: {
      // Like JsonCpp, -2^63 itself is rejected, and a fractional value in
      // range is truncated.
      const double val = std::get<double>(storage_);
      if (!(val > -kTwoTo63 && val < kTwoTo63)) {
        FailConversion("Double out of Int64 range");
      }
      return static_cast<int64_t>(val);
    }
    case ValueType::kString:
    case ValueType::kArray:
    case ValueType::kObject:
      break;
  }
  FailConversion("Value is not convertible to Int64");
}

unsigned int Value::asUInt() const {
  switch (type()) {
    case ValueType::kNull:
      return 0;
    case ValueType::kBoolean:
      return std::get<bool>(storage_) ? 1 : 0;
    case ValueType::kInt:
    case ValueType::kUInt:
      if (!isUInt()) {
        FailConversion("Integer out of UInt range");
      }
      return static_cast<unsigned int>(asUInt64());
    case ValueType::kReal: {
      const double val = std::get<double>(storage_);
      if (!(val >= 0.0 && val <= std::numeric_limits<unsigned int>::max())) {
        FailConversion("Double out of UInt range");
      }
      return static_cast<unsigned int>(val);
    }
    case ValueType::kString:
    case ValueType::kArray:
    case ValueType::kObject:
      break;
  }
  FailConversion("Value is not convertible to UInt");
}

uint64_t Value::asUInt64() const {
  switch (type()) {
    case ValueType::kNull:
      return 0;
    case ValueType::kBoolean:
      return std::get<bool>(storage_) ? 1 : 0;
    case ValueType::kInt:
      if (!isUInt64()) {
        FailConversion("Int64 out of UInt64 range");
      }
      return static_cast<uint64_t>(std::get<int64_t>(storage_));
    case ValueType::kUInt:
      return std::get<uint64_t>(storage_);
    case ValueType::kReal: {
      const double val = std::get<double>(storage_);
      if (!(val >= 0.0 && val < kTwoTo64)) {
        FailConversion("Double out of UInt64 range");
      }
      return static_cast<uint64_t>(val);
    }
    case ValueType::kString:
    case ValueType::kArray:
    case ValueType::kObject:
      break;
  }
  FailConversion("Value is not convertible to UInt64");
}

double Value::asDouble() const {
  switch (type()) {
    case ValueType::kNull:
      return 0.0;
    case ValueType::kBoolean:
      return std::get<bool>(storage_) ? 1.0 : 0.0;
    case ValueType::kInt:
      return static_cast<double>(std::get<int64_t>(storage_));
    case ValueType::kUInt:
      return static_cast<double>(std::get<uint64_t>(storage_));
    case ValueType::kReal:
      return std::get<double>(storage_);
    case ValueType::kString:
    case ValueType::kArray:
    case ValueType::kObject:
      break;
  }
  FailConversion("Value is not convertible to double");
}

std::string Value::asString() const {
  switch (type()) {
    case ValueType::kNull:
      return "";
    case ValueType::kString:
      return std::get<std::string>(storage_);
    case ValueType::kBoolean:
      return std::get<bool>(storage_) ? "true" : "false";
    case ValueType::kInt:
      return std::to_string(std::get<int64_t>(storage_));
    case ValueType::kUInt:
      return std::to_string(std::get<uint64_t>(storage_));
    case ValueType::kReal:
      return DoubleToString(std::get<double>(storage_));
    case ValueType::kArray:
    case ValueType::kObject:
      break;
  }
  FailConversion("Value is not convertible to string");
}

bool Value::getString(const char** begin, const char** end) const {
  if (std::holds_alternative<std::string>(storage_)) {
    const auto& s = std::get<std::string>(storage_);
    *begin = s.data();
    *end = s.data() + s.size();
    return true;
  }
  return false;
}

Value& Value::operator[](ArrayIndex index) {
  if (isNull()) {
    storage_ = Array{};
  }
  if (isArray()) {
    auto& arr = std::get<Array>(storage_);
    if (index >= arr.size()) {
      arr.resize(static_cast<size_t>(index) + 1);
    }
    return arr[index];
  }
  OSP_NOTREACHED();
}

const Value& Value::operator[](ArrayIndex index) const {
  if (isArray()) {
    const auto& arr = std::get<Array>(storage_);
    if (index < arr.size()) {
      return arr[index];
    }
  }
  return nullSingleton();
}

Value& Value::operator[](int index) {
  OSP_CHECK_GE(index, 0);
  return operator[](static_cast<ArrayIndex>(index));
}

const Value& Value::operator[](int index) const {
  if (index < 0) {
    return nullSingleton();
  }
  return operator[](static_cast<ArrayIndex>(index));
}

Value& Value::operator[](std::string_view key) {
  if (isNull()) {
    storage_ = Object{};
  }
  if (isObject()) {
    auto& obj = std::get<Object>(storage_);
    auto it = obj.lower_bound(key);
    if (it != obj.end() && it->first == key) {
      return it->second;
    }
    return obj.emplace_hint(it, std::string(key), Value())->second;
  }
  OSP_NOTREACHED();
}

const Value& Value::operator[](std::string_view key) const {
  if (isObject()) {
    const auto& obj = std::get<Object>(storage_);
    auto it = obj.find(key);
    if (it != obj.end()) {
      return it->second;
    }
  }
  return nullSingleton();
}

Value& Value::operator[](const std::string& key) {
  return operator[](std::string_view(key));
}

const Value& Value::operator[](const std::string& key) const {
  return operator[](std::string_view(key));
}

Value& Value::operator[](const char* key) {
  return operator[](std::string_view(key ? key : ""));
}

const Value& Value::operator[](const char* key) const {
  return operator[](std::string_view(key ? key : ""));
}

Value Value::get(std::string_view key, const Value& default_value) const {
  if (isObject()) {
    const auto& obj = std::get<Object>(storage_);
    auto it = obj.find(key);
    if (it != obj.end()) {
      return it->second;
    }
  }
  return default_value;
}

Value Value::get(ArrayIndex index, const Value& default_value) const {
  if (isArray()) {
    const auto& arr = std::get<Array>(storage_);
    if (index < arr.size()) {
      return arr[index];
    }
  }
  return default_value;
}

bool Value::isMember(std::string_view key) const {
  if (isObject()) {
    const auto& obj = std::get<Object>(storage_);
    return obj.find(key) != obj.end();
  }
  return false;
}

const Value* Value::find(const char* begin, const char* end) const {
  if (!begin || end < begin) {
    return nullptr;
  }
  return find(std::string_view(begin, end - begin));
}

const Value* Value::find(std::string_view key) const {
  if (isObject()) {
    const auto& obj = std::get<Object>(storage_);
    auto it = obj.find(key);
    if (it != obj.end()) {
      return &it->second;
    }
  }
  return nullptr;
}

std::vector<std::string> Value::getMemberNames() const {
  std::vector<std::string> names;
  if (isObject()) {
    const auto& obj = std::get<Object>(storage_);
    names.reserve(obj.size());
    for (const auto& [k, v] : obj) {
      names.push_back(k);
    }
  }
  return names;
}

Value::ArrayIndex Value::size() const {
  if (isArray()) {
    return static_cast<ArrayIndex>(std::get<Array>(storage_).size());
  }
  if (isObject()) {
    return static_cast<ArrayIndex>(std::get<Object>(storage_).size());
  }
  return 0;
}

bool Value::empty() const {
  if (isNull() || isArray() || isObject()) {
    return size() == 0;
  }
  return false;
}

void Value::clear() {
  if (isArray()) {
    std::get<Array>(storage_).clear();
  } else if (isObject()) {
    std::get<Object>(storage_).clear();
  } else if (!isNull()) {
    FailConversion("clear() requires a null, array, or object value");
  }
}

void Value::resize(ArrayIndex new_size) {
  if (isNull()) {
    storage_ = Array{};
  }
  if (!isArray()) {
    FailConversion("resize() requires a null or array value");
  }
  std::get<Array>(storage_).resize(new_size);
}

Value& Value::append(Value value) {
  if (isNull()) {
    storage_ = Array{};
  }
  if (isArray()) {
    auto& arr = std::get<Array>(storage_);
    arr.push_back(std::move(value));
    return arr.back();
  }
  OSP_NOTREACHED();
}

Value::iterator Value::begin() {
  if (isArray()) {
    return std::get<Array>(storage_).begin();
  }
  if (isObject()) {
    FailConversion("Iterating an object is not supported");
  }
  // Null and scalar values are empty ranges. Value-initialized iterators
  // compare equal.
  return iterator{};
}

Value::iterator Value::end() {
  if (isArray()) {
    return std::get<Array>(storage_).end();
  }
  if (isObject()) {
    FailConversion("Iterating an object is not supported");
  }
  return iterator{};
}

Value::const_iterator Value::begin() const {
  if (isArray()) {
    return std::get<Array>(storage_).begin();
  }
  if (isObject()) {
    FailConversion("Iterating an object is not supported");
  }
  return EmptyArray().begin();
}

Value::const_iterator Value::end() const {
  if (isArray()) {
    return std::get<Array>(storage_).end();
  }
  if (isObject()) {
    FailConversion("Iterating an object is not supported");
  }
  return EmptyArray().end();
}

const Value& Value::nullSingleton() {
  static const NoDestructor<Value> kNull;
  return *kNull;
}

bool Value::operator==(const Value& other) const {
  return storage_ == other.storage_;
}

bool Value::operator!=(const Value& other) const {
  return storage_ != other.storage_;
}

std::ostream& operator<<(std::ostream& os, const Value& val) {
  // TODO(crbug.com/577078424): Serialize json::Value directly once Stringify()
  // takes a json::Value, instead of copying it into a Json::Value.
  ErrorOr<std::string> str = Stringify(static_cast<::Json::Value>(val));
  if (str) {
    os << str.value();
  } else {
    os << "<invalid json value>";
  }
  return os;
}

}  // namespace openscreen::json

namespace openscreen {

std::optional<int> MaybeGetInt(const json::Value& message,
                               const char* first,
                               const char* last) {
  const json::Value* value = message.find(first, last);
  if (!value || !value->isInt()) {
    return std::nullopt;
  }
  return value->asInt();
}

std::optional<int> MaybeGetInt(const ::Json::Value& message,
                               const char* first,
                               const char* last) {
  const ::Json::Value* value = message.find(first, last);
  std::optional<int> result;
  if (value && value->isInt()) {
    result = value->asInt();
  }
  return result;
}

std::optional<std::string_view> MaybeGetString(const json::Value& message) {
  const char* begin = nullptr;
  const char* end = nullptr;
  if (message.getString(&begin, &end) && begin && end >= begin) {
    return std::string_view(begin, end - begin);
  }
  return std::nullopt;
}

std::optional<std::string_view> MaybeGetString(const ::Json::Value& message) {
  if (message.isString()) {
    const char* begin = nullptr;
    const char* end = nullptr;
    message.getString(&begin, &end);
    if (begin && end >= begin) {
      return std::string_view(begin, end - begin);
    }
  }
  return std::nullopt;
}

std::optional<std::string_view> MaybeGetString(const json::Value& message,
                                               const char* first,
                                               const char* last) {
  const json::Value* value = message.find(first, last);
  if (!value) {
    return std::nullopt;
  }
  return MaybeGetString(*value);
}

std::optional<std::string_view> MaybeGetString(const ::Json::Value& message,
                                               const char* first,
                                               const char* last) {
  const ::Json::Value* value = message.find(first, last);
  std::optional<std::string_view> result;
  if (value && value->isString()) {
    return MaybeGetString(*value);
  }
  return result;
}

}  // namespace openscreen
