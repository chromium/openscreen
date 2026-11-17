// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/json/json_serialization.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "gtest/gtest.h"
#include "platform/api/runtime_options.h"
#include "platform/base/error.h"
#include "platform/test/scoped_runtime_options.h"
#include "util/json/json_serialization_internal.h"

namespace openscreen {
namespace {

using json::internal::JsonBackend;

template <typename Value>
void AssertError(ErrorOr<Value> error_or, Error::Code code) {
  EXPECT_EQ(error_or.error().code(), code);
}

// Runs each test against every JSON backend built into this binary.
class JsonSerializationTest : public testing::TestWithParam<JsonBackend> {
 protected:
  ErrorOr<json::Value> Parse(std::string_view document) {
    return json::internal::Parse(document, GetParam());
  }

  ErrorOr<std::string> Stringify(const json::Value& value) {
    return json::internal::Stringify(value, GetParam());
  }
};

}  // namespace

TEST(JsonSerializationDefaultBackendTest, FollowsRuntimeOptions) {
  RuntimeOptions options;
  options.use_rust_json = true;
  {
    ScopedRuntimeOptions scoped(options);
#if defined(USE_JSON_SERDE)
    EXPECT_EQ(json::internal::GetDefaultJsonBackend(), JsonBackend::kSerde);
#else
    // The serde backend is not built, so jsoncpp is used.
    EXPECT_EQ(json::internal::GetDefaultJsonBackend(), JsonBackend::kJsoncpp);
#endif  // defined(USE_JSON_SERDE)
  }

  options.use_rust_json = false;
  {
    ScopedRuntimeOptions scoped(options);
    EXPECT_EQ(json::internal::GetDefaultJsonBackend(), JsonBackend::kJsoncpp);
  }
}

TEST(JsonSerializationDefaultBackendTest, PublicApiParsesAndStringifies) {
  const auto parsed = json::Parse(R"({"foo": "bar"})");
  ASSERT_TRUE(parsed.is_value());
  EXPECT_EQ(parsed.value()["foo"].asString(), "bar");

  const auto serialized = json::Stringify(json::Value(true));
  ASSERT_TRUE(serialized.is_value());
  EXPECT_EQ(serialized.value(), "true");
}

TEST_P(JsonSerializationTest, MalformedDocumentReturnsParseError) {
  const std::array<std::string, 4> kMalformedDocuments{
      {"", "{", "{ foo: bar }", R"({"foo": "bar", "foo": baz})"}};

  for (auto& document : kMalformedDocuments) {
    AssertError(Parse(document), Error::Code::kJsonParseError);
  }
}

TEST_P(JsonSerializationTest, ValidEmptyDocumentParsedCorrectly) {
  const auto actual = Parse("{}");

  EXPECT_TRUE(actual.is_value());
  EXPECT_EQ(actual.value().getMemberNames().size(), 0u);
}

TEST_P(JsonSerializationTest, ValidDocumentParsedCorrectly) {
  const auto actual = Parse(R"({"foo": "bar", "baz": 1337})");

  EXPECT_TRUE(actual.is_value());
  EXPECT_EQ(actual.value().getMemberNames().size(), 2u);
  EXPECT_EQ(actual.value()["foo"].asString(), "bar");
  EXPECT_EQ(actual.value()["baz"].asInt(), 1337);
}

TEST_P(JsonSerializationTest, ComplexNestedDocumentParsedCorrectly) {
  const auto actual = Parse(
      R"({
        "string": "hello world",
        "int": 42,
        "bool": true,
        "null_val": null,
        "array": [1, "two", false],
        "nested": {"key": "val"}
      })");

  EXPECT_TRUE(actual.is_value());
  const auto& val = actual.value();
  EXPECT_EQ(val["string"].asString(), "hello world");
  EXPECT_EQ(val["int"].asInt(), 42);
  EXPECT_EQ(val["bool"].asBool(), true);
  EXPECT_TRUE(val["null_val"].isNull());
  EXPECT_EQ(val["array"].size(), 3u);
  EXPECT_EQ(val["array"][0].asInt(), 1);
  EXPECT_EQ(val["array"][1].asString(), "two");
  EXPECT_EQ(val["array"][2].asBool(), false);
  EXPECT_EQ(val["nested"]["key"].asString(), "val");
}

TEST_P(JsonSerializationTest, EmptyArrayReturnsBrackets) {
  const auto empty_array = json::Value(json::ValueType::kArray);
  const auto actual = Stringify(empty_array);

  EXPECT_TRUE(actual.is_value());
  EXPECT_EQ(actual.value(), "[]");
}

TEST_P(JsonSerializationTest, NullValueReturnsNull) {
  const auto null_value = json::Value();
  const auto actual = Stringify(null_value);

  EXPECT_TRUE(actual.is_value());
  EXPECT_EQ(actual.value(), "null");
}

TEST_P(JsonSerializationTest, ValidValueReturnsString) {
  const int64_t value = 31337;
  const auto actual = Stringify(json::Value(value));

  EXPECT_TRUE(actual.is_value());
  EXPECT_EQ(actual.value(), "31337");
}

TEST_P(JsonSerializationTest, BooleanValuesStringified) {
  EXPECT_EQ(Stringify(json::Value(true)).value(), "true");
  EXPECT_EQ(Stringify(json::Value(false)).value(), "false");
}

TEST_P(JsonSerializationTest, ArrayAndObjectRoundTrip) {
  const auto parsed = Parse(R"({"numbers":[1,2,3],"tag":"test"})");
  ASSERT_TRUE(parsed.is_value());
  const auto serialized = Stringify(parsed.value());
  ASSERT_TRUE(serialized.is_value());

  const auto reparsed = Parse(serialized.value());
  ASSERT_TRUE(reparsed.is_value());
  EXPECT_EQ(reparsed.value()["tag"].asString(), "test");
  EXPECT_EQ(reparsed.value()["numbers"].size(), 3u);
  EXPECT_EQ(reparsed.value()["numbers"][0].asInt(), 1);
  EXPECT_EQ(reparsed.value()["numbers"][1].asInt(), 2);
  EXPECT_EQ(reparsed.value()["numbers"][2].asInt(), 3);
}

TEST_P(JsonSerializationTest, ParsesObjectKeysWithEscapes) {
  const auto actual = Parse(R"({"quote\"key": 1, "line\nbreak": 2, )"
                            R"("caf\u00e9": 3, "k\u0000ey": 4})");

  ASSERT_TRUE(actual.is_value());
  const json::Value& value = actual.value();
  EXPECT_EQ(value.getMemberNames().size(), 4u);
  EXPECT_EQ(value["quote\"key"].asInt(), 1);
  EXPECT_EQ(value["line\nbreak"].asInt(), 2);
  EXPECT_EQ(value["caf\xC3\xA9"].asInt(), 3);
  EXPECT_EQ(value[std::string("k\0ey", 4)].asInt(), 4);
}

#if defined(USE_JSON_SERDE)
INSTANTIATE_TEST_SUITE_P(All,
                         JsonSerializationTest,
                         testing::Values(JsonBackend::kJsoncpp,
                                         JsonBackend::kSerde));
#else
INSTANTIATE_TEST_SUITE_P(All,
                         JsonSerializationTest,
                         testing::Values(JsonBackend::kJsoncpp));
#endif  // defined(USE_JSON_SERDE)

}  // namespace openscreen
