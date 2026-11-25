// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/uuid.h"

#include <stddef.h>
#include <stdint.h>

#include <format>
#include <ostream>

#include "util/big_endian.h"
#include "util/crypto/random_bytes.h"
#include "util/osp_logging.h"

namespace openscreen {

// static
Uuid Uuid::GenerateRandomV4() {
  return FormatRandomDataAsV4Impl(GenerateRandomBytes16());
}

// static
Uuid Uuid::FormatRandomDataAsV4Impl(ByteView input) {
  OSP_CHECK_EQ(input.size(), kGuidV4InputLength);

  auto first_u64 = ReadBigEndian<uint64_t>(input.first(8));
  auto second_u64 = ReadBigEndian<uint64_t>(input.last(8));

  // Set the Uuid to version 4 as described in RFC 4122, section 4.4.
  // The format of Uuid version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
  // where y is one of [8, 9, a, b].

  // Clear the version bits and set the version to 4:
  first_u64 &= 0xffffffff'ffff0fffULL;
  first_u64 |= 0x00000000'00004000ULL;

  // Clear bit 65 and set bit 64, to set the 'var' field to 0b10 per RFC 9562
  // section 5.4.
  second_u64 &= 0x3fffffff'ffffffffULL;
  second_u64 |= 0x80000000'00000000ULL;

  Uuid uuid;
  uuid.lowercase_ =
      std::format("{:08x}-{:04x}-{:04x}-{:04x}-{:012x}",
                  static_cast<uint32_t>(first_u64 >> 32),
                  static_cast<uint32_t>((first_u64 >> 16) & 0x0000'ffff),
                  static_cast<uint32_t>(first_u64 & 0x0000'ffff),
                  static_cast<uint32_t>(second_u64 >> 48),
                  second_u64 & 0x0000'ffff'ffff'ffffULL);
  return uuid;
}

Uuid::Uuid() = default;
Uuid::Uuid(const Uuid& other) = default;
Uuid::Uuid(Uuid&& other) noexcept = default;
Uuid& Uuid::operator=(const Uuid& other) = default;
Uuid& Uuid::operator=(Uuid&& other) = default;

const std::string& Uuid::AsLowercaseString() const {
  return lowercase_;
}

std::ostream& operator<<(std::ostream& out, const Uuid& uuid) {
  return out << uuid.AsLowercaseString();
}

}  // namespace openscreen
