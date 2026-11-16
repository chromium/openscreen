// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/cast_platform_client.h"

#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <string_view>
#include <utility>

#include "cast/common/channel/virtual_connection_router.h"
#include "cast/common/public/cast_socket.h"
#include "cast/common/public/receiver_info.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"

namespace openscreen::cast {

namespace {
constexpr std::chrono::seconds kRequestTimeout = std::chrono::seconds(60);

// Returns true if `session_id` is listed among the running applications in
// a RECEIVER_STATUS message's payload. Receivers confirm a successful
// STOP by omitting the stopped session from a follow-up RECEIVER_STATUS
// rather than with a dedicated response type.
bool IsSessionStillRunning(const json::Value& message,
                           const std::string_view session_id) {
  const json::Value* status =
      message.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyStatus));
  if (!status || !status->isObject()) {
    return false;
  }
  const json::Value* applications =
      status->find(JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyApplications));
  if (!applications || !applications->isArray()) {
    return false;
  }
  for (const json::Value& app : *applications) {
    std::optional<std::string_view> id = MaybeGetString(
        app, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeySessionId));
    if (id && id.value() == session_id) {
      return true;
    }
  }
  return false;
}

// Parses the session details from RECEIVER_STATUS's `status` object.
std::optional<CastPlatformClient::ReceiverStatus> ParseReceiverStatus(
    const json::Value& status) {
  const json::Value* applications =
      status.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyApplications));
  if (!applications || !applications->isArray() || applications->size() != 1) {
    return std::nullopt;
  }
  const json::Value& app = (*applications)[0];
  std::optional<std::string_view> session_id =
      MaybeGetString(app, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeySessionId));
  std::optional<std::string_view> app_id =
      MaybeGetString(app, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyAppId));
  std::optional<std::string_view> transport_id = MaybeGetString(
      app, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyTransportId));
  std::optional<std::string_view> display_name = MaybeGetString(
      app, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyDisplayName));
  if (!session_id || !app_id || !transport_id || !display_name) {
    return std::nullopt;
  }

  CastPlatformClient::ReceiverStatus result;
  result.session_id = std::string(*session_id);
  result.app_id = std::string(*app_id);
  result.transport_id = std::string(*transport_id);
  result.display_name = std::string(*display_name);
  result.status_text = std::string(
      MaybeGetString(app, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyStatusText))
          .value_or(""));

  const json::Value* is_idle_screen =
      app.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyIsIdleScreen));
  result.is_idle_screen =
      is_idle_screen && is_idle_screen->isBool() && is_idle_screen->asBool();

  const json::Value* namespaces =
      app.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyNamespaces));
  if (namespaces && namespaces->isArray()) {
    for (const json::Value& ns : *namespaces) {
      std::optional<std::string_view> name =
          MaybeGetString(ns, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyName));
      if (name) {
        result.namespaces.emplace_back(*name);
      }
    }
  }
  return result;
}

}  // namespace

CastPlatformClient::CastPlatformClient(VirtualConnectionRouter& router,
                                       ClockNowFunctionPtr clock,
                                       TaskRunner& task_runner)
    : sender_id_(MakeUniqueSessionId("sender")),
      virtual_conn_router_(router),
      clock_(clock),
      task_runner_(task_runner) {
  OSP_CHECK(clock_);
  virtual_conn_router_->AddHandlerForLocalId(sender_id_, this);
}

CastPlatformClient::~CastPlatformClient() {
  virtual_conn_router_->RemoveConnectionsByLocalId(sender_id_);
  virtual_conn_router_->RemoveHandlerForLocalId(sender_id_);

  for (auto& pending_requests : pending_requests_by_receiver_id_) {
    for (auto& avail_request : pending_requests.second.availability) {
      avail_request.callback(avail_request.app_id,
                             AppAvailabilityResult::kUnknown);
    }
    if (pending_requests.second.launch_request) {
      pending_requests.second.launch_request->callback(Error(
          Error::Code::kOperationCancelled, "CastPlatformClient destroyed"));
    }
    if (pending_requests.second.stop_request) {
      pending_requests.second.stop_request->callback(Error(
          Error::Code::kOperationCancelled, "CastPlatformClient destroyed"));
    }
  }

  // Cancel every in-flight Connect(). Since OpenRemoteConnection() callback
  // captures `this` directly and has no other way to be invalidated, it must be
  // synchronously resolved via CloseRemoteConnection() now.
  //
  // Iterating in reverse is safe because `pending_connects_by_receiver_id_`
  // is a vector-backed FlatMap. CloseRemoteConnection() can reentrantly erase
  // the last element via OnConnectResult(), which doesn't shifts or invalidates
  // the earlier elements this loop still needs to visit. This breaks if the
  // container type ever changes.
  for (auto& receiver_id : pending_connects_by_receiver_id_ |
                               std::views::reverse | std::views::keys) {
    auto virtual_conn = GetPlatformConnection(receiver_id);
    if (virtual_conn) {
      virtual_conn_router_->CloseRemoteConnection(*virtual_conn);
    }
  }

  // Fallback for any pending connect CloseRemoteConnection() didn't resolve
  // above.
  for (auto& [_, pending_connects] : pending_connects_by_receiver_id_) {
    for (auto& callback : pending_connects.callbacks) {
      callback(false);
    }
  }
}

std::optional<VirtualConnection> CastPlatformClient::GetPlatformConnection(
    std::string_view receiver_id) const {
  auto entry = socket_id_by_receiver_id_.find(receiver_id);
  if (entry == socket_id_by_receiver_id_.end()) {
    return std::nullopt;
  }
  return VirtualConnection{sender_id_, kPlatformReceiverId, entry->second};
}

CastPlatformClient::PendingRequests&
CastPlatformClient::GetOrCreatePendingRequests(std::string_view receiver_id) {
  auto it = pending_requests_by_receiver_id_.find(receiver_id);
  if (it != pending_requests_by_receiver_id_.end()) {
    return it->second;
  }
  pending_requests_by_receiver_id_.emplace_back(std::string(receiver_id),
                                                PendingRequests{});
  return pending_requests_by_receiver_id_.back().second;
}

CastPlatformClient::PendingConnect&
CastPlatformClient::GetOrCreatePendingConnect(std::string_view receiver_id) {
  auto it = pending_connects_by_receiver_id_.find(receiver_id);
  if (it != pending_connects_by_receiver_id_.end()) {
    return it->second;
  }
  pending_connects_by_receiver_id_.emplace_back(std::string(receiver_id),
                                                PendingConnect{});
  return pending_connects_by_receiver_id_.back().second;
}

void CastPlatformClient::Connect(std::string_view receiver_id,
                                 ConnectCallback callback) {
  auto virtual_conn = GetPlatformConnection(receiver_id);
  if (!virtual_conn) {
    callback(false);
    return;
  }
  if (virtual_conn_router_->GetConnectionData(*virtual_conn)) {
    callback(true);
    return;
  }

  PendingConnect& pending_connect = GetOrCreatePendingConnect(receiver_id);
  pending_connect.callbacks.push_back(std::move(callback));
  if (pending_connect.callbacks.size() > 1) {
    // A connect is already in flight for this receiver; it will invoke
    // every queued callback (including this one) once it resolves.
    return;
  }

  pending_connect.timeout = std::make_unique<Alarm>(clock_, *task_runner_);
  pending_connect.timeout->ScheduleFromNow(
      [this, receiver_id = std::string(receiver_id)]() {
        OnConnectResult(receiver_id, false);
      },
      kRequestTimeout);
  virtual_conn_router_->OpenRemoteConnection(
      *virtual_conn,
      [this, receiver_id = std::string(receiver_id)](bool success) {
        OnConnectResult(receiver_id, success);
      });
}

bool CastPlatformClient::IsConnected(std::string_view receiver_id) const {
  auto virtual_conn = GetPlatformConnection(receiver_id);
  return virtual_conn &&
         virtual_conn_router_->GetConnectionData(*virtual_conn).has_value();
}

void CastPlatformClient::OnConnectResult(std::string receiver_id,
                                         bool success) {
  auto entry = pending_connects_by_receiver_id_.find(receiver_id);
  if (entry == pending_connects_by_receiver_id_.end()) {
    return;  // Already resolved (e.g. the timeout raced the real reply).
  }
  std::vector<ConnectCallback> callbacks = std::move(entry->second.callbacks);
  pending_connects_by_receiver_id_.erase(entry);
  for (auto& callback : callbacks) {
    callback(success);
  }
}

std::optional<int> CastPlatformClient::RequestAppAvailability(
    std::string_view receiver_id,
    std::string_view app_id,
    AppAvailabilityCallback callback) {
  auto virtual_conn = GetPlatformConnection(receiver_id);
  if (!virtual_conn) {
    callback(app_id, AppAvailabilityResult::kUnknown);
    return std::nullopt;
  }

  int request_id = GetNextRequestId();
  ErrorOr<proto::CastMessage> message =
      CreateAppAvailabilityRequest(sender_id_, request_id, app_id);
  if (!message) {
    callback(app_id, AppAvailabilityResult::kUnknown);
    return std::nullopt;
  }

  PendingRequests& pending_requests = GetOrCreatePendingRequests(receiver_id);
  auto timeout = std::make_unique<Alarm>(clock_, *task_runner_);
  timeout->ScheduleFromNow(
      [this, request_id]() { CancelAppAvailabilityRequest(request_id); },
      kRequestTimeout);
  pending_requests.availability.push_back(
      AvailabilityRequest{request_id, std::string(app_id), std::move(timeout),
                          std::move(callback)});

  virtual_conn_router_->SendEnsuringConnected(
      std::move(*virtual_conn), std::move(message.value()),
      [this, request_id](Error error) {
        if (!error.ok()) {
          CancelAppAvailabilityRequest(request_id);
        }
      });

  return request_id;
}

std::optional<int> CastPlatformClient::LaunchSession(
    std::string_view receiver_id,
    std::string_view app_id,
    const json::Value& app_params,
    LaunchSessionCallback callback) {
  auto virtual_conn = GetPlatformConnection(receiver_id);
  if (!virtual_conn) {
    callback(Error(Error::Code::kItemNotFound, "Unknown receiver"));
    return std::nullopt;
  }

  auto pending_entry = pending_requests_by_receiver_id_.find(receiver_id);
  if (pending_entry != pending_requests_by_receiver_id_.end() &&
      pending_entry->second.launch_request) {
    callback(Error(Error::Code::kItemAlreadyExists,
                   "Launch already in progress for this receiver"));
    return std::nullopt;
  }

  int request_id = GetNextRequestId();
  ErrorOr<proto::CastMessage> message =
      CreateLaunchRequest(sender_id_, request_id, app_id, app_params);
  if (!message) {
    callback(message.error());
    return std::nullopt;
  }

  PendingRequests& pending_requests = GetOrCreatePendingRequests(receiver_id);
  auto timeout = std::make_unique<Alarm>(clock_, *task_runner_);
  timeout->ScheduleFromNow(
      [this, request_id]() { HandleLaunchTimeout(request_id); },
      kRequestTimeout);
  pending_requests.launch_request =
      LaunchRequest{request_id, std::move(timeout), std::move(callback)};

  virtual_conn_router_->SendEnsuringConnected(
      std::move(*virtual_conn), std::move(message.value()),
      [this, request_id](Error error) {
        if (!error.ok()) {
          FailLaunchRequest(request_id, std::move(error));
        }
      });

  return request_id;
}

std::optional<int> CastPlatformClient::StopSession(
    std::string_view receiver_id,
    std::string_view session_id,
    StopSessionCallback callback) {
  auto virtual_conn = GetPlatformConnection(receiver_id);
  if (!virtual_conn) {
    callback(Error(Error::Code::kItemNotFound, "Unknown receiver"));
    return std::nullopt;
  }

  auto pending_entry = pending_requests_by_receiver_id_.find(receiver_id);
  if (pending_entry != pending_requests_by_receiver_id_.end() &&
      pending_entry->second.stop_request) {
    callback(Error(Error::Code::kItemAlreadyExists,
                   "Stop already in progress for this receiver"));
    return std::nullopt;
  }

  int request_id = GetNextRequestId();
  ErrorOr<proto::CastMessage> message =
      CreateStopRequest(sender_id_, request_id, session_id);
  if (!message) {
    callback(message.error());
    return std::nullopt;
  }

  PendingRequests& pending_requests = GetOrCreatePendingRequests(receiver_id);
  auto timeout = std::make_unique<Alarm>(clock_, *task_runner_);
  timeout->ScheduleFromNow(
      [this, request_id]() { HandleStopTimeout(request_id); }, kRequestTimeout);
  pending_requests.stop_request =
      StopRequest{request_id, std::string(session_id), std::move(timeout),
                  std::move(callback)};

  virtual_conn_router_->SendEnsuringConnected(
      std::move(*virtual_conn), std::move(message.value()),
      [this, request_id](Error error) {
        if (!error.ok()) {
          FailStopRequest(request_id, std::move(error));
        }
      });

  return request_id;
}

void CastPlatformClient::AddOrUpdateReceiver(const ReceiverInfo& receiver,
                                             int socket_id) {
  auto it = socket_id_by_receiver_id_.find(receiver.unique_id);
  if (it != socket_id_by_receiver_id_.end()) {
    it->second = socket_id;
  } else {
    socket_id_by_receiver_id_.emplace_back(receiver.unique_id, socket_id);
  }
}

void CastPlatformClient::RemoveReceiver(const ReceiverInfo& receiver) {
  auto pending_requests_it =
      pending_requests_by_receiver_id_.find(receiver.unique_id);
  if (pending_requests_it != pending_requests_by_receiver_id_.end()) {
    for (const AvailabilityRequest& availability :
         pending_requests_it->second.availability) {
      availability.callback(availability.app_id,
                            AppAvailabilityResult::kUnknown);
    }
    if (pending_requests_it->second.launch_request) {
      pending_requests_it->second.launch_request->callback(
          Error(Error::Code::kOperationCancelled, "Receiver disconnected"));
    }
    if (pending_requests_it->second.stop_request) {
      pending_requests_it->second.stop_request->callback(
          Error(Error::Code::kOperationCancelled, "Receiver disconnected"));
    }
    pending_requests_by_receiver_id_.erase(pending_requests_it);
  }

  // Cancel any in-flight Connect() for this receiver before forgetting its
  // socket mapping below.
  if (pending_connects_by_receiver_id_.contains(receiver.unique_id)) {
    auto virtual_conn = GetPlatformConnection(receiver.unique_id);
    if (virtual_conn) {
      virtual_conn_router_->CloseRemoteConnection(*virtual_conn);
    }
  }
  auto pending_connects_it =
      pending_connects_by_receiver_id_.find(receiver.unique_id);
  if (pending_connects_it != pending_connects_by_receiver_id_.end()) {
    std::vector<ConnectCallback> callbacks =
        std::move(pending_connects_it->second.callbacks);
    pending_connects_by_receiver_id_.erase(pending_connects_it);
    for (auto& callback : callbacks) {
      callback(false);
    }
  }

  socket_id_by_receiver_id_.erase_key(receiver.unique_id);
}

// Note: unlike a timeout or a receiver disconnect, an explicit cancellation
// does not invoke the request's callback. The caller already knows it asked
// for the request to go away, so there's no error to report and calling back
// into code that may no longer expect it could be surprising.
void CastPlatformClient::CancelRequest(int request_id) {
  for (auto entry = pending_requests_by_receiver_id_.begin();
       entry != pending_requests_by_receiver_id_.end(); ++entry) {
    auto& pending_requests = entry->second;
    auto avail_it = std::ranges::find(pending_requests.availability, request_id,
                                      &AvailabilityRequest::request_id);
    if (avail_it != pending_requests.availability.end()) {
      pending_requests.availability.erase(avail_it);
      return;
    }
    if (pending_requests.launch_request &&
        pending_requests.launch_request->request_id == request_id) {
      pending_requests.launch_request.reset();
      return;
    }
    if (pending_requests.stop_request &&
        pending_requests.stop_request->request_id == request_id) {
      pending_requests.stop_request.reset();
      return;
    }
  }
}

void CastPlatformClient::OnMessage(VirtualConnectionRouter* router,
                                   CastSocket* socket,
                                   proto::CastMessage message) {
  if (message.payload_type() != proto::CastMessage_PayloadType_STRING ||
      message.namespace_() != kReceiverNamespace ||
      message.source_id() != kPlatformReceiverId) {
    return;
  }
  ErrorOr<json::Value> dict_or_error = json::Parse(GetPayload(message));
  if (dict_or_error.is_error() || !dict_or_error.value().isObject()) {
    return;
  }

  json::Value& dict = dict_or_error.value();
  std::optional<int> request_id =
      MaybeGetInt(dict, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyRequestId));
  if (!request_id) {
    // A successful LAUNCH_STATUS response echoes the original request id as
    // `launchRequestId` rather than `requestId` (see
    // ApplicationAgent::HandleLaunch() on the receiver side).
    request_id = MaybeGetInt(
        dict, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyLaunchRequestId));
  }
  if (request_id) {
    auto socket_map_entry =
        std::ranges::find(socket_id_by_receiver_id_, ToCastSocketId(socket),
                          [](const auto& entry) { return entry.second; });
    if (socket_map_entry != socket_id_by_receiver_id_.end()) {
      HandleResponse(socket_map_entry->first, request_id.value(), dict);
    }
  }
}

void CastPlatformClient::HandleResponse(std::string_view receiver_id,
                                        int request_id,
                                        const json::Value& message) {
  auto entry = pending_requests_by_receiver_id_.find(receiver_id);
  if (entry == pending_requests_by_receiver_id_.end()) {
    return;
  }
  PendingRequests& pending_requests = entry->second;
  if (HandleAppAvailabilityResponse(pending_requests, request_id, message)) {
    return;
  }
  if (HandleLaunchResponse(pending_requests, request_id, message)) {
    return;
  }
  HandleStopResponse(pending_requests, request_id, message);
}

bool CastPlatformClient::HandleAppAvailabilityResponse(
    PendingRequests& pending_requests,
    int request_id,
    const json::Value& message) {
  auto it = std::ranges::find(pending_requests.availability, request_id,
                              &AvailabilityRequest::request_id);
  if (it == pending_requests.availability.end()) {
    return false;
  }
  // TODO(btolsch): Can all of this manual parsing/checking be cleaned up into
  // a single parsing API along with other message handling?
  const json::Value* maybe_availability =
      message.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyAvailability));
  if (maybe_availability && maybe_availability->isObject()) {
    std::optional<std::string_view> result =
        MaybeGetString(*maybe_availability, &it->app_id[0],
                       &it->app_id[0] + it->app_id.size());
    if (result) {
      AppAvailabilityResult availability_result =
          AppAvailabilityResult::kUnknown;
      if (result.value() == kMessageValueAppAvailable) {
        availability_result = AppAvailabilityResult::kAvailable;
      } else if (result.value() == kMessageValueAppUnavailable) {
        availability_result = AppAvailabilityResult::kUnavailable;
      } else {
        OSP_VLOG << "Invalid availability result: " << result.value();
      }
      it->callback(it->app_id, availability_result);
    }
  }
  pending_requests.availability.erase(it);
  return true;
}

bool CastPlatformClient::HandleLaunchResponse(PendingRequests& pending_requests,
                                              int request_id,
                                              const json::Value& message) {
  if (!pending_requests.launch_request ||
      pending_requests.launch_request->request_id != request_id) {
    return false;
  }
  LaunchRequest& launch = *pending_requests.launch_request;
  // Some receivers confirm LAUNCH with a LAUNCH_STATUS reply, followed by a
  // separate RECEIVER_STATUS that also echoes the same request id; other
  // receivers skip LAUNCH_STATUS entirely and confirm success with a single
  // RECEIVER_STATUS whose `requestId` already matches this LAUNCH request
  // instead. The third branch below accepts either case.
  if (HasType(message, CastMessageType::kLaunchStatus)) {
    // LAUNCH_STATUS carries no `applications` list regardless of whether
    // `status` is USER_PENDING_AUTHORIZATION or USER_ALLOWED. Keep waiting for
    // the RECEIVER_STATUS that actually carries the launched session's details,
    // or the timeout.
  } else if (HasType(message, CastMessageType::kLaunchError)) {
    std::optional<std::string_view> extended_error = MaybeGetString(
        message, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyExtendedError));
    std::optional<std::string_view> reason = MaybeGetString(
        message, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyReason));
    std::string_view error_message =
        (extended_error == kMessageValueUserNotAllowed ||
         extended_error == kMessageValueNotificationDisabled)
            ? *extended_error
            : reason.value_or("Launch failed");
    launch.callback(
        Error(Error::Code::kUnknownError, std::string(error_message)));
    pending_requests.launch_request.reset();
  } else if (HasType(message, CastMessageType::kReceiverStatus)) {
    const json::Value* maybe_status =
        message.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyStatus));
    if (maybe_status && maybe_status->isObject()) {
      std::optional<ReceiverStatus> receiver_status =
          ParseReceiverStatus(*maybe_status);
      if (receiver_status) {
        launch.callback(*std::move(receiver_status));
      } else {
        launch.callback(Error(Error::Code::kUnknownError,
                              "Malformed RECEIVER_STATUS for launched "
                              "session"));
      }
      pending_requests.launch_request.reset();
    }
  }
  return true;
}

bool CastPlatformClient::HandleStopResponse(PendingRequests& pending_requests,
                                            int request_id,
                                            const json::Value& message) {
  if (!pending_requests.stop_request ||
      pending_requests.stop_request->request_id != request_id) {
    return false;
  }
  StopRequest& stop = *pending_requests.stop_request;
  if (HasType(message, CastMessageType::kReceiverStatus)) {
    if (IsSessionStillRunning(message, stop.session_id)) {
      // The target session is still listed as running, so this status
      // update doesn't confirm the stop -- keep waiting for a later message
      // or the timeout, rather than resolving prematurely.
      return true;
    }
    stop.callback(Error::None());
    pending_requests.stop_request.reset();
    return true;
  }
  // Any other response types are treated as a failure.
  std::optional<std::string_view> reason = MaybeGetString(
      message, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyReason));
  stop.callback(Error(Error::Code::kParameterInvalid,
                      std::string(reason.value_or("Stop failed"))));
  pending_requests.stop_request.reset();
  return true;
}

void CastPlatformClient::FailLaunchRequest(int request_id, Error error) {
  for (auto& entry : pending_requests_by_receiver_id_) {
    PendingRequests& pending_requests = entry.second;
    if (pending_requests.launch_request &&
        pending_requests.launch_request->request_id == request_id) {
      pending_requests.launch_request->callback(std::move(error));
      pending_requests.launch_request.reset();
      return;
    }
  }
}

void CastPlatformClient::FailStopRequest(int request_id, Error error) {
  for (auto& entry : pending_requests_by_receiver_id_) {
    PendingRequests& pending_requests = entry.second;
    if (pending_requests.stop_request &&
        pending_requests.stop_request->request_id == request_id) {
      pending_requests.stop_request->callback(std::move(error));
      pending_requests.stop_request.reset();
      return;
    }
  }
}

void CastPlatformClient::HandleLaunchTimeout(int request_id) {
  FailLaunchRequest(
      request_id, Error(Error::Code::kOperationCancelled, "Launch timed out"));
}

void CastPlatformClient::HandleStopTimeout(int request_id) {
  // Silence does not imply success (see StopSession()'s comment.)
  FailStopRequest(request_id,
                  Error(Error::Code::kOperationCancelled, "Stop timed out"));
}

void CastPlatformClient::CancelAppAvailabilityRequest(int request_id) {
  for (auto& entry : pending_requests_by_receiver_id_) {
    PendingRequests& pending_requests = entry.second;
    auto it = std::ranges::find(pending_requests.availability, request_id,
                                &AvailabilityRequest::request_id);
    if (it != pending_requests.availability.end()) {
      it->callback(it->app_id, AppAvailabilityResult::kUnknown);
      pending_requests.availability.erase(it);
    }
  }
}

// static
int CastPlatformClient::GetNextRequestId() {
  return next_request_id_++;
}

// static
// Starts at 1: `requestId: 0` is reserved for unsolicited RECEIVER_STATUS
// broadcasts.
int CastPlatformClient::next_request_id_ = 1;

}  // namespace openscreen::cast
