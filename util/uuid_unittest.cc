// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/uuid.h"

#include <stdint.h>

#include <array>
#include <limits>
#include <set>
#include <unordered_set>

#include "gtest/gtest.h"

namespace openscreen {

namespace {

// The format of Uuid version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
// where y is one of [8, 9, a, b].
bool IsValidV4(const Uuid& guid) {
  const std::string& lowercase = guid.AsLowercaseString();
  return guid.is_valid() && lowercase[14] == '4' &&
         (lowercase[19] == '8' || lowercase[19] == '9' ||
          lowercase[19] == 'a' || lowercase[19] == 'b');
}

}  // namespace

TEST(UuidTest, UuidBasicUniqueness) {
  constexpr int kIterations = 10;
  for (int i = 0; i < kIterations; ++i) {
    const Uuid guid1 = Uuid::GenerateRandomV4();
    const Uuid guid2 = Uuid::GenerateRandomV4();
    EXPECT_NE(guid1, guid2);
    EXPECT_TRUE(guid1.is_valid());
    EXPECT_TRUE(IsValidV4(guid1));
    EXPECT_TRUE(guid2.is_valid());
    EXPECT_TRUE(IsValidV4(guid2));
  }
}

TEST(UuidTest, InvalidUuid) {
  // Empty Uuid is invalid.
  EXPECT_FALSE(Uuid().is_valid());
  EXPECT_EQ(Uuid(), Uuid());
}

}  // namespace openscreen
