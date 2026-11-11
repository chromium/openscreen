// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_SENDER_LOOPING_FILE_CAST_AGENT_H_
#define CAST_STANDALONE_SENDER_LOOPING_FILE_CAST_AGENT_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cast/common/channel/cast_message_handler.h"
#include "cast/common/channel/cast_socket_message_port.h"
#include "cast/common/channel/connection_namespace_handler.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "cast/common/public/cast_socket.h"
#include "cast/common/public/trust_store.h"
#include "cast/sender/public/sender_socket_factory.h"
#include "cast/standalone_sender/connection_settings.h"
#include "cast/standalone_sender/file_sender.h"
#include "cast/standalone_sender/remoting_sender.h"
#include "cast/streaming/public/environment.h"
#include "cast/streaming/public/sender_session.h"
#include "platform/base/error.h"
#include "platform/base/interface_info.h"
#include "platform/impl/task_runner.h"
#include "util/alarm.h"
#include "util/scoped_wake_lock.h"

namespace Json {
class Value;
}

namespace openscreen::cast {

// A single-use sender-side Cast Agent that manages the workflow for a mirroring
// session, casting the content from a local file indefinitely. After being
// constructed and having its Connect() method called, the LoopingFileCastAgent
// steps through the following workflow:
//
//   1. Waits for a CastSocket representing a successful connection to a remote
//      Cast Receiver's agent.
//   2. Sends a LAUNCH request to the Cast Receiver to start its Mirroring App.
//   3. Waits for a RECEIVER_STATUS message from the Receiver indicating launch
//      success, or a LAUNCH_ERROR.
//   4. Once launched, message routing (i.e., a VirtualConnection) is requested,
//      for messaging between the SenderSession (locally) and the remote
//      Mirroring App.
//   5. Once message routing is established, the local SenderSession is created
//      and begins the mirroring-specific OFFER/ANSWER messaging to negotiate
//      the streaming parameters.
//   6. Streaming commences.
//
// If at any point an error occurs, the LoopingFileCastAgent executes a clean
// shut-down (both locally, and with the remote Cast Receiver), and then invokes
// the ShutdownCallback that was passed to the constructor.
//
// Normal shutdown happens when either:
//
//   1. Receiver-side, the Mirroring App is shut down. This is discovered via
//      a RECEIVER_STATUS message and torn down locally immediately -- there is
//      nothing left to ask the receiver to do.
//   2. Sender-side, RequestStop() is called. This sends a STOP request to the
//      Cast Receiver and gives it a bounded opportunity to confirm (also via a
//      RECEIVER_STATUS message, handled the same way as case 1) before tearing
//      down the connection locally. Callers that want to be sure the receiver
//      was told to stop (e.g. before exiting the process) should call
//      RequestStop() and wait for the ShutdownCallback, rather than simply
//      destroying the agent.
//   3. This LoopingFileCastAgent is destroyed without a prior call to
//      RequestStop(). This is a safety net: it makes a best-effort, immediate
//      attempt to notify the Cast Receiver, but -- since destruction cannot
//      wait around for a reply -- it does not guarantee the STOP request
//      reached the receiver.
//
// In all cases, the ShutdownCallback passed to the constructor is invoked once
// local teardown has completed.
class LoopingFileCastAgent final
    : public SenderSocketFactory::Client,
      public VirtualConnectionRouter::SocketErrorHandler,
      public ConnectionNamespaceHandler::VirtualConnectionPolicy,
      public CastMessageHandler,
      public SenderSession::Client,
      public SenderStatsClient,
      public RemotingSender::Client {
 public:
  using ShutdownCallback = std::function<void()>;
  using EnvironmentFactory =
      std::function<std::unique_ptr<Environment>(ClockNowFunctionPtr,
                                                 TaskRunner&,
                                                 const IPEndpoint&)>;

  // `shutdown_callback` is invoked after normal shutdown, whether initiated
  // sender- or receiver-side; or, for any fatal error. `environment_factory`
  // may be provided to inject a custom `Environment` (e.g. packet-loss
  // simulation in tests); when empty, a default `Environment` is constructed.
  LoopingFileCastAgent(TaskRunner& task_runner,
                       std::unique_ptr<TrustStore> cast_trust_store,
                       ShutdownCallback shutdown_callback,
                       EnvironmentFactory environment_factory = {});
  ~LoopingFileCastAgent();

  // Connect to a Cast Receiver, and start the workflow to establish a
  // mirroring session.
  void Connect(ConnectionSettings settings);

  // Asks the Cast Receiver to stop the current mirroring session (if any),
  // and disconnects. If a session is active, this sends a STOP request and
  // gives the receiver a bounded opportunity to confirm it (via a subsequent
  // RECEIVER_STATUS showing the app is no longer running) before the
  // connection is forcibly torn down via Shutdown(); otherwise it calls
  // Shutdown() immediately, since there is nothing to ask the receiver to do.
  // `shutdown_callback` is invoked once Shutdown() completes. Safe to call
  // multiple times.
  void RequestStop();

  // Returns the monotonic clock timestamp at which the first media frame is
  // scheduled to be captured, or std::nullopt if streaming has not started yet.
  std::optional<Clock::time_point> GetStreamingStartTime() const;

 private:
  // SenderSocketFactory::Client overrides.
  void OnConnected(SenderSocketFactory* factory,
                   const IPEndpoint& endpoint,
                   std::unique_ptr<CastSocket> socket) override;
  void OnError(SenderSocketFactory* factory,
               const IPEndpoint& endpoint,
               const Error& error) override;

  // VirtualConnectionRouter::SocketErrorHandler overrides.
  void OnClose(CastSocket* cast_socket) override;
  void OnError(CastSocket* socket, const Error& error) override;

  // ConnectionNamespaceHandler::VirtualConnectionPolicy overrides.
  bool IsConnectionAllowed(
      const VirtualConnection& virtual_conn) const override;

  // CastMessageHandler overrides.
  void OnMessage(VirtualConnectionRouter* router,
                 CastSocket* socket,
                 proto::CastMessage message) override;

  // RemotingSender::Client overrides.
  void OnReady() override;
  void OnPlaybackRateChange(double rate) override;

  // Returns the Cast application ID for either audio+video Cast Streaming or
  // audio-only streaming, as configured by the ConnectionSettings.
  const char* GetStreamingAppId() const;

  // Called by OnMessage() to determine whether the Cast Receiver has launched
  // or unlaunched the Mirroring App. If the former, a VirtualConnection is
  // requested. Otherwise, the workflow is aborted and Shutdown() is called.
  void HandleReceiverStatus(const Json::Value& status);

  // Called by the `connection_handler_` after message routing to the Cast
  // Receiver's Mirroring App has been established (if `success` is true).
  void OnRemoteMessagingOpened(bool success);

  // Called by the `connection_handler_` after message routing to the Cast
  // Receiver platform has been established (if `success` is true).
  void OnReceiverMessagingOpened(bool success);

  // Once we have a connection to the receiver we need to create and start
  // a sender session. This method results in the OFFER/ANSWER exchange
  // being completed and a session should be started.
  void CreateAndStartSession();

  // SenderSession::Client overrides.
  void OnNegotiated(const SenderSession* session,
                    SenderSession::ConfiguredSenders senders,
                    capture_recommendations::Recommendations
                        capture_recommendations) override;
  void OnError(const SenderSession* session, const Error& error) override;

  // Input message handler.
  void OnInputMessage(InputMessage message);

  // SenderStatsClient overrides.
  void OnStatisticsUpdated(const SenderStats& updated_stats) override;

  // Starts the file sender. This may occur when mirroring or remoting is
  // "ready" if the session is already negotiated, or upon session negotiation
  // if the receiver is already ready.
  void StartFileSender();

  // Tears down the local session/connections/socket and invokes
  // `shutdown_callback_`. Does not send anything to the Cast Receiver --
  // callers that want to ask the receiver to stop should call RequestStop()
  // instead, which sends the STOP request and then calls this once it's
  // confirmed or has timed out. Idempotent, and safe to call re-entrantly
  // (e.g. it re-enters itself via CloseSocket() synchronously invoking
  // OnClose()).
  void Shutdown();

  // Member variables set as part of construction.
  TaskRunner& task_runner_;
  ShutdownCallback shutdown_callback_;
  EnvironmentFactory environment_factory_;
  VirtualConnectionRouter router_;
  ConnectionNamespaceHandler connection_handler_;
  SenderSocketFactory socket_factory_;
  std::unique_ptr<TlsConnectionFactory> connection_factory_;
  CastSocketMessagePort message_port_;

  // Counter for distinguishing request messages sent to the Cast Receiver.
  int next_request_id_ = 1;

  // Initialized by Connect().
  std::optional<ConnectionSettings> connection_settings_;
  ScopedWakeLockPtr wake_lock_;

  // If non-empty, this is the sessionId associated with the Cast Receiver
  // application that this LoopingFileCastAgent launched.
  std::string app_session_id_;

  // This is set once LoopingFileCastAgent has requested to start messaging to
  // the mirroring app on a Cast Receiver.
  std::optional<VirtualConnection> remote_connection_;
  std::optional<VirtualConnection> platform_remote_connection_;

  CastMode cast_mode_ = CastMode::kMirroring;

  // Member variables set while a streaming to the mirroring app on a Cast
  // Receiver.
  std::unique_ptr<Environment> environment_;
  std::unique_ptr<SenderSession> current_session_;
  std::unique_ptr<FileSender> file_sender_;

  // Remoting specific member variables.
  std::unique_ptr<RemotingSender> remoting_sender_;

  // Set when remoting is successfully negotiated. However, remoting streams
  // won't start until `is_ready_for_remoting_` is true.
  std::unique_ptr<SenderSession::ConfiguredSenders> current_negotiation_;

  // Set to true once we have gotten news that the mirroring application has
  // been launched at least once.
  bool has_launched_ = false;

  // Set to true when the remoting receiver is ready.  However, remoting streams
  // won't start until remoting is successfully negotiated.
  bool is_ready_for_remoting_ = false;

  // Used to not spam the console with statistic update messages.
  int num_times_on_statistics_updated_called_ = 0;

  // Last reported statistics, logged as part of shutdown.
  std::optional<SenderStats> last_reported_statistics_;

  // Set to true the first time RequestStop() does anything meaningful (sends
  // STOP and arms `stop_ack_timeout_`, or falls through to call Shutdown()
  // directly). Guards against a second, independent call to RequestStop() --
  // e.g. from two different error paths firing in close succession -- seeing
  // `app_session_id_` already cleared by the first call and falling through
  // to Shutdown() immediately, which would close the socket before the first
  // call's STOP message has had a chance to actually reach the wire.
  bool stop_requested_ = false;

  // Governs the bounded wait, started by RequestStop(), for the Cast
  // Receiver to confirm a STOP request before Shutdown() forcibly tears down
  // the connection. Scoped to this object's lifetime: if this is destroyed
  // while the timeout is still pending, it is automatically canceled.
  Alarm stop_ack_timeout_;
};

}  // namespace openscreen::cast

#endif  // CAST_STANDALONE_SENDER_LOOPING_FILE_CAST_AGENT_H_
