// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_CAST_PLATFORM_CLIENT_H_
#define CAST_SENDER_CAST_PLATFORM_CLIENT_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cast/common/channel/cast_message_handler.h"
#include "cast/sender/channel/message_util.h"
#include "platform/base/error.h"
#include "util/alarm.h"
#include "util/flat_map.h"
#include "util/json/json_value.h"
#include "util/raw_ref.h"

namespace openscreen::cast {

struct ReceiverInfo;
struct VirtualConnection;
class VirtualConnectionRouter;

// This class handles Cast messages that generally relate to the "platform", in
// other words not a specific app currently running (e.g. app availability,
// receiver status).  These messages follow a request/response format, so each
// request requires a corresponding response callback.  These requests will also
// timeout if there is no response after a certain amount of time (currently 5
// seconds).  The timeout callbacks will be called on the thread managed by
// `task_runner`.
class CastPlatformClient final : public CastMessageHandler {
 public:
  using AppAvailabilityCallback =
      std::function<void(std::string_view app_id, AppAvailabilityResult)>;

  // Session details parsed from the RECEIVER_STATUS that confirms a
  // successful LaunchSession() call.
  struct ReceiverStatus {
    std::string session_id;
    std::string app_id;
    std::string transport_id;
    std::string display_name;
    std::string status_text;
    std::vector<std::string> namespaces;
    bool is_idle_screen = false;
  };

  // Called with the launched session's details on success, or an Error
  // otherwise.
  using LaunchSessionCallback = std::function<void(ErrorOr<ReceiverStatus>)>;
  // Called with Error::None() on success, or a descriptive Error otherwise.
  using StopSessionCallback = std::function<void(const Error&)>;
  // Called with true once a Connect() has succeeded, or false if the
  // receiver is unknown, rejects the connection, or the attempt times out.
  using ConnectCallback = std::function<void(bool)>;

  // Connect() performs its CONNECT/CONNECTED handshake via
  // `router.OpenRemoteConnection()`, which forwards to whichever
  // ConnectionNamespaceHandler is registered on `router` (see
  // VirtualConnectionRouter::set_connection_namespace_handler()). If none is
  // registered, Connect() reports failure -- a ConnectionNamespaceHandler
  // must already be constructed on `router` for this class to be able to
  // connect to anything.
  CastPlatformClient(VirtualConnectionRouter& router,
                     ClockNowFunctionPtr clock,
                     TaskRunner& task_runner);
  ~CastPlatformClient() override;

  // Establishes (or confirms) the platform virtual connection to the
  // receiver identified by `receiver_id`. `callback` is invoked exactly
  // once, synchronously if already connected. Calling this explicitly is
  // optional: RequestAppAvailability(), LaunchSession(), and StopSession()
  // all connect on demand if needed, so this is useful for pre-warming a
  // connection or checking IsConnected() proactively.
  void Connect(std::string_view receiver_id, ConnectCallback callback);

  // Returns true if a platform virtual connection to `receiver_id` is
  // currently established.
  bool IsConnected(std::string_view receiver_id) const;

  // Requests availability information for `app_id` from the receiver identified
  // by `receiver_id`. `callback` will be called exactly once with a result.
  // Connects on demand if `receiver_id` isn't already connected (callers do
  // not need to call Connect() first), and only sends the actual request
  // once that completes.
  //
  // Returns std::nullopt if the request could not be started at all (unknown
  // receiver, or the request message could not be constructed); in that case
  // `callback` has already been invoked synchronously with the failure and
  // will not be called again. Otherwise, returns the id of the pending
  // request -- even while still waiting to connect -- which may be passed to
  // CancelRequest().
  std::optional<int> RequestAppAvailability(std::string_view receiver_id,
                                            std::string_view app_id,
                                            AppAvailabilityCallback callback);

  // Requests that the receiver identified by `receiver_id` launch `app_id`,
  // passing `app_params` as the app-specific launch parameters (may be null).
  // `callback` will be called exactly once with the result. LAUNCH_STATUS
  // reply carries no session details. Only RECEIVER_STATUS does, whether it
  // follows a LAUNCH_STATUS or arrives directly with a `requestId` that already
  // matches this request. Connects on demand if not already connected, the
  // same as RequestAppAvailability() -- callers do not need to call Connect()
  // first.
  //
  // Returns std::nullopt if the request could not be started at all (unknown
  // receiver, a launch is already pending for this receiver, or the request
  // message could not be constructed); in that case `callback` has already
  // been invoked synchronously with the failure and will not be called
  // again. Otherwise, returns the id of the pending request -- even while
  // still waiting to connect -- which may be passed to CancelRequest().
  std::optional<int> LaunchSession(std::string_view receiver_id,
                                   std::string_view app_id,
                                   const json::Value& app_params,
                                   LaunchSessionCallback callback);

  // Requests that the receiver identified by `receiver_id` stop the session
  // given by `session_id`. Connects on demand if not already connected, the
  // same as RequestAppAvailability(). Receivers do not send a response type
  // dedicated to confirming a stop; success is instead signaled by a
  // follow-up RECEIVER_STATUS in which `session_id` is no longer among the
  // running applications. An explicit rejection is reported directly on some
  // receivers. Some receivers return plain RECEIVER_STATUS instead of
  // rejecting a mismatched session id, same as a match. `callback` is called
  // with Error::None() once the stop is confirmed this way, or with an Error
  // if the connection could not be established, the receiver rejects the
  // request, or the request times out without a confirmation.
  std::optional<int> StopSession(std::string_view receiver_id,
                                 std::string_view session_id,
                                 StopSessionCallback callback);

  // Notifies this object about general receiver connectivity or property
  // changes.
  void AddOrUpdateReceiver(const ReceiverInfo& receiver, int socket_id);
  void RemoveReceiver(const ReceiverInfo& receiver);

  void CancelRequest(int request_id);

 private:
  struct AvailabilityRequest {
    int request_id;
    std::string app_id;
    std::unique_ptr<Alarm> timeout;
    AppAvailabilityCallback callback;
  };

  struct LaunchRequest {
    int request_id;
    std::unique_ptr<Alarm> timeout;
    LaunchSessionCallback callback;
  };

  struct StopRequest {
    int request_id;
    std::string session_id;
    std::unique_ptr<Alarm> timeout;
    StopSessionCallback callback;
  };

  struct PendingRequests {
    std::vector<AvailabilityRequest> availability;
    // At most one launch and stop may be pending per receiver at a time
    std::optional<LaunchRequest> launch_request;
    std::optional<StopRequest> stop_request;
  };

  // Tracks an in-flight Connect() for a receiver. `callbacks` collects every
  // caller waiting on this same connect if Connect() is called again for the
  // same receiver before the first attempt resolves; all of them are invoked
  // together once it does.
  struct PendingConnect {
    std::unique_ptr<Alarm> timeout;
    std::vector<ConnectCallback> callbacks;
  };

  // Returns the VirtualConnection to the platform endpoint on `receiver_id`'s
  // socket, or nullopt if no socket is registered for `receiver_id`.
  std::optional<VirtualConnection> GetPlatformConnection(
      std::string_view receiver_id) const;

  // FlatMap has no operator[]; these return the existing entry for
  // `receiver_id` if one exists, or insert and return a freshly
  // default-constructed one otherwise.
  PendingRequests& GetOrCreatePendingRequests(std::string_view receiver_id);
  PendingConnect& GetOrCreatePendingConnect(std::string_view receiver_id);

  // CastMessageHandler overrides.
  void OnMessage(VirtualConnectionRouter* router,
                 CastSocket* socket,
                 proto::CastMessage message) override;

  void HandleResponse(std::string_view receiver_id,
                      int request_id,
                      const json::Value& message);

  // Invokes and clears every callback queued in
  // `pending_connects_by_receiver_id_` for `receiver_id`, if any. Safe to
  // call more than once for the same receiver_id.
  void OnConnectResult(std::string receiver_id, bool success);

  // Each of these returns true if `request_id` matched a pending request of
  // its kind in `pending_requests` or false if it didn't match, so
  // HandleResponse() can try the next kind.
  bool HandleAppAvailabilityResponse(PendingRequests& pending_requests,
                                     int request_id,
                                     const json::Value& message);
  bool HandleLaunchResponse(PendingRequests& pending_requests,
                            int request_id,
                            const json::Value& message);
  bool HandleStopResponse(PendingRequests& pending_requests,
                          int request_id,
                          const json::Value& message);

  void CancelAppAvailabilityRequest(int request_id);

  // Resolves the pending launch/stop request matching `request_id`, if any,
  // with `error` and clears it. Shared by the request's own timeout and by
  // SendEnsuringConnected() failing (e.g. because connecting failed).
  void FailLaunchRequest(int request_id, Error error);
  void FailStopRequest(int request_id, Error error);

  // Alarm callback for a LaunchSession() request that received no response
  // within the timeout window.
  void HandleLaunchTimeout(int request_id);

  // Alarm callback for a StopSession() request that received no confirming
  // RECEIVER_STATUS (or rejection) within the timeout window. Treated as a
  // failure; silence is not success.
  void HandleStopTimeout(int request_id);

  static int GetNextRequestId();

  static int next_request_id_;

  const std::string sender_id_;
  const raw_ref<VirtualConnectionRouter> virtual_conn_router_;
  using ReceiverId = std::string;
  FlatMap<ReceiverId, int> socket_id_by_receiver_id_;
  FlatMap<ReceiverId, PendingRequests> pending_requests_by_receiver_id_;
  FlatMap<ReceiverId, PendingConnect> pending_connects_by_receiver_id_;

  const ClockNowFunctionPtr clock_;
  const raw_ref<TaskRunner> task_runner_;
};

}  // namespace openscreen::cast

#endif  // CAST_SENDER_CAST_PLATFORM_CLIENT_H_
