// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/std_util.h"

#include <array>
#include <set>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace openscreen {

TEST(StdUtilTest, Data) {
  std::string non_empty("Where no one has gone before");
  EXPECT_TRUE(data(non_empty) != nullptr);
  EXPECT_EQ(data(non_empty), non_empty.data());

  std::string empty;
  EXPECT_TRUE(data(empty) != nullptr);
  EXPECT_EQ(data(empty), empty.data());
}

TEST(StdUtilTest, AreElementSortedAndUnique) {
  EXPECT_TRUE(AreElementsSortedAndUnique(std::vector<std::string>({})));
  EXPECT_TRUE(AreElementsSortedAndUnique(std::vector<std::string>({"Joey"})));
  EXPECT_TRUE(AreElementsSortedAndUnique(
      std::vector<std::string>({"Chandler", "Joey", "Phoebe"})));
  EXPECT_FALSE(AreElementsSortedAndUnique(
      std::vector<std::string>({"Chandler", "Joey", "Joey", "Phoebe"})));
  EXPECT_FALSE(AreElementsSortedAndUnique(
      std::vector<std::string>({"Chandler", "Phoebe", "Joey"})));
}

TEST(StdUtilTest, Append) {
  std::vector<std::string> one_friend({"Joey"});
  auto friends = Append(std::move(one_friend), "Rachel", "Monica", "Chandler",
                        "Phoebe", "Ross");
  EXPECT_EQ(std::vector<std::string>(
                {"Joey", "Rachel", "Monica", "Chandler", "Phoebe", "Ross"}),
            friends);
}

TEST(StdUtilTest, GetVectorWithCapacity) {
  auto ten_strings(GetVectorWithCapacity<std::string>(10));
  EXPECT_EQ(static_cast<size_t>(0), ten_strings.size());
  EXPECT_EQ(static_cast<size_t>(10), ten_strings.capacity());
}

TEST(StdUtilTest, Contains) {
  auto friends = std::vector<std::string>(
      {"Joey", "Rachel", "Monica", "Chandler", "Phoebe", "Ross"});
  EXPECT_TRUE(Contains(friends, "Rachel"));
  EXPECT_FALSE(Contains(friends, "Ursula"));

  // Associative container with .contains()
  std::set<std::string> friend_set = {"Joey", "Monica", "Chandler"};
  EXPECT_TRUE(Contains(friend_set, "Joey"));
  EXPECT_FALSE(Contains(friend_set, "Ross"));
}

TEST(StdUtilTest, ContainsIf) {
  auto friends = std::vector<std::string>(
      {"Joey", "Rachel", "Monica", "Chandler", "Phoebe", "Ross"});
  EXPECT_TRUE(
      ContainsIf(friends, [](const auto& f) { return f == "Chandler"; }));
  EXPECT_FALSE(
      ContainsIf(friends, [](const auto& f) { return f == "Ursula"; }));
}

static_assert(AreElementsSortedAndUnique(std::array<int, 4>{1, 3, 5, 7}));
static_assert(!AreElementsSortedAndUnique(std::array<int, 4>{1, 3, 3, 7}));
static_assert(!AreElementsSortedAndUnique(std::array<int, 4>{1, 5, 3, 7}));

}  // namespace openscreen
