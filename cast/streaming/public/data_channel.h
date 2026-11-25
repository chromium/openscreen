// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_PUBLIC_DATA_CHANNEL_H_
#define CAST_STREAMING_PUBLIC_DATA_CHANNEL_H_

#include <string_view>

#include "platform/base/byte_span.h"
#include "platform/base/error.h"

namespace openscreen::cast {

class DataChannel {
 public:
  // Channel lifecycle. Note that `Close()` completes synchronously, so
  // `kClosing` is reported to the delegate but is never observable via
  // `state()` from outside a callback.
  enum class State { kConnecting, kOpen, kClosing, kClosed };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when data is received on the channel. `data` is only valid for
    // the duration of this call.
    virtual void OnMessage(ByteView data) = 0;

    // Called when the channel state changes.
    virtual void OnStateChange(State state) = 0;

    // Called when the channel's write buffer has space again
    // after a previous `Send()` failed with `kInsufficientBuffer`.
    virtual void OnWriteReady() = 0;
  };

  virtual ~DataChannel() = default;

  // Send a message on the channel. This is an all-or-nothing operation.
  // Returns `Error::None()` if the message was accepted for
  // buffering/transmission, otherwise:
  //   kInsufficientBuffer - transient; retry after `OnWriteReady()`.
  //   kConnectionFailed   - permanent; the channel is not usable.
  //   kParameterInvalid   - `data` exceeds the maximum message size.
  [[nodiscard]] virtual Error Send(ByteView data) = 0;

  // Close the channel.
  virtual void Close() = 0;

  // Get the channel label. The returned view is valid until the next call into
  // this DataChannel or until the channel is destroyed, whichever comes first.
  virtual std::string_view label() const = 0;

  // Get the current state of the channel.
  virtual State state() const = 0;

  // Set the delegate for handling events. Passing `nullptr` is valid and
  // unregisters the current delegate; subsequent events are dropped.
  virtual void SetDelegate(Delegate* delegate) = 0;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_PUBLIC_DATA_CHANNEL_H_
