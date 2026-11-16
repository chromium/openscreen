// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/json/json_value.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "gtest/gtest.h"
#include "platform/base/error.h"
#include "util/json/json_serialization.h"

namespace openscreen {

TEST(JsonValueTest, GetInt) {
  std::string_view obj(R"!({"key1": 17, "key2": 32.3, "key3": "asdf"})!");
  ErrorOr<json::Value> value_or_error = json::Parse(obj);
  ASSERT_TRUE(value_or_error);
  json::Value& value = value_or_error.value();
  std::optional<int> result1 =
      MaybeGetInt(value, JSON_EXPAND_FIND_CONSTANT_ARGS("key1"));
  std::optional<int> result2 =
      MaybeGetInt(value, JSON_EXPAND_FIND_CONSTANT_ARGS("key2"));
  std::optional<int> result3 =
      MaybeGetInt(value, JSON_EXPAND_FIND_CONSTANT_ARGS("key42"));
  EXPECT_FALSE(result2);
  EXPECT_FALSE(result3);

  ASSERT_TRUE(result1);
  EXPECT_EQ(result1.value(), 17);
}

TEST(JsonValueTest, GetString) {
  std::string_view obj(
      R"!({"key1": 17, "key2": 32.3, "key3": "asdf", "key4": ""})!");
  ErrorOr<json::Value> value_or_error = json::Parse(obj);
  ASSERT_TRUE(value_or_error);
  json::Value& value = value_or_error.value();
  std::optional<std::string_view> result1 =
      MaybeGetString(value, JSON_EXPAND_FIND_CONSTANT_ARGS("key3"));
  std::optional<std::string_view> result2 =
      MaybeGetString(value, JSON_EXPAND_FIND_CONSTANT_ARGS("key2"));
  std::optional<std::string_view> result3 =
      MaybeGetString(value, JSON_EXPAND_FIND_CONSTANT_ARGS("key42"));
  std::optional<std::string_view> result4 =
      MaybeGetString(value, JSON_EXPAND_FIND_CONSTANT_ARGS("key4"));

  EXPECT_FALSE(result2);
  EXPECT_FALSE(result3);

  ASSERT_TRUE(result1);
  EXPECT_EQ(result1.value(), "asdf");
  ASSERT_TRUE(result4);
  EXPECT_EQ(result4.value(), "");
}

TEST(JsonValueDomTest, PrimitiveTypes) {
  json::Value null_val;
  EXPECT_TRUE(null_val.isNull());
  EXPECT_EQ(null_val.type(), json::ValueType::kNull);
  EXPECT_TRUE(!null_val);

  json::Value bool_val(true);
  EXPECT_TRUE(bool_val.isBool());
  EXPECT_TRUE(bool_val.asBool());
  EXPECT_EQ(bool_val.type(), json::ValueType::kBoolean);

  json::Value int_val(-123);
  EXPECT_TRUE(int_val.isInt());
  EXPECT_TRUE(int_val.isInt64());
  EXPECT_FALSE(int_val.isUInt());
  EXPECT_EQ(int_val.asInt(), -123);
  EXPECT_EQ(int_val.asInt64(), -123);

  json::Value uint_val(456u);
  EXPECT_TRUE(uint_val.isUInt());
  EXPECT_TRUE(uint_val.isInt());
  EXPECT_EQ(uint_val.asUInt(), 456u);

  json::Value double_val(3.14159);
  EXPECT_TRUE(double_val.isDouble());
  EXPECT_TRUE(double_val.isNumeric());
  EXPECT_FALSE(double_val.isInt());
  EXPECT_DOUBLE_EQ(double_val.asDouble(), 3.14159);

  json::Value integral_double(500.0);
  EXPECT_TRUE(integral_double.isInt());
  EXPECT_TRUE(integral_double.isUInt());
  EXPECT_EQ(integral_double.asInt(), 500);

  json::Value str_val("hello");
  EXPECT_TRUE(str_val.isString());
  EXPECT_EQ(str_val.asString(), "hello");
}

TEST(JsonValueDomTest, ArrayOperations) {
  json::Value arr(json::ValueType::kArray);
  EXPECT_TRUE(arr.isArray());
  EXPECT_TRUE(arr.empty());
  EXPECT_EQ(arr.size(), 0u);

  arr.append(json::Value(1));
  arr.append(json::Value(2));
  arr.append(json::Value("three"));

  EXPECT_EQ(arr.size(), 3u);
  EXPECT_FALSE(arr.empty());
  EXPECT_EQ(arr[0].asInt(), 1);
  EXPECT_EQ(arr[1].asInt(), 2);
  EXPECT_EQ(arr[2].asString(), "three");
  const auto& const_arr = arr;
  EXPECT_TRUE(const_arr[99].isNull());
  EXPECT_EQ(arr.size(), 3u);

  int count = 0;
  for (const auto& item : arr) {
    if (count == 0)
      EXPECT_EQ(item.asInt(), 1);
    if (count == 1)
      EXPECT_EQ(item.asInt(), 2);
    if (count == 2)
      EXPECT_EQ(item.asString(), "three");
    ++count;
  }
  EXPECT_EQ(count, 3);
}

TEST(JsonValueDomTest, ObjectOperations) {
  json::Value obj(json::ValueType::kObject);
  EXPECT_TRUE(obj.isObject());
  EXPECT_TRUE(obj.empty());

  obj["foo"] = json::Value("bar");
  obj["num"] = json::Value(42);

  EXPECT_EQ(obj.size(), 2u);
  EXPECT_TRUE(obj.isMember("foo"));
  EXPECT_TRUE(obj.isMember("num"));
  EXPECT_FALSE(obj.isMember("nonexistent"));

  EXPECT_EQ(obj["foo"].asString(), "bar");
  EXPECT_EQ(obj["num"].asInt(), 42);
  EXPECT_TRUE(obj["nonexistent"].isNull());

  EXPECT_EQ(obj.get("foo", json::Value("default")).asString(), "bar");
  EXPECT_EQ(obj.get("missing", json::Value("default")).asString(), "default");

  const auto members = obj.getMemberNames();
  EXPECT_EQ(members.size(),
            3u);  // "foo", "num", and "nonexistent" (auto-inserted on read [])
}

TEST(JsonValueDomTest, JsonCppShims) {
  ::Json::Value cpp_val(::Json::objectValue);
  cpp_val["msg"] = "test";
  cpp_val["code"] = 200;

  json::Value os_val(cpp_val);
  EXPECT_TRUE(os_val.isObject());
  EXPECT_EQ(os_val["msg"].asString(), "test");
  EXPECT_EQ(os_val["code"].asInt(), 200);

  ::Json::Value converted_back = static_cast<::Json::Value>(os_val);
  EXPECT_TRUE(converted_back.isObject());
  EXPECT_EQ(converted_back["msg"].asString(), "test");
  EXPECT_EQ(converted_back["code"].asInt(), 200);
}

TEST(JsonValueDomTest, StreamOutput) {
  json::Value val(json::ValueType::kObject);
  val["a"] = json::Value(1);
  val["b"] = json::Value("two");

  ::Json::Value expected(::Json::objectValue);
  expected["a"] = 1;
  expected["b"] = "two";
  ErrorOr<std::string> expected_string = json::Stringify(expected);
  ASSERT_TRUE(expected_string);

  std::ostringstream ss;
  ss << val;
  EXPECT_EQ(ss.str(), expected_string.value());
}

TEST(JsonValueDomTest, EmptySemantics) {
  json::Value null_val;
  EXPECT_TRUE(null_val.empty());

  json::Value empty_arr(json::ValueType::kArray);
  EXPECT_TRUE(empty_arr.empty());

  json::Value non_empty_arr(json::ValueType::kArray);
  non_empty_arr.append(json::Value(1));
  EXPECT_FALSE(non_empty_arr.empty());

  json::Value empty_obj(json::ValueType::kObject);
  EXPECT_TRUE(empty_obj.empty());

  json::Value non_empty_obj(json::ValueType::kObject);
  non_empty_obj["key"] = json::Value(1);
  EXPECT_FALSE(non_empty_obj.empty());

  EXPECT_FALSE(json::Value(false).empty());
  EXPECT_FALSE(json::Value(true).empty());
  EXPECT_FALSE(json::Value(0).empty());
  EXPECT_FALSE(json::Value(0u).empty());
  EXPECT_FALSE(json::Value(0.0).empty());
  EXPECT_FALSE(json::Value("").empty());
  EXPECT_FALSE(json::Value("hello").empty());
}

TEST(JsonValueDomTest, NullIterationDoesNotMutate) {
  json::Value null_val;
  EXPECT_TRUE(null_val.isNull());

  int loop_count = 0;
  for (auto& item : null_val) {
    (void)item;
    ++loop_count;
  }
  EXPECT_EQ(loop_count, 0);
  EXPECT_TRUE(null_val.isNull());
  EXPECT_FALSE(null_val.isArray());

  null_val["key"] = json::Value("value");
  EXPECT_TRUE(null_val.isObject());
  EXPECT_EQ(null_val["key"].asString(), "value");
}

TEST(JsonValueDomTest, ScalarIterationIsEmpty) {
  json::Value str_val("hello");
  int loop_count = 0;
  for (auto& item : str_val) {
    (void)item;
    ++loop_count;
  }
  const json::Value& const_int_val = json::Value(5);
  for (const auto& item : const_int_val) {
    (void)item;
    ++loop_count;
  }
  EXPECT_EQ(loop_count, 0);
  EXPECT_TRUE(str_val.isString());
}

TEST(JsonValueDomTest, DoubleIntegerBoundaries) {
  // 2^63 and 2^64: INT64_MAX and UINT64_MAX round up to these as doubles.
  const json::Value two_to_63(static_cast<double>(INT64_MAX));
  EXPECT_FALSE(two_to_63.isInt64());
  EXPECT_TRUE(two_to_63.isUInt64());
  EXPECT_FALSE(two_to_63.GetIfInt64());
  EXPECT_EQ(two_to_63.GetIfUInt64(), uint64_t{1} << 63);

  const json::Value two_to_64(static_cast<double>(UINT64_MAX));
  EXPECT_FALSE(two_to_64.isUInt64());
  EXPECT_FALSE(two_to_64.GetIfUInt64());

  // Like JsonCpp, -2^63 is rejected, because nearby doubles round to it.
  const json::Value minus_two_to_63(static_cast<double>(INT64_MIN));
  EXPECT_FALSE(minus_two_to_63.isInt64());
  EXPECT_FALSE(minus_two_to_63.isIntegral());
  EXPECT_FALSE(minus_two_to_63.GetIfInt64());

  const json::Value nan_val(std::numeric_limits<double>::quiet_NaN());
  EXPECT_FALSE(nan_val.isInt());
  EXPECT_FALSE(nan_val.isInt64());
  EXPECT_FALSE(nan_val.isUInt());
  EXPECT_FALSE(nan_val.isUInt64());
  EXPECT_FALSE(nan_val.asBool());
  EXPECT_FALSE(nan_val.GetIfInt64());
  EXPECT_FALSE(nan_val.GetIfUInt64());

  const json::Value inf_val(std::numeric_limits<double>::infinity());
  EXPECT_FALSE(inf_val.isInt64());
  EXPECT_FALSE(inf_val.isUInt64());
  EXPECT_TRUE(inf_val.asBool());
  EXPECT_FALSE(inf_val.GetIfInt64());
  EXPECT_FALSE(inf_val.GetIfUInt64());
}

TEST(JsonValueDomTest, AsConversionsMatchJsonCpp) {
  // Compatible conversions that JsonCpp allows.
  EXPECT_EQ(json::Value().asInt(), 0);
  EXPECT_EQ(json::Value(true).asInt(), 1);
  EXPECT_EQ(json::Value(2.9).asInt(), 2);
  EXPECT_EQ(json::Value(-2.9).asInt64(), -2);
  EXPECT_EQ(json::Value(7u).asInt64(), 7);
  EXPECT_EQ(json::Value(7).asUInt64(), 7u);
  EXPECT_EQ(json::Value(INT64_MAX).asUInt64(),
            static_cast<uint64_t>(INT64_MAX));
  EXPECT_DOUBLE_EQ(json::Value(3).asDouble(), 3.0);
  EXPECT_EQ(json::Value(-5).asString(), "-5");
  EXPECT_EQ(json::Value(UINT64_MAX).asString(), "18446744073709551615");
  EXPECT_EQ(json::Value(false).asString(), "false");
  EXPECT_EQ(json::Value().asString(), "");
}

TEST(JsonValueDomTest, AsStringOnDoubleMatchesJsonCpp) {
  for (const double val :
       {0.0, -0.0, 1.0, -1.0, 0.1, 1.5, 1234567.0, 1e300, -1e-300, 5e-324,
        static_cast<double>(INT64_MAX), std::numeric_limits<double>::max(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()}) {
    SCOPED_TRACE(val);
    EXPECT_EQ(json::Value(val).asString(), ::Json::Value(val).asString());
  }
  EXPECT_EQ(json::Value(1234567.0).asString(), "1234567.0");
  EXPECT_EQ(json::Value(0.1).asString(), "0.10000000000000001");
  EXPECT_EQ(json::Value(std::numeric_limits<double>::quiet_NaN()).asString(),
            "null");
}

TEST(JsonValueDomTest, GetIfAccessors) {
  EXPECT_EQ(json::Value(5).GetIfInt(), 5);
  EXPECT_EQ(json::Value(5u).GetIfInt(), 5);
  EXPECT_EQ(json::Value(5.0).GetIfInt(), 5);
  EXPECT_FALSE(json::Value(5.5).GetIfInt());
  EXPECT_FALSE(json::Value(int64_t{1} << 40).GetIfInt());
  EXPECT_FALSE(json::Value(3e9).GetIfInt());
  EXPECT_FALSE(json::Value("5").GetIfInt());
  EXPECT_FALSE(json::Value(true).GetIfInt());
  EXPECT_FALSE(json::Value().GetIfInt());

  EXPECT_EQ(json::Value(3e9).GetIfUInt(), 3000000000u);
  EXPECT_FALSE(json::Value(-1).GetIfUInt());
  EXPECT_FALSE(json::Value(-1).GetIfUInt64());
  EXPECT_FALSE(json::Value(UINT64_MAX).GetIfInt64());
  EXPECT_EQ(json::Value(UINT64_MAX).GetIfUInt64(), UINT64_MAX);
  EXPECT_EQ(json::Value(INT64_MIN).GetIfInt64(), INT64_MIN);

  EXPECT_EQ(json::Value(5).GetIfDouble(), 5.0);
  EXPECT_EQ(json::Value(2.5).GetIfDouble(), 2.5);
  EXPECT_FALSE(json::Value("2.5").GetIfDouble());
  EXPECT_FALSE(json::Value(true).GetIfDouble());

  EXPECT_EQ(json::Value(true).GetIfBool(), true);
  EXPECT_EQ(json::Value(false).GetIfBool(), false);
  EXPECT_FALSE(json::Value(1).GetIfBool());
  EXPECT_FALSE(json::Value().GetIfBool());

  const std::string with_nul("a\0b", 3);
  const json::Value str_val(with_nul);
  ASSERT_TRUE(str_val.GetIfString());
  EXPECT_EQ(str_val.GetIfString().value(), with_nul);
  EXPECT_FALSE(json::Value(1).GetIfString());
  EXPECT_FALSE(json::Value().GetIfString());
}

TEST(JsonValueDomDeathTest, AsConversionsAbortLikeJsonCpp) {
  EXPECT_DEATH_IF_SUPPORTED(json::Value(-1).asUInt(), "");
  EXPECT_DEATH_IF_SUPPORTED(json::Value(-1).asUInt64(), "");
  EXPECT_DEATH_IF_SUPPORTED(json::Value(UINT64_MAX).asInt64(), "");
  EXPECT_DEATH_IF_SUPPORTED(json::Value(int64_t{1} << 40).asInt(), "");
  EXPECT_DEATH_IF_SUPPORTED(json::Value(3e9).asInt(), "");
  EXPECT_DEATH_IF_SUPPORTED(json::Value(1e300).asInt64(), "");
  EXPECT_DEATH_IF_SUPPORTED(
      json::Value(std::numeric_limits<double>::quiet_NaN()).asInt(), "");
  EXPECT_DEATH_IF_SUPPORTED(
      json::Value(static_cast<double>(INT64_MIN)).asInt64(), "");
  EXPECT_DEATH_IF_SUPPORTED(
      json::Value(static_cast<double>(UINT64_MAX)).asUInt64(), "");
  EXPECT_DEATH_IF_SUPPORTED(json::Value("5").asInt(), "");
  EXPECT_DEATH_IF_SUPPORTED(json::Value("5").asDouble(), "");
  EXPECT_DEATH_IF_SUPPORTED(json::Value("true").asBool(), "");
  EXPECT_DEATH_IF_SUPPORTED(json::Value(json::ValueType::kArray).asString(),
                            "");
  EXPECT_DEATH_IF_SUPPORTED(json::Value(json::ValueType::kObject).asInt(), "");
}

TEST(JsonValueDomDeathTest, ContainerOperationsAbortLikeJsonCpp) {
  EXPECT_DEATH_IF_SUPPORTED(json::Value(5).clear(), "");
  EXPECT_DEATH_IF_SUPPORTED(json::Value(json::ValueType::kObject).resize(2),
                            "");
  const json::Value obj(json::ValueType::kObject);
  EXPECT_DEATH_IF_SUPPORTED(obj.begin(), "");
  json::Value mutable_obj(json::ValueType::kObject);
  EXPECT_DEATH_IF_SUPPORTED(mutable_obj.begin(), "");
}

TEST(JsonValueDomTest, ClearAndResize) {
  json::Value null_val;
  null_val.clear();
  EXPECT_TRUE(null_val.isNull());

  json::Value resized;
  resized.resize(3);
  ASSERT_TRUE(resized.isArray());
  EXPECT_EQ(resized.size(), 3u);
  EXPECT_TRUE(resized[2].isNull());

  json::Value obj(json::ValueType::kObject);
  obj["a"] = 1;
  obj.clear();
  EXPECT_TRUE(obj.isObject());
  EXPECT_TRUE(obj.empty());
}

TEST(JsonValueDomTest, AssignFromOwnChild) {
  // Scalar child of an object.
  json::Value obj(json::ValueType::kObject);
  obj["n"] = 5;
  obj = obj["n"];
  EXPECT_EQ(obj, json::Value(5));

  // Object child of an object, like unwrapping a message envelope.
  json::Value msg(json::ValueType::kObject);
  msg["data"]["type"] = "OFFER";
  msg = msg["data"];
  ASSERT_TRUE(msg.isObject());
  EXPECT_EQ(msg["type"].asString(), "OFFER");

  // String child of an array.
  json::Value arr(json::ValueType::kArray);
  arr.append("x");
  arr = arr[0];
  EXPECT_EQ(arr, json::Value("x"));

  // Array child of an array, which keeps the same variant alternative.
  json::Value nested(json::ValueType::kArray);
  nested[0][0] = 1;
  nested[0][1] = 2;
  nested = nested[0];
  ASSERT_TRUE(nested.isArray());
  ASSERT_EQ(nested.size(), 2u);
  EXPECT_EQ(nested[1].asInt(), 2);

  // Move from a child.
  json::Value moved(json::ValueType::kObject);
  moved["child"]["k"] = true;
  moved = std::move(moved["child"]);
  ASSERT_TRUE(moved.isObject());
  EXPECT_TRUE(moved["k"].asBool());

  json::Value moved_scalar(json::ValueType::kArray);
  moved_scalar.append(1.5);
  moved_scalar = std::move(moved_scalar[0]);
  EXPECT_EQ(moved_scalar, json::Value(1.5));

  // Self-assignment.
  json::Value self(json::ValueType::kObject);
  self["a"] = 1;
  const json::Value& self_ref = self;
  self = self_ref;
  EXPECT_EQ(self["a"].asInt(), 1);
}

TEST(JsonValueDomTest, AppendOwnElement) {
  // append() takes its argument by value, so appending one of the array's own
  // elements is safe even when the array reallocates.
  json::Value arr(json::ValueType::kArray);
  arr.append("first");
  for (int i = 0; i < 100; ++i) {
    arr.append(arr[0]);
  }
  ASSERT_EQ(arr.size(), 101u);
  EXPECT_EQ(arr[100].asString(), "first");
}

TEST(JsonValueDomTest, ConstructFromJsonCppValueType) {
  // Json::ValueType is an unscoped enum, so without a dedicated constructor
  // these would pick Value(int).
  EXPECT_TRUE(json::Value(::Json::nullValue).isNull());
  EXPECT_EQ(json::Value(::Json::intValue).type(), json::ValueType::kInt);
  EXPECT_EQ(json::Value(::Json::uintValue).type(), json::ValueType::kUInt);
  EXPECT_EQ(json::Value(::Json::realValue).type(), json::ValueType::kReal);
  EXPECT_TRUE(json::Value(::Json::stringValue).isString());
  EXPECT_TRUE(json::Value(::Json::booleanValue).isBool());
  EXPECT_TRUE(json::Value(::Json::arrayValue).isArray());
  EXPECT_TRUE(json::Value(::Json::objectValue).isObject());

  json::Value implicit_array = ::Json::arrayValue;
  EXPECT_TRUE(implicit_array.isArray());
  json::Value implicit_object = json::objectValue;
  EXPECT_TRUE(implicit_object.isObject());

  json::Value root;
  root["streams"] = ::Json::arrayValue;
  EXPECT_TRUE(root["streams"].isArray());
}

TEST(JsonValueDomTest, Comparisons) {
  json::Value obj(json::ValueType::kObject);
  obj["type"] = "OFFER";
  obj["seqNum"] = 1;

  // Comparing against literals compiles and works.
  EXPECT_EQ(obj["type"], "OFFER");
  EXPECT_EQ(obj["seqNum"], 1);
  EXPECT_NE(obj["seqNum"], 2);

  // Like JsonCpp, signed, unsigned, and real values are different types.
  EXPECT_NE(json::Value(1), json::Value(1u));
  EXPECT_NE(json::Value(1), json::Value(1.0));

  // Comparing against a Json::Value, in both orders.
  ::Json::Value cpp(::Json::objectValue);
  cpp["type"] = "OFFER";
  cpp["seqNum"] = 1;
  EXPECT_TRUE(obj == cpp);
  EXPECT_TRUE(cpp == obj);
  EXPECT_FALSE(obj != cpp);
  cpp["seqNum"] = 2;
  EXPECT_TRUE(obj != cpp);
  EXPECT_TRUE(cpp != obj);
  cpp["seqNum"] = 1u;
  EXPECT_TRUE(obj != cpp);

  // operator bool matches Json::Value: true unless null.
  EXPECT_FALSE(json::Value());
  EXPECT_TRUE(json::Value(0));
  EXPECT_TRUE(json::Value(false));
}

TEST(JsonValueDomTest, ConstObjectLookupDoesNotInsert) {
  json::Value obj(json::ValueType::kObject);
  obj["a"] = 1;
  const json::Value& const_obj = obj;
  EXPECT_TRUE(const_obj["missing"].isNull());
  EXPECT_EQ(obj.size(), 1u);
}

TEST(JsonValueDomTest, JsonCppRoundTripConversion) {
  const std::string value_with_nul("a\0b", 3);
  const std::string key_with_nul("k\0ey", 4);

  ::Json::Value root(::Json::objectValue);
  root["title"] = "example";
  root["count"] = 10;
  root["negative"] = -42;
  root["int64_min"] = ::Json::Value::Int64{INT64_MIN};
  root["uint64_max"] = ::Json::Value::UInt64{UINT64_MAX};
  root["real"] = 0.1;
  root["integral_real"] = 1.0;
  root["negative_zero"] = -0.0;
  root["flag"] = false;
  root["with_nul"] = ::Json::Value(
      value_with_nul.data(), value_with_nul.data() + value_with_nul.size());
  root[key_with_nul] = "nul key";
  root["empty_arr"] = ::Json::Value(::Json::arrayValue);
  root["nested"]["inner"]["deep"] = "value";
  root["items"].append(1);
  root["items"].append("two");
  root["items"].append(::Json::Value(::Json::nullValue));

  json::Value os_val(root);
  EXPECT_TRUE(os_val == root);
  ASSERT_TRUE(os_val.isObject());
  EXPECT_EQ(os_val["title"].asString(), "example");
  EXPECT_EQ(os_val["count"].type(), json::ValueType::kInt);
  EXPECT_EQ(os_val["negative"].asInt(), -42);
  EXPECT_EQ(os_val["int64_min"].asInt64(), INT64_MIN);
  EXPECT_EQ(os_val["uint64_max"].type(), json::ValueType::kUInt);
  EXPECT_EQ(os_val["uint64_max"].asUInt64(), UINT64_MAX);
  EXPECT_EQ(os_val["real"].asDouble(), 0.1);
  EXPECT_EQ(os_val["integral_real"].type(), json::ValueType::kReal);
  EXPECT_TRUE(std::signbit(os_val["negative_zero"].asDouble()));
  EXPECT_FALSE(os_val["flag"].asBool());
  EXPECT_EQ(os_val["with_nul"].asString(), value_with_nul);
  EXPECT_EQ(os_val[key_with_nul].asString(), "nul key");
  EXPECT_TRUE(os_val["empty_arr"].isArray());
  EXPECT_TRUE(os_val["empty_arr"].empty());
  EXPECT_EQ(os_val["nested"]["inner"]["deep"].asString(), "value");
  ASSERT_EQ(os_val["items"].size(), 3u);
  EXPECT_EQ(os_val["items"][0].asInt(), 1);
  EXPECT_EQ(os_val["items"][1].asString(), "two");
  EXPECT_TRUE(os_val["items"][2].isNull());

  ::Json::Value roundtrip = static_cast<::Json::Value>(os_val);
  EXPECT_EQ(roundtrip, root);
  EXPECT_EQ(roundtrip["uint64_max"].type(), ::Json::uintValue);
  EXPECT_EQ(roundtrip["integral_real"].type(), ::Json::realValue);
  EXPECT_TRUE(std::signbit(roundtrip["negative_zero"].asDouble()));
  EXPECT_EQ(roundtrip["with_nul"].asString(), value_with_nul);
  EXPECT_EQ(roundtrip[key_with_nul].asString(), "nul key");
}

}  // namespace openscreen
