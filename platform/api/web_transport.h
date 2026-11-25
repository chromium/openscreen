// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_WEB_TRANSPORT_H_
#define PLATFORM_API_WEB_TRANSPORT_H_

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "platform/base/byte_span.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"

namespace openscreen {

class TaskRunner;

// Represents a bidirectional WebTransport stream. Streams are created inside an
// active WebTransport session. They allow asynchronous reading and writing of
// bytes.
class WebTransportStream {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when data is available to read from the stream.
    // The implementation should read the data from the stream.
    // `data` is a view of the buffered data and is only valid during this call.
    virtual void OnRead(WebTransportStream* stream, ByteView data) = 0;

    // Called when the stream is closed by the remote peer.
    virtual void OnClose(WebTransportStream* stream) = 0;

    // Called when an error occurs on the stream.
    virtual void OnError(WebTransportStream* stream, const Error& error) = 0;

    // Called when the stream is ready for writing.
    virtual void OnWriteReady(WebTransportStream* stream) {}

    // Called when `stream` is about to be destroyed. After this returns,
    // `stream` is invalid and must not be dereferenced.
    //
    // Delegates that do not own the stream (e.g. incoming streams, which are
    // owned by the session) must drop their pointer to it here. There is no
    // need to call SetDelegate(nullptr): the delegate is automatically unset
    // before this is invoked, so re-entrant callbacks cannot occur.
    virtual void OnDestroyed(WebTransportStream* stream) {}
  };

  virtual ~WebTransportStream() = default;

  // Sets the Delegate associated with this instance.
  // Pass nullptr to unset the Delegate.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Writes data to the stream. Returns true if the data was successfully
  // buffered or sent.
  [[nodiscard]] virtual bool Write(ByteView data) = 0;

  // Closes the stream.
  virtual void Close() = 0;
};

// Represents a WebTransport session established between a client and a server.
// The session acts as a multiplexer for bidirectional streams and handles
// session closure.
class WebTransportSession {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when a new incoming bidirectional stream is accepted.
    // The session retains ownership of the stream.
    virtual void OnIncomingStream(WebTransportStream* stream) = 0;

    // Called when the session is ready to be used (handshake complete).
    virtual void OnSessionReady(WebTransportSession* session) = 0;

    // Called when the session is closed.
    virtual void OnSessionClosed(WebTransportSession* session,
                                 const Error& error) = 0;
  };

  virtual ~WebTransportSession() = default;

  // Sets the Delegate associated with this instance.
  // Pass nullptr to unset the Delegate.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Initiates the creation of a new outgoing bidirectional stream.
  // Returns the new stream, or an error if it fails.
  virtual ErrorOr<std::unique_ptr<WebTransportStream>>
  CreateOutgoingStream() = 0;

  // Closes the session with the given error.
  virtual void Close(const Error& error) = 0;
};

// Options used when establishing a WebTransport client session.
struct WebTransportOptions {
  // SHA-256 fingerprints of the certificates to trust for self-signed certs.
  // Base64 encoded.
  std::vector<std::string> server_certificate_hashes;
};

// A client that can initiate WebTransport connections to a remote server.
class WebTransportClient {
 public:
  using ConnectCallback =
      std::function<void(ErrorOr<std::unique_ptr<WebTransportSession>>)>;

  virtual ~WebTransportClient() = default;

  static std::unique_ptr<WebTransportClient> Create(TaskRunner& task_runner);

  // Connects to the given URL using the provided options.
  // The callback will be notified of the result.
  virtual void Connect(std::string_view url,
                       const WebTransportOptions& options,
                       ConnectCallback callback) = 0;
};

// A server that dynamically generates self-signed certificates and accepts
// incoming WebTransport client connections on a specified UDP port.
class WebTransportServer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when a new WebTransport session is established.
    virtual void OnSessionCreated(
        std::unique_ptr<WebTransportSession> session) = 0;

    // Called when the server is closed.
    virtual void OnServerClosed() = 0;
  };

  virtual ~WebTransportServer() = default;

  static std::unique_ptr<WebTransportServer> Create(TaskRunner& task_runner);

  virtual void SetDelegate(Delegate* delegate) = 0;

  // Starts the WebTransport server on the given local endpoint.
  // Returns the port the server is listening on, or 0 if it failed.
  [[nodiscard]] virtual uint16_t Start(const IPEndpoint& local_endpoint) = 0;

  // Stops the server.
  virtual void Stop() = 0;

  // Returns the certificate fingerprint (SHA-256 base64) of the dynamically
  // generated certificate.
  [[nodiscard]] virtual std::string GetCertificateFingerprint() const = 0;
};

}  // namespace openscreen

#endif  // PLATFORM_API_WEB_TRANSPORT_H_
