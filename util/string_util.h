// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_STRING_UTIL_H_
#define UTIL_STRING_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <concepts>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "platform/base/span.h"

// String query and manipulation utilities.
//
// NOTE: Open Screen provides its own ascii_* classification and conversion
// utilities instead of using <cctype> (e.g. std::isalpha, std::isdigit,
// std::tolower) for the following reasons:
// 1. Safety: <cctype> functions invoke undefined behavior when passed signed
//    char values < 0 (such as UTF-8 continuation bytes), requiring tedious
//    static_cast<unsigned char>(c).
// 2. Locale Independence: Network protocols, DNS-SD, mDNS, and JSON require
//    strict 7-bit US-ASCII behavior independent of the host OS/C locale.
// 3. Constexpr & Performance: These functions are constexpr, branchless, and
//    inline, avoiding runtime locale checks and enabling SIMD
//    auto-vectorization.
namespace openscreen {

// Determines whether `c` is a valid ASCII alphabetic character code.
template <typename Char>
  requires(std::integral<Char>)
constexpr bool ascii_isalpha(Char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// Determines whether `c` is a valid ASCII decimal digit (i.e. [0-9]).
template <typename Char>
  requires(std::integral<Char>)
constexpr bool ascii_isdigit(Char c) {
  return c >= '0' && c <= '9';
}

// Determines whether `c` is a valid ASCII lower case hexadecimal digit
// (i.e. [a-f0-9]).
template <typename Char>
  requires(std::integral<Char>)
constexpr bool ascii_islowerhex(Char c) {
  return ascii_isdigit(c) || (c >= 'a' && c <= 'f');
}

// Determines whether `c` is a valid ASCII hexadecimal digit (i.e. [a-fA-F0-9]).
template <typename Char>
  requires(std::integral<Char>)
constexpr bool ascii_ishex(Char c) {
  return ascii_islowerhex(c) || (c >= 'A' && c <= 'F');
}

// Determines whether `c` is a valid, printable ASCII character.
template <typename Char>
  requires(std::integral<Char>)
constexpr bool ascii_isprint(Char c) {
  return c >= ' ' && c <= '~';
}

// Determines whether `c` is a whitespace character
// (space, tab, vertical tab, formfeed, linefeed, or carriage return).
template <typename Char>
  requires(std::integral<Char>)
constexpr bool ascii_isspace(Char c) {
  return c == ' ' || (c >= '\t' && c <= '\r');
}

// If `c` is an upper case ASCII character, returns its lower case equivalent.
// Otherwise, returns `c` unchanged.
template <typename Char>
  requires(std::integral<Char>)
constexpr Char ascii_tolower(Char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<Char>(c + ('a' - 'A')) : c;
}

// Converts `s` to uppercase in-place.
inline void AsciiStrToUpper(std::string& s) {
  for (char& c : s) {
    if (c >= 'a' && c <= 'z') {
      c = static_cast<char>(c - ('a' - 'A'));
    }
  }
}

// Creates an uppercase string from a given string_view.
[[nodiscard]] inline std::string AsciiStrToUpper(std::string_view s) {
  std::string result(s);
  AsciiStrToUpper(result);
  return result;
}

// Returns whether given ASCII strings `a` and `b` are equal, ignoring
// case in the comparison.
[[nodiscard]] constexpr bool EqualsIgnoreCase(std::string_view a,
                                              std::string_view b) {
  return std::ranges::equal(
      a, b, std::equal_to<>{}, [](char c) { return ascii_tolower(c); },
      [](char c) { return ascii_tolower(c); });
}

// Returns std::string_view with whitespace stripped from the beginning of the
// given string_view.
[[nodiscard]] constexpr std::string_view StripLeadingAsciiWhitespace(
    std::string_view str) {
  while (!str.empty() && ascii_isspace(str.front())) {
    str.remove_prefix(1);
  }
  return str;
}

// Concatenates arguments into a single string.
[[nodiscard]] constexpr std::string StrCat(
    std::initializer_list<std::string_view> pieces) {
  size_t length = 0;
  for (const auto& piece : pieces) {
    length += piece.size();
  }

  std::string result;
  result.reserve(length);
  for (const auto& piece : pieces) {
    result.append(piece);
  }
  return result;
}

template <typename... Args>
  requires(sizeof...(Args) > 0 &&
           (std::convertible_to<Args, std::string_view> && ...))
[[nodiscard]] std::string StrCat(const Args&... args) {
  return StrCat({std::string_view(args)...});
}

// Splits `value` into tokens separated by `delim`. Leading and trailing
// delimiters are stripped, and multiple consecutive delimiters are treated as
// one.
[[nodiscard]] inline std::vector<std::string_view> Split(std::string_view value,
                                                         char delim) {
  std::vector<std::string_view> tokens;
  if (value.empty()) {
    return tokens;
  }

  size_t start = 0;
  while (start <= value.size()) {
    size_t end = value.find(delim, start);
    if (end == std::string_view::npos) {
      std::string_view token = value.substr(start);
      if (!token.empty()) {
        tokens.push_back(token);
      }
      break;
    }
    std::string_view token = value.substr(start, end - start);
    if (!token.empty()) {
      tokens.push_back(token);
    }
    start = end + 1;
  }
  return tokens;
}

// Splits a string at the first instance of `delim`. Returns std::nullopt if
// `delim` is not found.
[[nodiscard]] constexpr std::optional<
    std::pair<std::string_view, std::string_view>>
SplitFirst(std::string_view value, char delim) {
  size_t pos = value.find(delim);
  if (pos == std::string_view::npos) {
    return std::nullopt;
  }
  return std::pair{value.substr(0, pos), value.substr(pos + 1)};
}

// Joins a range of strings or string_views with `delimiter`.
template <std::ranges::input_range R>
  requires(
      std::convertible_to<std::ranges::range_reference_t<R>, std::string_view>)
[[nodiscard]] std::string Join(R&& range, std::string_view delimiter = ", ") {
  auto it = std::ranges::begin(range);
  auto end = std::ranges::end(range);
  if (it == end) {
    return {};
  }

  size_t total_length = 0;
  size_t count = 0;
  if constexpr (std::ranges::forward_range<R>) {
    for (auto cur = it; cur != end; ++cur) {
      total_length += std::string_view(*cur).size();
      ++count;
    }
    if (count > 1) {
      total_length += (count - 1) * delimiter.size();
    }
  }

  std::string result;
  if (total_length > 0) {
    result.reserve(total_length);
  }
  result.append(std::string_view(*it));
  ++it;
  for (; it != end; ++it) {
    result.append(delimiter);
    result.append(std::string_view(*it));
  }
  return result;
}

// Fallback Join for ranges of other streamable types (e.g. integers).
template <std::ranges::input_range R>
[[nodiscard]] std::string Join(R&& range, std::string_view delimiter = ", ") {
  auto it = std::ranges::begin(range);
  auto end = std::ranges::end(range);
  if (it == end) {
    return {};
  }
  std::stringstream ss;
  ss << *it;
  ++it;
  for (; it != end; ++it) {
    ss << delimiter << *it;
  }
  return ss.str();
}

// Returns a string made by concatenating the elements iterated by `[begin,
// end)`, each separated by `delimiter`.
template <std::input_iterator Iterator>
[[nodiscard]] std::string Join(Iterator begin,
                               Iterator end,
                               std::string_view delimiter = ", ") {
  return Join(std::ranges::subrange{begin, end}, delimiter);
}

// Returns a lowercase hex string representation of the given `bytes`.
inline std::string HexEncode(ByteView bytes) {
  static constexpr char kHexChars[] = "0123456789abcdef";
  if (bytes.empty()) {
    return {};
  }
  std::string result;
  result.resize(bytes.size() * 2);
  char* dest = result.data();
  for (uint8_t byte : bytes) {
    *dest++ = kHexChars[(byte >> 4) & 0x0F];
    *dest++ = kHexChars[byte & 0x0F];
  }
  return result;
}

inline std::string HexEncode(const uint8_t* bytes, size_t len) {
  return HexEncode(ByteView(bytes, len));
}

}  // namespace openscreen

#endif  // UTIL_STRING_UTIL_H_
