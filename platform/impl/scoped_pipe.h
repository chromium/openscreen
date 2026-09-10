// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_SCOPED_PIPE_H_
#define PLATFORM_IMPL_SCOPED_PIPE_H_

#include <unistd.h>

#include <utility>

namespace openscreen {

struct IntFdTraits {
  using PipeType = int;
  static constexpr int kInvalidValue = -1;

  static void Close(PipeType pipe) { close(pipe); }
};

// This class wraps file descriptor and uses RAII to ensure it is closed
// properly when control leaves its scope.  It is parameterized by a traits type
// which defines the value type of the file descriptor, an invalid value, and a
// closing function.
//
// This class is move-only as it represents ownership of the wrapped file
// descriptor.  It is not thread-safe.
template <typename Traits>
class ScopedPipe {
 public:
  using PipeType = typename Traits::PipeType;

  ScopedPipe() : pipe_(Traits::kInvalidValue) {}
  explicit ScopedPipe(PipeType pipe) : pipe_(pipe) {}
  ScopedPipe(const ScopedPipe&) = delete;
  ScopedPipe(ScopedPipe&& other) noexcept
      : pipe_(std::exchange(other.pipe_, Traits::kInvalidValue)) {}
  ~ScopedPipe() { Close(); }

  ScopedPipe& operator=(const ScopedPipe&) = delete;
  ScopedPipe& operator=(ScopedPipe&& other) noexcept {
    if (this != &other) {
      Close();
      pipe_ = std::exchange(other.pipe_, Traits::kInvalidValue);
    }
    return *this;
  }

  PipeType get() const { return pipe_; }

  bool operator==(const ScopedPipe& other) const = default;

  explicit operator bool() const { return pipe_ != Traits::kInvalidValue; }

 private:
  void Close() {
    if (pipe_ != Traits::kInvalidValue) {
      Traits::Close(pipe_);
      pipe_ = Traits::kInvalidValue;
    }
  }

  PipeType pipe_;
};

using ScopedFd = ScopedPipe<IntFdTraits>;

}  // namespace openscreen

#endif  // PLATFORM_IMPL_SCOPED_PIPE_H_
