// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/socket_handle_waiter_posix.h"

#include <poll.h>
#include <stdint.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <vector>

#include "platform/base/error.h"
#include "platform/impl/socket_handle_posix.h"
#include "platform/impl/udp_socket_posix.h"
#include "util/osp_logging.h"

namespace openscreen {

SocketHandleWaiterPosix::SocketHandleWaiterPosix(
    ClockNowFunctionPtr now_function)
    : SocketHandleWaiter(now_function) {}

SocketHandleWaiterPosix::~SocketHandleWaiterPosix() = default;

ErrorOr<std::vector<SocketHandleWaiterPosix::HandleWithFlags>>
SocketHandleWaiterPosix::AwaitSocketsReady(
    const std::vector<SocketHandleWaiterPosix::HandleWithFlags>& sockets,
    const Clock::duration& timeout) {
  if (sockets.empty()) {
    return Error::Code::kAgain;
  }

  std::vector<struct pollfd> pollfds;
  pollfds.reserve(sockets.size());
  for (const HandleWithFlags& hwf : sockets) {
    if (hwf.handle.get().fd < 0) {
      return Error::Code::kIOFailure;
    }
    decltype(pollfd::events) events = 0;
    if (hwf.flags & Flags::kReadable) {
      events |= POLLIN;
    }
    if (hwf.flags & Flags::kWritable) {
      events |= POLLOUT;
    }
    OSP_CHECK_GT(events, 0);
    pollfds.push_back(
        {.fd = hwf.handle.get().fd, .events = events, .revents = 0});
  }

  OSP_CHECK_GE(timeout, Clock::duration::zero());
  const int64_t timeout_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
  OSP_CHECK_LE(timeout_ms,
               static_cast<int64_t>(std::numeric_limits<int>::max()));
  const int timeout_arg = static_cast<int>(timeout_ms);
  const int rv = poll(pollfds.data(), pollfds.size(), timeout_arg);
  if (rv == -1) {
    return Error::Code::kIOFailure;
  } else if (rv == 0) {
    // poll() timed out and zero file descriptors have ready events.
    return Error::Code::kAgain;
  }

  std::vector<HandleWithFlags> changed_handles;
  for (size_t i = 0; i < sockets.size(); ++i) {
    uint32_t flags = 0;
    const decltype(pollfd::revents) revents = pollfds[i].revents;
    // Map POLLHUP, POLLERR, and POLLNVAL to readable/writable so subscribers
    // are unblocked to handle EOF/errors on the next read() or write(),
    // matching select() behavior where closed or errored sockets become ready.
    if (revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
      flags |= Flags::kReadable;
    }
    if (revents & (POLLOUT | POLLHUP | POLLERR | POLLNVAL)) {
      flags |= Flags::kWritable;
    }
    // Only return flags that were originally requested.
    flags &= sockets[i].flags;
    if (flags) {
      changed_handles.push_back({sockets[i].handle, flags});
    }
  }
  return changed_handles;
}

void SocketHandleWaiterPosix::RunUntilStopped() {
  const bool was_running =
      is_running_.exchange(true, std::memory_order_acq_rel);
  OSP_CHECK(!was_running);

  constexpr Clock::duration kHandleReadyTimeout = std::chrono::milliseconds(50);
  while (is_running_.load(std::memory_order_relaxed)) {
    ProcessHandles(kHandleReadyTimeout);
  }
}

void SocketHandleWaiterPosix::RequestStopSoon() {
  is_running_.store(false, std::memory_order_release);
}

}  // namespace openscreen
