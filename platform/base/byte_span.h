// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_BYTE_SPAN_H_
#define PLATFORM_BASE_BYTE_SPAN_H_

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace openscreen {

// Convenience aliases for byte spans.
using ByteView = std::span<const uint8_t>;
using ByteBuffer = std::span<uint8_t>;

inline ByteView ByteViewFromString(std::string_view str) {
  return ByteView(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

inline std::string ByteViewToString(ByteView bytes) {
  return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

}  // namespace openscreen

#endif  // PLATFORM_BASE_BYTE_SPAN_H_
