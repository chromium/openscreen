// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_BIG_ENDIAN_H_
#define UTIL_BIG_ENDIAN_H_

#include <stdint.h>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <ranges>
#include <span>
#include <type_traits>

#include "platform/base/span.h"
#include "util/osp_logging.h"

namespace openscreen {

// Returns true if this code is running on a big-endian architecture.
constexpr bool IsBigEndianArchitecture() noexcept {
  return std::endian::native == std::endian::big;
}

// Returns true if this code is running on a little-endian architecture.
constexpr bool IsLittleEndianArchitecture() noexcept {
  return std::endian::native == std::endian::little;
}

// Returns the bytes of `x` in reverse order.
template <typename Integer>
  requires(std::is_integral_v<Integer> || std::is_enum_v<Integer>)
[[nodiscard]] constexpr Integer ByteSwap(Integer x) noexcept {
  using Unsigned = std::make_unsigned_t<Integer>;
  auto val = static_cast<Unsigned>(x);
  if constexpr (sizeof(Unsigned) == sizeof(uint8_t)) {
    return static_cast<Integer>(val);
#if defined(__clang__) || defined(__GNUC__)
  } else if constexpr (sizeof(Unsigned) == sizeof(uint16_t)) {
    return static_cast<Integer>(__builtin_bswap16(val));
  } else if constexpr (sizeof(Unsigned) == sizeof(uint32_t)) {
    return static_cast<Integer>(__builtin_bswap32(val));
  } else if constexpr (sizeof(Unsigned) == sizeof(uint64_t)) {
    return static_cast<Integer>(__builtin_bswap64(val));
#elif defined(_MSC_VER)
  } else if constexpr (sizeof(Unsigned) == sizeof(unsigned short)) {  // NOLINT
    return static_cast<Integer>(_byteswap_ushort(val));
  } else if constexpr (sizeof(Unsigned) == sizeof(unsigned long)) {  // NOLINT
    return static_cast<Integer>(_byteswap_ulong(val));
  } else if constexpr (sizeof(Unsigned) == sizeof(unsigned __int64)) {
    return static_cast<Integer>(_byteswap_uint64(val));
#endif
  } else {
    static_assert(sizeof(Unsigned) == 0,
                  "Unsupported integer size for ByteSwap");
  }
}

// Converts an integer or enum to big-endian byte order as a std::array.
template <typename Integer>
  requires(std::is_integral_v<Integer> || std::is_enum_v<Integer>)
[[nodiscard]] constexpr std::array<uint8_t, sizeof(Integer)> ToBigEndian(
    Integer val) noexcept {
  using Unsigned = std::make_unsigned_t<Integer>;
  auto uval = static_cast<Unsigned>(val);
  if constexpr (IsLittleEndianArchitecture()) {
    uval = ByteSwap(uval);
  }
  return std::bit_cast<std::array<uint8_t, sizeof(Integer)>>(uval);
}

// Converts a big-endian byte array to native byte order.
template <typename Integer>
  requires(std::is_integral_v<Integer> || std::is_enum_v<Integer>)
[[nodiscard]] constexpr Integer FromBigEndian(
    std::array<uint8_t, sizeof(Integer)> bytes) noexcept {
  using Unsigned = std::make_unsigned_t<Integer>;
  auto uval = std::bit_cast<Unsigned>(bytes);
  if constexpr (IsLittleEndianArchitecture()) {
    uval = ByteSwap(uval);
  }
  return static_cast<Integer>(uval);
}

// Read a POD integer from `src` in big-endian byte order, returning the integer
// in native byte order.
template <typename Integer>
  requires(std::is_integral_v<Integer> || std::is_enum_v<Integer>)
inline Integer ReadBigEndian(ByteView src) {
  OSP_CHECK_GE(src.size(), sizeof(Integer));
  std::array<uint8_t, sizeof(Integer)> bytes;
  std::copy_n(src.begin(), sizeof(Integer), bytes.begin());
  return FromBigEndian<Integer>(bytes);
}

// Write a POD integer `val` to `dest` in big-endian byte order.
template <typename Integer>
  requires(std::is_integral_v<Integer> || std::is_enum_v<Integer>)
inline void WriteBigEndian(Integer val, ByteBuffer dest) {
  OSP_CHECK_GE(dest.size(), sizeof(val));
  const auto bytes = ToBigEndian(val);
  std::ranges::copy(bytes, dest.begin());
}

// TODO(crbug.com/520101123): Remove unsafe raw pointer methods.
template <typename Integer>
  requires(std::is_integral_v<Integer> || std::is_enum_v<Integer>)
inline Integer ReadBigEndian(const void* src) {
  return ReadBigEndian<Integer>(
      ByteView(static_cast<const uint8_t*>(src), sizeof(Integer)));
}

// TODO(crbug.com/520101123): Remove unsafe raw pointer methods.
template <typename Integer>
  requires(std::is_integral_v<Integer> || std::is_enum_v<Integer>)
inline void WriteBigEndian(Integer val, void* dest) {
  WriteBigEndian(val, ByteBuffer(static_cast<uint8_t*>(dest), sizeof(val)));
}

template <class T>
class BigEndianBuffer {
 public:
  bool Skip(size_t length) {
    if (length > remaining()) {
      return false;
    }
    offset_ += length;
    return true;
  }

  Span<T> buffer() const { return buffer_; }
  Span<T> remaining_span() const { return buffer_.subspan(offset_); }
  // TODO(crbug.com/520101123): Remove unsafe raw pointer methods once
  // MdnsReader and MdnsWriter are fully spanified in follow-up CLs.
  T* begin() const { return buffer_.data(); }
  T* end() const { return buffer_.data() + buffer_.size(); }
  size_t length() const { return buffer_.size(); }
  size_t remaining() const { return buffer_.size() - offset_; }
  size_t offset() const { return offset_; }

  explicit BigEndianBuffer(Span<T> buffer) : buffer_(buffer) {}
  // TODO(crbug.com/520101123): Remove unsafe raw pointer and length methods.
  BigEndianBuffer(T* buffer, size_t length) : buffer_(buffer, length) {}
  BigEndianBuffer(const BigEndianBuffer&) = delete;
  BigEndianBuffer& operator=(const BigEndianBuffer&) = delete;

 private:
  Span<T> buffer_;
  size_t offset_ = 0;
};

class BigEndianReader : public BigEndianBuffer<const uint8_t> {
 public:
  explicit BigEndianReader(ByteView buffer);
  // TODO(crbug.com/520101123): Remove unsafe raw pointer and length methods.
  BigEndianReader(const uint8_t* buffer, size_t length);

  template <typename T>
    requires(std::is_integral_v<T> || std::is_enum_v<T>)
  bool Read(T* out) {
    ByteView view = remaining_span();
    if (view.size() >= sizeof(T)) {
      *out = ReadBigEndian<T>(view);
      Skip(sizeof(T));
      return true;
    }
    return false;
  }

  // TODO(crbug.com/520101123): Remove unsafe raw pointer and length methods.
  bool Read(size_t length, void* out);
  bool Read(ByteBuffer out);
};

class BigEndianWriter : public BigEndianBuffer<uint8_t> {
 public:
  explicit BigEndianWriter(ByteBuffer buffer);

  template <typename T>
    requires(std::is_integral_v<T> || std::is_enum_v<T>)
  bool Write(T value) {
    ByteBuffer view = remaining_span();
    if (view.size() >= sizeof(T)) {
      WriteBigEndian<T>(value, view);
      Skip(sizeof(T));
      return true;
    }
    return false;
  }

  bool Write(ByteView buffer);
};

}  // namespace openscreen

#endif  // UTIL_BIG_ENDIAN_H_
