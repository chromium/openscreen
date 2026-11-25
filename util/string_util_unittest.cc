// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/string_util.h"

#include <array>
#include <list>
#include <set>

#include "gtest/gtest.h"

namespace openscreen {

// Compile-time constexpr verification.
static_assert(ascii_isalpha('a'));
static_assert(ascii_isalpha('Z'));
static_assert(!ascii_isalpha('1'));
static_assert(ascii_isdigit('5'));
static_assert(!ascii_isdigit('a'));
static_assert(ascii_islowerhex('f'));
static_assert(!ascii_islowerhex('G'));
static_assert(ascii_ishex('F'));
static_assert(ascii_isprint(' '));
static_assert(ascii_isprint('~'));
static_assert(!ascii_isprint('\0'));
static_assert(!ascii_isprint('\n'));
static_assert(ascii_isspace(' '));
static_assert(ascii_isspace('\t'));
static_assert(ascii_isspace('\n'));
static_assert(ascii_isspace('\r'));
static_assert(!ascii_isspace('a'));
static_assert(ascii_tolower('A') == 'a');
static_assert(ascii_tolower('a') == 'a');
static_assert(EqualsIgnoreCase("", ""));
static_assert(EqualsIgnoreCase("abc", "ABC"));
static_assert(!EqualsIgnoreCase("abc", "ABCD"));
static_assert(StripLeadingAsciiWhitespace("  abc") == "abc");
static_assert(SplitFirst("key=value", '=').value() ==
              std::pair<std::string_view, std::string_view>{"key", "value"});
static_assert(!SplitFirst("novalue", '=').has_value());

// Reference: https://ascii-code.com
TEST(StringUtilTest, AsciiTest) {
  constexpr char kAlpha[] = "aAzZ";
  constexpr char kDigits[] = "09";
  constexpr char kPrintable[] = "*&$^ ";
  constexpr char kNonPrintable[] = "\000\010\015\177\202";

  for (size_t i = 0; i < sizeof(kAlpha) - 1; i++) {
    EXPECT_TRUE(ascii_isalpha(kAlpha[i])) << i;
    EXPECT_FALSE(ascii_isdigit(kAlpha[i])) << i;
    EXPECT_TRUE(ascii_isprint(kAlpha[i])) << i;
  }

  for (size_t i = 0; i < sizeof(kDigits) - 1; i++) {
    EXPECT_FALSE(ascii_isalpha(kDigits[i])) << i;
    EXPECT_TRUE(ascii_isdigit(kDigits[i])) << i;
    EXPECT_TRUE(ascii_isprint(kDigits[i])) << i;
  }

  for (size_t i = 0; i < sizeof(kPrintable) - 1; i++) {
    EXPECT_FALSE(ascii_isalpha(kPrintable[i])) << i;
    EXPECT_FALSE(ascii_isdigit(kPrintable[i])) << i;
    EXPECT_TRUE(ascii_isprint(kPrintable[i])) << i;
  }

  for (size_t i = 0; i < sizeof(kNonPrintable) - 1; i++) {
    EXPECT_FALSE(ascii_isalpha(kNonPrintable[i])) << i;
    EXPECT_FALSE(ascii_isdigit(kNonPrintable[i])) << i;
    EXPECT_FALSE(ascii_isprint(kNonPrintable[i])) << i;
  }

  EXPECT_EQ(ascii_tolower('A'), 'a');
  EXPECT_EQ(ascii_tolower('a'), 'a');
  EXPECT_EQ(ascii_tolower('0'), '0');
}

TEST(StringUtilTest, EqualsIgnoreCase) {
  constexpr char kString[] = "Vulcans!";
  EXPECT_TRUE(EqualsIgnoreCase("", ""));
  EXPECT_FALSE(EqualsIgnoreCase("", kString));
  EXPECT_FALSE(EqualsIgnoreCase("planet vulcan", kString));
  EXPECT_TRUE(EqualsIgnoreCase("Vulcans!", kString));
  EXPECT_TRUE(EqualsIgnoreCase("vUlCaNs!", kString));
  EXPECT_FALSE(EqualsIgnoreCase("vUlKaNs!", kString));
}

TEST(StringUtilTest, AsciiStrToUpper) {
  constexpr char kString[] = "Vulcans!";
  EXPECT_EQ("", AsciiStrToUpper(""));

  EXPECT_EQ("VULCANS!", AsciiStrToUpper("Vulcans!"));
  std::string s1(kString);
  AsciiStrToUpper(s1);
  EXPECT_EQ("VULCANS!", s1);
}

TEST(StringUtilTest, StripLeadingAsciiWhitespace) {
  EXPECT_EQ("", StripLeadingAsciiWhitespace(""));
  EXPECT_EQ("", StripLeadingAsciiWhitespace("   \t\r\n "));
  EXPECT_EQ("abc", StripLeadingAsciiWhitespace("abc"));
  EXPECT_EQ("abc  ", StripLeadingAsciiWhitespace("  abc  "));
}

TEST(StringUtilTest, StrCat) {
  EXPECT_EQ(std::string(), StrCat({}));
  EXPECT_EQ(std::string(), StrCat({"", ""}));
  EXPECT_EQ(std::string("abcdef"), StrCat({"abc", std::string("def")}));

  // Variadic StrCat.
  EXPECT_EQ("abcdef", StrCat("abc", "def"));
  EXPECT_EQ("123456", StrCat("1", "2", "3", "4", "5", "6"));
  EXPECT_EQ("hello world",
            StrCat(std::string("hello"), " ", std::string_view("world")));
}

TEST(StringUtilTest, Split) {
  std::vector<std::string_view> result;
  std::vector<std::string_view> empty;
  auto single = std::vector<std::string_view>({"donut"});
  auto expected = std::vector<std::string_view>({"a", "b", "ccc"});

  result = Split("", ';');
  EXPECT_EQ(result, empty);
  result = Split(";;;;;", ';');
  EXPECT_EQ(result, empty);
  result = Split("donut", ';');
  EXPECT_EQ(result, single);
  result = Split(";;;donut", ';');
  EXPECT_EQ(result, single);
  result = Split("donut;;;", ';');
  EXPECT_EQ(result, single);
  result = Split("a;;b;;;ccc", ';');
  EXPECT_EQ(result, expected);
  result = Split(";;;a;;b;;;ccc", ';');
  EXPECT_EQ(result, expected);
  result = Split(";;;a;;b;;;ccc;;;;", ';');
  EXPECT_EQ(result, expected);
}

TEST(StringUtilTest, SplitFirst) {
  EXPECT_EQ((std::pair<std::string_view, std::string_view>{"foo", "bar"}),
            SplitFirst("foo=bar", '='));
  EXPECT_EQ((std::pair<std::string_view, std::string_view>{"foo", "bar=baz"}),
            SplitFirst("foo=bar=baz", '='));
  EXPECT_EQ((std::pair<std::string_view, std::string_view>{"", "bar"}),
            SplitFirst("=bar", '='));
  EXPECT_EQ((std::pair<std::string_view, std::string_view>{"foo", ""}),
            SplitFirst("foo=", '='));
  EXPECT_FALSE(SplitFirst("foobar", '=').has_value());
}

TEST(StringUtilTest, JoinStringViewCollection) {
  std::vector<std::string_view> empty;
  auto single = std::vector<std::string_view>({"donut"});
  auto input = std::vector<std::string_view>({"foo", "bar", "bazzz"});

  EXPECT_EQ("", Join(empty.begin(), empty.end(), ","));
  EXPECT_EQ("", Join(empty, ","));

  EXPECT_EQ("donut", Join(single.begin(), single.end(), ","));
  EXPECT_EQ("donut", Join(single, ","));

  EXPECT_EQ("foobarbazzz", Join(input.begin(), input.end(), ""));
  EXPECT_EQ("foo,bar,bazzz", Join(input.begin(), input.end(), ","));
  EXPECT_EQ("foo<->bar<->bazzz", Join(input.begin(), input.end(), "<->"));

  EXPECT_EQ("foo, bar, bazzz", Join(input));
  EXPECT_EQ("foo_*_bar_*_bazzz", Join(input, "_*_"));
}

TEST(StringUtilTest, JoinIntegerCollection) {
  std::vector<int> empty;
  std::vector<int> single = {1};
  std::vector<int> multiple = {{2, 29, 99}};

  EXPECT_EQ("", Join(empty));
  EXPECT_EQ("", Join(empty, "---"));

  EXPECT_EQ("1", Join(single));
  EXPECT_EQ("1", Join(single.begin(), single.end(), "*"));

  EXPECT_EQ("2, 29, 99", Join(multiple));
  EXPECT_EQ("2 * 29 * 99", Join(multiple.begin(), multiple.end(), " * "));
}

TEST(StringUtilTest, JoinGenericContainers) {
  std::set<std::string> set_input = {"apple", "banana", "cherry"};
  EXPECT_EQ("apple, banana, cherry", Join(set_input));
  EXPECT_EQ("apple | banana | cherry", Join(set_input, " | "));

  std::list<int> list_input = {10, 20, 30};
  EXPECT_EQ("10, 20, 30", Join(list_input));

  std::array<std::string_view, 3> array_input = {"x", "y", "z"};
  EXPECT_EQ("x-y-z", Join(array_input, "-"));
}

TEST(StringUtilTest, HexEncode) {
  const uint8_t kSomeMemoryLocation = 0;
  EXPECT_EQ("", HexEncode(&kSomeMemoryLocation, 0));
  EXPECT_EQ("", HexEncode(ByteView{}));

  const uint8_t kMessage[] = "Hello world!";
  const char kMessageInHex[] = "48656c6c6f20776f726c642100";
  EXPECT_EQ(kMessageInHex, HexEncode(kMessage, sizeof(kMessage)));
  EXPECT_EQ(kMessageInHex, HexEncode(ByteView(kMessage, sizeof(kMessage))));

  const uint8_t kAllBytes[] = {0x00, 0x0F, 0x10, 0xAB, 0xCD, 0xEF, 0xFF};
  EXPECT_EQ("000f10abcdefff", HexEncode(kAllBytes, sizeof(kAllBytes)));
}

}  // namespace openscreen
