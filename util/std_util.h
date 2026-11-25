// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_STD_UTIL_H_
#define UTIL_STD_UTIL_H_

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>
#include <vector>

namespace openscreen {

// Returns true if the elements in range `c` are sorted in ascending order with
// no duplicate elements.
template <std::ranges::forward_range R>
constexpr bool AreElementsSortedAndUnique(const R& c) {
  return std::is_sorted(std::ranges::cbegin(c), std::ranges::cend(c)) &&
         std::adjacent_find(std::ranges::cbegin(c), std::ranges::cend(c)) ==
             std::ranges::cend(c);
}

// Append the provided elements together into a single vector using fold
// expressions.
template <typename T, typename... TOthers>
std::vector<T> Append(std::vector<T>&& so_far, TOthers&&... new_elements) {
  if constexpr (sizeof...(new_elements) > 0) {
    so_far.reserve(so_far.size() + sizeof...(new_elements));
    (so_far.push_back(std::forward<TOthers>(new_elements)), ...);
  }
  return std::move(so_far);
}

// Creates an empty vector with `size` elements reserved.
template <typename T>
std::vector<T> GetVectorWithCapacity(size_t size) {
  std::vector<T> results;
  results.reserve(size);
  return results;
}

// Returns true if an element equal to `element` is found in `container`.
// Uses member `.contains()` if available (e.g. std::set, std::map, FlatMap),
// otherwise falls back to std::ranges::find.
template <typename C, typename E>
constexpr bool Contains(const C& container, const E& element) {
  if constexpr (requires { container.contains(element); }) {
    return container.contains(element);
  } else {
    return std::find(std::ranges::cbegin(container),
                     std::ranges::cend(container),
                     element) != std::ranges::cend(container);
  }
}

// Returns true if any element in `container` returns true for `predicate`.
template <typename C, typename P>
constexpr bool ContainsIf(const C& container, P predicate) {
  return std::ranges::any_of(container, std::move(predicate));
}

}  // namespace openscreen

#endif  // UTIL_STD_UTIL_H_
