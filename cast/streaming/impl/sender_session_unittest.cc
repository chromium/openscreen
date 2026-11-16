// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/public/sender_session.h"

#include <cstdio>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "cast/streaming/capture_configs.h"
#include "cast/streaming/impl/message_constants.h"
#include "cast/streaming/input.pb.h"
#include "cast/streaming/public/capture_recommendations.h"
#include "cast/streaming/public/data_channel.h"
#include "cast/streaming/public/data_session.h"
#include "cast/streaming/public/protobuf_messenger.h"
#include "cast/streaming/public/receiver_message.h"
#include "cast/streaming/testing/mock_environment.h"
#include "cast/streaming/testing/simple_message_port.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/web_transport.h"
#include "platform/base/ip_address.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "util/chrono_helpers.h"
#include "util/no_destructor.h"
#include "util/weak_ptr.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace openscreen::cast {

namespace {
constexpr char kMalformedAnswerMessage[] = R"({
  "type": "ANSWER",
  "seqNum": 1,
  "answer": {
    "castMode": "mirroring",
    "udpPort": 1234,
    "sendIndexes": [1, 3],
    "ssrcs": [1, 2]
})";

constexpr char kValidJsonInvalidFormatAnswerMessage[] = R"({
  "type": "ANSWER",
  "seqNum": 1,
  "answer-2": {
    "castMode": "mirroring",
    "udpPort": 1234,
    "sendIndexes": [1, 3],
    "ssrcs": [1, 2]
  }
})";

constexpr char kValidJsonInvalidAnswerMessage[] = R"({
  "type": "ANSWER",
  "seqNum": 1,
  "answer": {
    "castMode": "mirroring",
    "udpPort": -1234,
    "sendIndexes": [1, 3],
    "ssrcs": [1, 2]
  }
})";

constexpr char kMissingAnswerMessage[] = R"({
  "type": "ANSWER",
  "seqNum": 1
})";

constexpr char kInvalidSequenceNumberMessage[] = R"({
  "type": "ANSWER",
  "seqNum": "not actually a number"
})";

constexpr char kUnknownTypeMessage[] = R"({
  "type": "ANSWER_VERSION_2",
  "seqNum": 1
})";

constexpr char kInvalidTypeMessage[] = R"({
  "type": 39,
  "seqNum": 1
})";

constexpr char kInvalidTypeMessageWithNoSeqNum[] = R"({
  "type": 39
})";

constexpr char kErrorAnswerMessage[] = R"({
  "seqNum": 1,
  "type": "ANSWER",
  "result": "error",
  "error": {
    "code": 123,
    "description": "something bad happened"
  }
})";

constexpr char kCapabilitiesResponse[] = R"({
  "seqNum": 1,
  "result": "ok",
  "type": "CAPABILITIES_RESPONSE",
  "capabilities": {
    "mediaCaps": ["video", "vp8", "audio", "aac"]
  }
})";

constexpr char kCapabilitiesErrorResponse[] = R"({
  "seqNum": 2,
  "type": "CAPABILITIES_RESPONSE",
  "result": "error",
  "error": {
    "code": 123,
    "description": "something bad happened"
  }
})";

constexpr char kInputMessage[] = R"({
  "seqNum": 1,
  "type": "INPUT",
  "input": "CGQQnBiCGQgSAggMGgIIBg=="
})";

const AudioCaptureConfig& GetAudioCaptureConfigInvalidChannels() {
  static const NoDestructor<AudioCaptureConfig>
      kAudioCaptureConfigInvalidChannels(AudioCodec::kAac, -1 /* channels */,
                                         44000 /* bit_rate */,
                                         96000 /* sample_rate */);
  return *kAudioCaptureConfigInvalidChannels;
}

const AudioCaptureConfig& GetAudioCaptureConfigValid() {
  static const NoDestructor<AudioCaptureConfig> kAudioCaptureConfigValid(
      AudioCodec::kAac, 5 /* channels */, 32000 /* bit_rate */,
      44000 /* sample_rate */, std::chrono::milliseconds(300), "mp4a.40.5");
  return *kAudioCaptureConfigValid;
}

const VideoCaptureConfig& GetVideoCaptureConfigMissingResolutions() {
  static const NoDestructor<VideoCaptureConfig>
      kVideoCaptureConfigMissingResolutions(
          VideoCodec::kHevc, SimpleFraction{60, 1}, 300000 /* max_bit_rate */,
          std::vector<Resolution>{}, std::chrono::milliseconds(500),
          "hev1.1.6.L150.B0");
  return *kVideoCaptureConfigMissingResolutions;
}

const VideoCaptureConfig& GetVideoCaptureConfigInvalid() {
  static const NoDestructor<VideoCaptureConfig> kVideoCaptureConfigInvalid(
      VideoCodec::kHevc, SimpleFraction{60, 1}, -300000 /* max_bit_rate */,
      std::vector<Resolution>{Resolution{1920, 1080}, Resolution{1280, 720}});
  return *kVideoCaptureConfigInvalid;
}

const VideoCaptureConfig& GetVideoCaptureConfigValid() {
  static const NoDestructor<VideoCaptureConfig> kVideoCaptureConfigValid(
      VideoCodec::kHevc, SimpleFraction{60, 1}, 300000 /* max_bit_rate */,
      std::vector<Resolution>{Resolution{1280, 720}, Resolution{1920, 1080}},
      std::chrono::milliseconds(250), "hev1.1.6.L150.B0");
  return *kVideoCaptureConfigValid;
}

[[maybe_unused]] const VideoCaptureConfig&
GetVideoCaptureConfigValidSimplest() {
  static const NoDestructor<VideoCaptureConfig>
      kVideoCaptureConfigValidSimplest(
          VideoCodec::kHevc, SimpleFraction{60, 1}, 300000 /* max_bit_rate */,
          std::vector<Resolution>{Resolution{1920, 1080}});
  return *kVideoCaptureConfigValidSimplest;
}

class FakeClient : public SenderSession::Client {
 public:
  MOCK_METHOD(void,
              OnNegotiated,
              (const SenderSession*,
               SenderSession::ConfiguredSenders,
               capture_recommendations::Recommendations),
              (override));
  MOCK_METHOD(void,
              OnCapabilitiesDetermined,
              (const SenderSession*, RemotingCapabilities),
              (override));
  MOCK_METHOD(void,
              OnDataSessionOpened,
              (const SenderSession*, DataSession&),
              (override));
  MOCK_METHOD(void,
              OnDataSessionClosed,
              (const SenderSession*, const Error&),
              (override));
  MOCK_METHOD(void,
              OnError,
              (const SenderSession*, const Error& error),
              (override));
};

class FakeWebTransportStream : public WebTransportStream {
 public:
  void SetDelegate(Delegate* delegate) override { delegate_ = delegate; }
  bool Write(ByteView data) override { return true; }
  void Close() override {}

 private:
  raw_ptr<Delegate> delegate_ = nullptr;
};

class FakeWebTransportSession : public WebTransportSession {
 public:
  void SetDelegate(Delegate* delegate) override { delegate_ = delegate; }

  ErrorOr<std::unique_ptr<WebTransportStream>> CreateOutgoingStream() override {
    return std::unique_ptr<WebTransportStream>(
        std::make_unique<FakeWebTransportStream>());
  }

  void Close(const Error& error) override { SimulateClosed(error); }

  // Like a real transport, keeps using its own state after notifying the
  // delegate, so destroying this session from inside the notification is
  // caught by ASAN.
  void SimulateClosed(const Error& error) {
    if (delegate_) {
      delegate_->OnSessionClosed(this, error);
    }
    ++close_notification_count_;
  }

  WeakPtr<FakeWebTransportSession> GetWeakPtr() const {
    return weak_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<Delegate> delegate_ = nullptr;
  int close_notification_count_ = 0;
  WeakPtrFactory<FakeWebTransportSession> weak_factory_{this};
};

// Completes Connect() synchronously, with either a new session or
// `connect_error_`.
class FakeWebTransportClient : public WebTransportClient {
 public:
  void Connect(std::string_view url,
               const WebTransportOptions& options,
               ConnectCallback callback) override {
    last_url_ = std::string(url);
    last_options_ = options;
    if (connect_error_.has_value()) {
      std::move(callback)(connect_error_.value());
      return;
    }
    auto session = std::make_unique<FakeWebTransportSession>();
    last_session_ = session->GetWeakPtr();
    std::move(callback)(
        std::unique_ptr<WebTransportSession>(std::move(session)));
  }

  WeakPtr<FakeWebTransportClient> GetWeakPtr() const {
    return weak_factory_.GetWeakPtr();
  }

  std::string last_url_;
  WebTransportOptions last_options_;
  std::optional<Error> connect_error_;
  WeakPtr<FakeWebTransportSession> last_session_;

 private:
  WeakPtrFactory<FakeWebTransportClient> weak_factory_{this};
};

// Runs `on_closed_`, if set, when the channel reports that it closed.
class FakeDataChannelDelegate : public DataChannel::Delegate {
 public:
  void OnMessage(ByteView data) override {}
  void OnStateChange(DataChannel::State state) override {
    if (state == DataChannel::State::kClosed && on_closed_) {
      on_closed_();
    }
  }
  void OnWriteReady() override {}

  std::function<void()> on_closed_;
};

// TODO(jophba): this matcher is likely more generally useful and should
// be refactored.
MATCHER_P(CodeEquals, code, "Checks error codes but not messages.") {
  return arg.code() == code;
}

}  // namespace

class SenderSessionTest : public ::testing::Test {
 public:
  SenderSessionTest() : clock_(Clock::time_point{}), task_runner_(clock_) {}

  std::unique_ptr<MockEnvironment> MakeEnvironment() {
    auto environment = std::make_unique<NiceMock<MockEnvironment>>(
        &FakeClock::now, task_runner_);
    ON_CALL(*environment, GetBoundLocalEndpoint())
        .WillByDefault(
            Return(IPEndpoint{IPAddress::Parse("127.0.0.1").value(), 12345}));
    return environment;
  }

  void SetUp() {
    message_port_ = std::make_unique<SimpleMessagePort>("receiver-12345");
    environment_ = MakeEnvironment();

    SenderSession::Configuration config{IPAddress::kV4LoopbackAddress(),
                                        client_,
                                        environment_.get(),
                                        message_port_.get(),
                                        "sender-12345",
                                        "receiver-12345",
                                        /* use_android_rtp_hack */ true,
                                        /* use_dscp */ true};
    session_ = std::make_unique<SenderSession>(std::move(config));
  }

  void NegotiateMirroringWithValidConfigs() {
    const Error error = session_->Negotiate(
        std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
        std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});
    ASSERT_TRUE(error.ok());
  }

  void NegotiateRemotingWithValidConfigs() {
    const Error error = session_->NegotiateRemoting(
        GetAudioCaptureConfigValid(), GetVideoCaptureConfigValid());
    ASSERT_TRUE(error.ok());
  }

  // Recreates `session_` with DataChannel support backed by a
  // FakeWebTransportClient. Each client the session creates is exposed through
  // `web_transport_client_` and fails to connect with `connect_error`, if set.
  void CreateSessionWithDataChannels(
      std::optional<Error> connect_error = std::nullopt) {
    SenderSession::Configuration config{IPAddress::kV4LoopbackAddress(),
                                        client_,
                                        environment_.get(),
                                        message_port_.get(),
                                        "sender-12345",
                                        "receiver-12345",
                                        /* use_android_rtp_hack */ true,
                                        /* use_dscp */ true};
    config.create_web_transport_client = [this, connect_error](TaskRunner&) {
      auto client = std::make_unique<FakeWebTransportClient>();
      client->connect_error_ = connect_error;
      web_transport_client_ = client->GetWeakPtr();
      return client;
    };
    session_.reset();
    session_ = std::make_unique<SenderSession>(std::move(config));
  }

  // Answers require specific fields from the original offer to be valid. The
  // answer responds to the most recently sent OFFER.
  std::string ConstructAnswerFromOffer(CastMode mode,
                                       bool with_data_transport = false) {
    const auto& messages = message_port_->posted_messages();
    if (messages.empty()) {
      return {};
    }
    auto message_body = json::Parse(messages.back());
    if (message_body.is_error()) {
      return {};
    }
    const json::Value offer = std::move(message_body.value());
    EXPECT_EQ("OFFER", offer["type"].asString());
    EXPECT_LT(0, offer["seqNum"].asInt());

    const json::Value& offer_body = offer["offer"];
    if (!offer_body.isObject()) {
      return {};
    }

    const json::Value& streams = offer_body["supportedStreams"];
    EXPECT_TRUE(streams.isArray());
    EXPECT_EQ(2u, streams.size());

    const json::Value& audio_stream = streams[0];
    const int audio_index = audio_stream["index"].asInt();
    const int audio_ssrc = audio_stream["ssrc"].asUInt();

    const json::Value& video_stream = streams[1];
    const int video_index = video_stream["index"].asInt();
    const int video_ssrc = video_stream["ssrc"].asUInt();

    static constexpr char kAnswerTemplate[] = R"({{
        "type": "ANSWER",
        "seqNum": {},
        "result": "ok",
        "answer": {{
          "castMode": "{}",
          "udpPort": 1234,
          "sendIndexes": [{}, {}],
          "ssrcs": [{}, {}]{}
        }}
  }})";
    static constexpr char kDataTransport[] = R"(,
          "dataTransport": {
            "protocol": "webtransport",
            "port": 33534,
            "certificateFingerprint":
                "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXoxMjM0NTY="
          })";
    return std::format(kAnswerTemplate, offer["seqNum"].asInt(),
                       mode == CastMode::kMirroring ? "mirroring" : "remoting",
                       audio_index, video_index, audio_ssrc + 1, video_ssrc + 1,
                       with_data_transport ? kDataTransport : "");
  }

  // Negotiates mirroring and answers with a data transport, expecting the
  // data session to be opened. If `channel` is set, a channel is created
  // on the data session and stored there.
  void NegotiateWithDataSession(
      std::unique_ptr<DataChannel>* channel = nullptr) {
    NegotiateMirroringWithValidConfigs();
    const std::string answer =
        ConstructAnswerFromOffer(CastMode::kMirroring, true);
    EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _));
    EXPECT_CALL(client_, OnDataSessionOpened(session_.get(), _))
        .WillOnce([channel](const SenderSession*, DataSession& data_session) {
          if (channel) {
            auto channel_or_error = data_session.CreateDataChannel("label");
            ASSERT_TRUE(channel_or_error.is_value());
            *channel = std::move(channel_or_error.value());
          }
        });
    message_port_->ReceiveMessage(answer);
    task_runner_.RunTasksUntilIdle();
    ::testing::Mock::VerifyAndClearExpectations(&client_);
    ASSERT_TRUE(web_transport_client_);
    ASSERT_TRUE(web_transport_client_->last_session_);
  }

 protected:
  StrictMock<FakeClient> client_;
  FakeClock clock_;
  std::unique_ptr<MockEnvironment> environment_;
  std::unique_ptr<SimpleMessagePort> message_port_;
  std::unique_ptr<SenderSession> session_;
  FakeTaskRunner task_runner_;
  WeakPtr<FakeWebTransportClient> web_transport_client_;
};

TEST_F(SenderSessionTest, ConfiguresUdpBufferSizes) {
  auto message_port = std::make_unique<SimpleMessagePort>("receiver-12345");
  auto environment = MakeEnvironment();

  EXPECT_CALL(*environment, SetReceiveBufferSize(1024u));
  EXPECT_CALL(*environment, SetSendBufferSize(2048u));

  SenderSession::Configuration config{IPAddress::kV4LoopbackAddress(),
                                      client_,
                                      environment.get(),
                                      message_port.get(),
                                      "sender-12345",
                                      "receiver-12345",
                                      /* use_android_rtp_hack */ true,
                                      /* use_dscp */ true};
  config.udp_receive_buffer_size = 1024;
  config.udp_send_buffer_size = 2048;
  auto session = std::make_unique<SenderSession>(std::move(config));
}

TEST_F(SenderSessionTest, ComplainsIfNoConfigsToOffer) {
  const Error error = session_->Negotiate(std::vector<AudioCaptureConfig>{},
                                          std::vector<VideoCaptureConfig>{});

  EXPECT_EQ(error,
            Error(Error::Code::kParameterInvalid,
                  "Need at least one audio or video config to negotiate."));
}

TEST_F(SenderSessionTest, ComplainsIfInvalidAudioCaptureConfig) {
  const Error error = session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigInvalidChannels()},
      std::vector<VideoCaptureConfig>{});

  EXPECT_EQ(error,
            Error(Error::Code::kParameterInvalid, "Invalid configs provided."));
}

TEST_F(SenderSessionTest, ComplainsIfInvalidVideoCaptureConfig) {
  const Error error = session_->Negotiate(
      std::vector<AudioCaptureConfig>{},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigInvalid()});
  EXPECT_EQ(error,
            Error(Error::Code::kParameterInvalid, "Invalid configs provided."));
}

TEST_F(SenderSessionTest, ComplainsIfMissingResolutions) {
  const Error error =
      session_->Negotiate(std::vector<AudioCaptureConfig>{},
                          std::vector<VideoCaptureConfig>{
                              GetVideoCaptureConfigMissingResolutions()});
  EXPECT_EQ(error,
            Error(Error::Code::kParameterInvalid, "Invalid configs provided."));
}

TEST_F(SenderSessionTest, SendsOfferWithZeroBitrateOptions) {
  VideoCaptureConfig video_config = GetVideoCaptureConfigValid();
  video_config.max_bit_rate = 0;
  AudioCaptureConfig audio_config = GetAudioCaptureConfigValid();
  audio_config.bit_rate = 0;

  const Error error =
      session_->Negotiate(std::vector<AudioCaptureConfig>{audio_config},
                          std::vector<VideoCaptureConfig>{video_config});
  EXPECT_TRUE(error.ok());

  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  const json::Value offer = std::move(message_body.value());
  EXPECT_EQ("OFFER", offer["type"].asString());
}

TEST_F(SenderSessionTest, SendsOfferWithSimpleVideoOnly) {
  const Error error = session_->Negotiate(
      std::vector<AudioCaptureConfig>{},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});
  EXPECT_TRUE(error.ok());

  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  const json::Value offer = std::move(message_body.value());
  EXPECT_EQ("OFFER", offer["type"].asString());
}

TEST_F(SenderSessionTest, SendsOfferAudioOnly) {
  const Error error = session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{});
  EXPECT_TRUE(error.ok());

  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  const json::Value offer = std::move(message_body.value());
  EXPECT_EQ("OFFER", offer["type"].asString());
}

TEST_F(SenderSessionTest, SendsOfferMessage) {
  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());

  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  const json::Value offer = std::move(message_body.value());
  EXPECT_EQ("OFFER", offer["type"].asString());
  EXPECT_LT(0, offer["seqNum"].asInt());

  const json::Value& offer_body = offer["offer"];
  ASSERT_FALSE(offer_body.isNull());
  ASSERT_TRUE(offer_body.isObject());
  EXPECT_EQ("mirroring", offer_body["castMode"].asString());

  const json::Value& streams = offer_body["supportedStreams"];
  EXPECT_TRUE(streams.isArray());
  EXPECT_EQ(2u, streams.size());

  const json::Value& audio_stream = streams[0];
  EXPECT_EQ("aac", audio_stream["codecName"].asString());
  EXPECT_EQ(0, audio_stream["index"].asInt());
  EXPECT_EQ(32u, audio_stream["aesKey"].asString().length());
  EXPECT_EQ(32u, audio_stream["aesIvMask"].asString().length());
  EXPECT_EQ(5, audio_stream["channels"].asInt());
  EXPECT_LT(0u, audio_stream["ssrc"].asUInt());
  EXPECT_EQ(127, audio_stream["rtpPayloadType"].asInt());
  EXPECT_EQ("mp4a.40.5", audio_stream["codecParameter"].asString());

  const json::Value& video_stream = streams[1];
  EXPECT_EQ("hevc", video_stream["codecName"].asString());
  EXPECT_EQ(1, video_stream["index"].asInt());
  EXPECT_EQ(32u, video_stream["aesKey"].asString().length());
  EXPECT_EQ(32u, video_stream["aesIvMask"].asString().length());
  EXPECT_EQ(1, video_stream["channels"].asInt());
  EXPECT_LT(0u, video_stream["ssrc"].asUInt());
  EXPECT_EQ(96, video_stream["rtpPayloadType"].asInt());
  EXPECT_EQ("hev1.1.6.L150.B0", video_stream["codecParameter"].asString());
}

TEST_F(SenderSessionTest, HandlesValidAnswer) {
  NegotiateMirroringWithValidConfigs();
  std::string answer = ConstructAnswerFromOffer(CastMode::kMirroring);

  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _));
  message_port_->ReceiveMessage(answer);
}

TEST_F(SenderSessionTest, HandlesStreamTypes) {
  NegotiateMirroringWithValidConfigs();
  std::string answer = ConstructAnswerFromOffer(CastMode::kMirroring);

  // Valid and successful negotiations should result in senders that are
  // configured with the proper StreamType.
  ON_CALL(client_, OnNegotiated(session_.get(), _, _))
      .WillByDefault(
          [&](const SenderSession* sender_session,
              SenderSession::ConfiguredSenders senders,
              capture_recommendations::Recommendations recommendations) {
            StreamType audio_stream_type =
                senders.audio_sender->config().stream_type;
            StreamType video_stream_type =
                senders.video_sender->config().stream_type;

            EXPECT_EQ(audio_stream_type, StreamType::kAudio);
            EXPECT_EQ(video_stream_type, StreamType::kVideo);
          });
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _));
  message_port_->ReceiveMessage(answer);
}

TEST_F(SenderSessionTest, HandlesInvalidNamespace) {
  NegotiateMirroringWithValidConfigs();
  std::string answer = ConstructAnswerFromOffer(CastMode::kMirroring);
  message_port_->ReceiveMessage("random-namespace", answer);
}

TEST_F(SenderSessionTest, HandlesMalformedAnswer) {
  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  // Note that unlike when we simply don't select any streams, when the answer
  // is actually malformed we have no way of knowing it was an answer at all,
  // so we report an Error and drop the message.
  EXPECT_CALL(client_, OnError(session_.get(), _));
  message_port_->ReceiveMessage(kMalformedAnswerMessage);
}

TEST_F(SenderSessionTest, HandlesImproperlyFormattedAnswer) {
  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  EXPECT_CALL(client_,
              OnError(session_.get(), CodeEquals(Error::Code::kInvalidAnswer)));
  message_port_->ReceiveMessage(kValidJsonInvalidFormatAnswerMessage);
}

TEST_F(SenderSessionTest, HandlesInvalidAnswer) {
  const Error error = session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  EXPECT_CALL(client_,
              OnError(session_.get(), CodeEquals(Error::Code::kInvalidAnswer)));
  message_port_->ReceiveMessage(kValidJsonInvalidAnswerMessage);
}

TEST_F(SenderSessionTest, HandlesNullAnswer) {
  const Error error = session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  EXPECT_TRUE(error.ok());
  EXPECT_CALL(client_, OnError(session_.get(), _));
  message_port_->ReceiveMessage(kMissingAnswerMessage);
}

TEST_F(SenderSessionTest, HandlesAnswerTimeout) {
  const Error error = session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});
  EXPECT_TRUE(error.ok());

  // No ANSWER received in time, should report an error.
  EXPECT_CALL(client_,
              OnError(session_.get(), CodeEquals(Error::Code::kAnswerTimeout)));
  clock_.Advance(std::chrono::seconds(10));
}

TEST_F(SenderSessionTest, HandlesCapabilitiesTimeout) {
  const Error error = session_->RequestCapabilities();
  EXPECT_TRUE(error.ok());

  // No CAPABILITIES_RESPONSE received in time, should report an error.
  EXPECT_CALL(client_, OnError(session_.get(),
                               CodeEquals(Error::Code::kRemotingNotSupported)));
  clock_.Advance(std::chrono::seconds(10));
}

TEST_F(SenderSessionTest, HandlesInvalidSequenceNumber) {
  const Error error = session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  // We should just discard messages with an invalid sequence number.
  message_port_->ReceiveMessage(kInvalidSequenceNumberMessage);
}

TEST_F(SenderSessionTest, HandlesUnknownTypeMessageWithValidSeqNum) {
  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  // If a message is of unknown type but has an expected seqnum, it's
  // probably a malformed response.
  EXPECT_CALL(client_, OnError(session_.get(), _));
  message_port_->ReceiveMessage(kUnknownTypeMessage);
}

TEST_F(SenderSessionTest, HandlesInvalidTypeMessageWithValidSeqNum) {
  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  // If a message is of unknown type but has an expected seqnum, it's
  // probably a malformed response. The sender session will end up
  // handling this message as an ANSWER to the OFFER with a sequence
  // number of 1.
  EXPECT_CALL(client_,
              OnError(session_.get(), CodeEquals(Error::Code::kInvalidAnswer)));
  message_port_->ReceiveMessage(kInvalidTypeMessage);
}

TEST_F(SenderSessionTest, HandlesInvalidTypeMessage) {
  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  // We should just discard messages with an invalid message type and
  // no sequence number.
  message_port_->ReceiveMessage(kInvalidTypeMessageWithNoSeqNum);
}

TEST_F(SenderSessionTest, HandlesErrorMessage) {
  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  // We should report error responses. NOTE: according to the spec,
  // RPC messages should never be an error result.
  EXPECT_CALL(client_,
              OnError(session_.get(), CodeEquals(Error::Code::kInvalidAnswer)));
  message_port_->ReceiveMessage(kErrorAnswerMessage);

  session_->RequestCapabilities();
  EXPECT_CALL(client_, OnError(session_.get(),
                               CodeEquals(Error::Code::kRemotingNotSupported)));
  message_port_->ReceiveMessage(kCapabilitiesErrorResponse);
}

TEST_F(SenderSessionTest, NormalizesOpenscreenAnswerErrorsToInvalidAnswer) {
  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  // Open Screen error codes in an ANSWER (such as kRemotingNotSupported or
  // kParameterInvalid) should be reported as kInvalidAnswer.
  const std::string remoting_rejected_answer = std::format(
      R"({{
        "seqNum": 1,
        "type": "ANSWER",
        "result": "error",
        "error": {{
          "code": {},
          "description": "remoting rejected"
        }}
      }})",
      ReceiverError::kOpenscreenErrorOffset +
          static_cast<int>(Error::Code::kRemotingNotSupported));

  EXPECT_CALL(client_,
              OnError(session_.get(), CodeEquals(Error::Code::kInvalidAnswer)));
  message_port_->ReceiveMessage(remoting_rejected_answer);

  // kNoStreamSelected in an ANSWER should be preserved.
  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  const std::string no_stream_answer = std::format(
      R"({{
        "seqNum": 2,
        "type": "ANSWER",
        "result": "error",
        "error": {{
          "code": {},
          "description": "no streams matched"
        }}
      }})",
      ReceiverError::kOpenscreenErrorOffset +
          static_cast<int>(Error::Code::kNoStreamSelected));

  EXPECT_CALL(client_, OnError(session_.get(),
                               CodeEquals(Error::Code::kNoStreamSelected)));
  message_port_->ReceiveMessage(no_stream_answer);
}

TEST_F(SenderSessionTest, HandlesMessagePortError) {
  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  // We should report message port errors.
  EXPECT_CALL(client_, OnError(session_.get(), _));
  message_port_->ReceiveError(Error(Error::Code::kUnknownError));
}

TEST_F(SenderSessionTest, ReportsZeroBandwidthWhenNoPacketsSent) {
  // TODO(issuetracker.google.com/183996645): As part of end to end testing,
  // we need to ensure that we are providing reasonable network bandwidth
  // measurements.
  EXPECT_EQ(0, session_->GetEstimatedNetworkBandwidth());
}

TEST_F(SenderSessionTest, ComplainsIfInvalidAudioCaptureConfigRemoting) {
  const Error error = session_->NegotiateRemoting(
      GetAudioCaptureConfigInvalidChannels(), GetVideoCaptureConfigValid());

  EXPECT_EQ(error.code(), Error::Code::kParameterInvalid);
}

TEST_F(SenderSessionTest, ComplainsIfInvalidVideoCaptureConfigRemoting) {
  const Error error = session_->NegotiateRemoting(
      GetAudioCaptureConfigValid(), GetVideoCaptureConfigInvalid());
  EXPECT_EQ(error.code(), Error::Code::kParameterInvalid);
}

TEST_F(SenderSessionTest, ComplainsIfMissingResolutionsRemoting) {
  const Error error = session_->NegotiateRemoting(
      GetAudioCaptureConfigValid(), GetVideoCaptureConfigMissingResolutions());
  EXPECT_EQ(error.code(), Error::Code::kParameterInvalid);
}

TEST_F(SenderSessionTest, HandlesValidAnswerRemoting) {
  NegotiateRemotingWithValidConfigs();
  std::string answer = ConstructAnswerFromOffer(CastMode::kRemoting);

  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _));
  message_port_->ReceiveMessage(answer);
}

TEST_F(SenderSessionTest, SuccessfulRemotingNegotiationYieldsValidObject) {
  NegotiateRemotingWithValidConfigs();
  std::string answer = ConstructAnswerFromOffer(CastMode::kRemoting);

  SenderSession::ConfiguredSenders senders;
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _))
      .WillOnce(
          [&senders](const SenderSession* session,
                     SenderSession::ConfiguredSenders arg0,
                     capture_recommendations::Recommendations recommendations) {
            senders = std::move(arg0);
          });

  message_port_->ReceiveMessage(answer);

  EXPECT_NE(nullptr, senders.audio_sender);
  EXPECT_EQ(senders.audio_config.codec, AudioCodec::kAac);
  EXPECT_NE(nullptr, senders.video_sender);
  EXPECT_EQ(senders.video_config.codec, VideoCodec::kHevc);
}

TEST_F(SenderSessionTest, RemotingNegotiationPopulatesCaptureRecommendations) {
  NegotiateRemotingWithValidConfigs();
  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  const json::Value& offer = message_body.value();
  const json::Value& streams = offer["offer"]["supportedStreams"];
  const int audio_index = streams[0]["index"].asInt();
  const int audio_ssrc = streams[0]["ssrc"].asUInt();
  const int video_index = streams[1]["index"].asInt();
  const int video_ssrc = streams[1]["ssrc"].asUInt();

  const std::string answer = std::format(
      R"({{
        "type": "ANSWER",
        "seqNum": {},
        "result": "ok",
        "answer": {{
          "castMode": "remoting",
          "udpPort": 1234,
          "sendIndexes": [{}, {}],
          "ssrcs": [{}, {}],
          "display": {{
            "dimensions": {{
              "width": 3840,
              "height": 2160,
              "frameRate": "60"
            }}
          }}
        }}
      }})",
      offer["seqNum"].asInt(), audio_index, video_index, audio_ssrc + 1,
      video_ssrc + 1);

  capture_recommendations::Recommendations received_recommendations{};
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _))
      .WillOnce([&received_recommendations](
                    const SenderSession* session,
                    SenderSession::ConfiguredSenders senders,
                    capture_recommendations::Recommendations recommendations) {
        received_recommendations = recommendations;
      });

  message_port_->ReceiveMessage(answer);
  EXPECT_EQ(received_recommendations.video.maximum.width, 3840);
  EXPECT_EQ(received_recommendations.video.maximum.height, 2160);
}

TEST_F(SenderSessionTest, SuccessfulGetCapabilitiesRequest) {
  session_->RequestCapabilities();

  RemotingCapabilities capabilities;
  EXPECT_CALL(client_, OnCapabilitiesDetermined(session_.get(), _))
      .WillOnce(testing::SaveArg<1>(&capabilities));
  message_port_->ReceiveMessage(kCapabilitiesResponse);

  // The capabilities should match the values in `kCapabilitiesResponse`.
  EXPECT_THAT(capabilities.audio,
              testing::ElementsAre(AudioCapability::kBaselineSet,
                                   AudioCapability::kAac));

  // The "video" capability is ignored since it means nothing.
  EXPECT_THAT(capabilities.video, testing::ElementsAre(VideoCapability::kVp8));
}

TEST_F(SenderSessionTest, SendsOfferWithoutDscp) {
  // Tear down the default session before attempting to do a custom one here.
  session_.reset();
  message_port_ = std::make_unique<SimpleMessagePort>("receiver-12345");
  environment_ = MakeEnvironment();

  SenderSession::Configuration config{
      IPAddress::kV4LoopbackAddress(),
      client_,
      environment_.get(),
      message_port_.get(),
      "sender-12345",
      "receiver-12345",
      true,  // use_android_rtp_hack
      false  // enable_dscp
  };
  session_ = std::make_unique<SenderSession>(std::move(config));

  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());

  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  const json::Value offer = std::move(message_body.value());
  const json::Value& offer_body = offer["offer"];
  const json::Value& streams = offer_body["supportedStreams"];
  EXPECT_TRUE(streams.isArray());
  EXPECT_EQ(2u, streams.size());

  const json::Value& audio_stream = streams[0];
  EXPECT_TRUE(audio_stream["receiverRtcpDscp"].isNull());

  const json::Value& video_stream = streams[1];
  EXPECT_TRUE(video_stream["receiverRtcpDscp"].isNull());
}

TEST_F(SenderSessionTest, SendsOfferWithDscp) {
  // Tear down the default session before attempting to do a custom one here.
  session_.reset();
  message_port_ = std::make_unique<SimpleMessagePort>("receiver-12345");
  environment_ = MakeEnvironment();

  SenderSession::Configuration config{
      IPAddress::kV4LoopbackAddress(),
      client_,
      environment_.get(),
      message_port_.get(),
      "sender-12345",
      "receiver-12345",
      true,  // use_android_rtp_hack
      true   // enable_dscp
  };
  session_ = std::make_unique<SenderSession>(std::move(config));

  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());

  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  const json::Value offer = std::move(message_body.value());
  const json::Value& offer_body = offer["offer"];
  const json::Value& streams = offer_body["supportedStreams"];
  EXPECT_TRUE(streams.isArray());
  EXPECT_EQ(2u, streams.size());

  const json::Value& audio_stream = streams[0];
  EXPECT_EQ(static_cast<int>(UdpSocket::DscpMode::kAF41),
            audio_stream["receiverRtcpDscp"].asInt());

  const json::Value& video_stream = streams[1];
  EXPECT_EQ(static_cast<int>(UdpSocket::DscpMode::kAF41),
            video_stream["receiverRtcpDscp"].asInt());
}

TEST_F(SenderSessionTest, InputEventsOptIn) {
  // Opt-in using SetInputCallback.
  session_->SetInputCallback([](InputMessage message) {});

  session_->Negotiate(
      std::vector<AudioCaptureConfig>{GetAudioCaptureConfigValid()},
      std::vector<VideoCaptureConfig>{GetVideoCaptureConfigValid()});

  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  const json::Value& offer = message_body.value()["offer"];

  // Verify input_events extension is present in the offer.
  bool found_extension = false;
  for (const auto& stream : offer["supportedStreams"]) {
    for (const auto& ext : stream["rtpExtensions"]) {
      if (ext.asString() == kInputEventsRtpExtension) {
        found_extension = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_extension);
}

TEST_F(SenderSessionTest, HandlesSetInputCallbackUpdate) {
  session_->SetInputCallback([](InputMessage message) {});
}

TEST_F(SenderSessionTest, HandlesSetInputCallbackNull) {
  session_->SetInputCallback([](InputMessage message) {});

  session_->SetInputCallback(nullptr);
}

TEST_F(SenderSessionTest, HandlesInputMessage) {
  NegotiateMirroringWithValidConfigs();
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _));
  message_port_->ReceiveMessage(ConstructAnswerFromOffer(CastMode::kMirroring));

  bool called = false;
  session_->SetInputCallback(
      [&called](InputMessage message) { called = true; });
  message_port_->ReceiveMessage(kInputMessage);
  EXPECT_TRUE(called);
}

TEST_F(SenderSessionTest, SendsInputMessage) {
  NegotiateMirroringWithValidConfigs();
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _));
  message_port_->ReceiveMessage(ConstructAnswerFromOffer(CastMode::kMirroring));

  session_->SetInputCallback([](InputMessage message) {});

  InputMessage message;
  auto* event = message.add_events();
  event->set_type(InputMessage::INPUT_TYPE_KEY_DOWN);

  message_port_->clear();
  session_->SendInputMessage(message);

  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  EXPECT_EQ("INPUT", message_body.value()["type"].asString());
  EXPECT_FALSE(message_body.value()["input"].asString().empty());
}

TEST_F(SenderSessionTest, HandlesValidAnswerWithDataChannel) {
  CreateSessionWithDataChannels();
  NegotiateMirroringWithValidConfigs();

  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  const json::Value& data_transport =
      message_body.value()["offer"]["dataTransport"];
  ASSERT_TRUE(data_transport.isObject());
  EXPECT_EQ("webtransport", data_transport["protocol"].asString());

  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _));
  message_port_->ReceiveMessage(
      ConstructAnswerFromOffer(CastMode::kMirroring, true));

  ASSERT_TRUE(web_transport_client_);
  EXPECT_EQ("https://127.0.0.1:33534/cast-mirroring",
            web_transport_client_->last_url_);
  ASSERT_EQ(
      1u,
      web_transport_client_->last_options_.server_certificate_hashes.size());
  EXPECT_EQ("YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXoxMjM0NTY=",
            web_transport_client_->last_options_.server_certificate_hashes[0]);

  // The client is told about the data session from a separate task, even
  // though the fake client connects synchronously, and must be able to create
  // channels as soon as it is told.
  std::unique_ptr<DataChannel> channel;
  EXPECT_CALL(client_, OnDataSessionOpened(session_.get(), _))
      .WillOnce([&](const SenderSession*, DataSession& data_session) {
        auto channel_or_error = data_session.CreateDataChannel("test-label");
        ASSERT_TRUE(channel_or_error.is_value());
        channel = std::move(channel_or_error.value());
      });
  task_runner_.RunTasksUntilIdle();
  ASSERT_NE(nullptr, channel);
  EXPECT_EQ("test-label", channel->label());
}

TEST_F(SenderSessionTest, HandlesDataSessionGracefulClosure) {
  CreateSessionWithDataChannels();
  NegotiateWithDataSession();

  // A graceful close is reported through OnDataSessionClosed() only, not as
  // an error.
  EXPECT_CALL(client_, OnDataSessionClosed(session_.get(),
                                           CodeEquals(Error::Code::kNone)));
  web_transport_client_->last_session_->SimulateClosed(Error::None());
  task_runner_.RunTasksUntilIdle();

  EXPECT_FALSE(web_transport_client_);
}

TEST_F(SenderSessionTest, HandlesDataSessionErrorClosureWithoutOnError) {
  CreateSessionWithDataChannels();
  NegotiateWithDataSession();

  // An error close is reported through OnDataSessionClosed() only, not
  // OnError().
  EXPECT_CALL(client_,
              OnDataSessionClosed(session_.get(),
                                  CodeEquals(Error::Code::kConnectionFailed)));
  EXPECT_CALL(client_, OnError(_, _)).Times(0);
  web_transport_client_->last_session_->SimulateClosed(
      Error(Error::Code::kConnectionFailed, "Transport connection reset"));
  task_runner_.RunTasksUntilIdle();

  EXPECT_FALSE(web_transport_client_);
}

TEST_F(SenderSessionTest, HandlesDataSessionConnectSynchronousFailure) {
  CreateSessionWithDataChannels(
      Error(Error::Code::kSocketConnectFailure, "Sync connect failure"));
  NegotiateMirroringWithValidConfigs();

  // OnDataSessionOpened() is never called for a failed connection, and the
  // failure is reported through OnDataSessionClosed() only, not OnError().
  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _));
  EXPECT_CALL(client_, OnDataSessionClosed(
                           session_.get(),
                           CodeEquals(Error::Code::kSocketConnectFailure)));
  EXPECT_CALL(client_, OnError(_, _)).Times(0);
  message_port_->ReceiveMessage(
      ConstructAnswerFromOffer(CastMode::kMirroring, true));
  task_runner_.RunTasksUntilIdle();

  EXPECT_FALSE(web_transport_client_);
}

TEST_F(SenderSessionTest, NegotiateFromOnDataSessionClosedWithErrorIsSafe) {
  CreateSessionWithDataChannels();
  NegotiateWithDataSession();
  WeakPtr<FakeWebTransportSession> old_session =
      web_transport_client_->last_session_;

  // Renegotiating after a data session failure must not destroy the data
  // session while it is still reporting the closure.
  EXPECT_CALL(client_,
              OnDataSessionClosed(session_.get(),
                                  CodeEquals(Error::Code::kConnectionFailed)))
      .WillOnce([this](const SenderSession*, const Error&) {
        NegotiateMirroringWithValidConfigs();
      });
  old_session->SimulateClosed(
      Error(Error::Code::kConnectionFailed, "Transport connection reset"));
  task_runner_.RunTasksUntilIdle();
  EXPECT_FALSE(old_session);
  ASSERT_EQ(2u, message_port_->posted_messages().size());

  // The new negotiation gets its own data session.
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _));
  EXPECT_CALL(client_, OnDataSessionOpened(session_.get(), _))
      .WillOnce([](const SenderSession*, DataSession& data_session) {
        EXPECT_TRUE(data_session.CreateDataChannel("test-label").is_value());
      });
  message_port_->ReceiveMessage(
      ConstructAnswerFromOffer(CastMode::kMirroring, true));
  task_runner_.RunTasksUntilIdle();
}

TEST_F(SenderSessionTest, NegotiateFromOnDataSessionClosedIsSafe) {
  CreateSessionWithDataChannels();
  NegotiateWithDataSession();
  WeakPtr<FakeWebTransportSession> old_session =
      web_transport_client_->last_session_;

  EXPECT_CALL(client_, OnDataSessionClosed(session_.get(),
                                           CodeEquals(Error::Code::kNone)))
      .WillOnce([this](const SenderSession*, const Error&) {
        NegotiateMirroringWithValidConfigs();
      });
  old_session->SimulateClosed(Error::None());
  task_runner_.RunTasksUntilIdle();
  EXPECT_FALSE(old_session);
  EXPECT_EQ(2u, message_port_->posted_messages().size());
}

TEST_F(SenderSessionTest, DestroyFromOnDataSessionClosedIsSafe) {
  CreateSessionWithDataChannels();
  NegotiateWithDataSession();
  WeakPtr<FakeWebTransportSession> old_session =
      web_transport_client_->last_session_;

  EXPECT_CALL(client_,
              OnDataSessionClosed(session_.get(),
                                  CodeEquals(Error::Code::kConnectionFailed)))
      .WillOnce(
          [this](const SenderSession*, const Error&) { session_.reset(); });
  old_session->SimulateClosed(
      Error(Error::Code::kConnectionFailed, "Transport connection reset"));
  task_runner_.RunTasksUntilIdle();
  EXPECT_FALSE(session_);
  EXPECT_FALSE(old_session);
}

TEST_F(SenderSessionTest, NegotiateFromDataChannelCloseIsSafe) {
  CreateSessionWithDataChannels();
  // Declared before `channel` so that it outlives it.
  FakeDataChannelDelegate delegate;
  std::unique_ptr<DataChannel> channel;
  NegotiateWithDataSession(&channel);
  ASSERT_TRUE(channel);
  WeakPtr<FakeWebTransportSession> old_session =
      web_transport_client_->last_session_;
  channel->SetDelegate(&delegate);

  // The channel reports its closure from inside the data session's own
  // closure handling, so renegotiating here must not destroy the data session
  // synchronously. The renegotiation drops the data session, so its closure
  // is not reported.
  delegate.on_closed_ = [this] { NegotiateMirroringWithValidConfigs(); };
  old_session->SimulateClosed(Error::None());
  EXPECT_TRUE(old_session);
  EXPECT_EQ(DataChannel::State::kClosed, channel->state());

  task_runner_.RunTasksUntilIdle();
  EXPECT_FALSE(old_session);
  EXPECT_EQ(2u, message_port_->posted_messages().size());
}

TEST_F(SenderSessionTest, RenegotiationDropsPreviousDataSession) {
  CreateSessionWithDataChannels();
  NegotiateWithDataSession();
  WeakPtr<FakeWebTransportSession> old_session =
      web_transport_client_->last_session_;

  NegotiateMirroringWithValidConfigs();

  // Events from the previous data session are no longer reported, even before
  // it has been destroyed.
  ASSERT_TRUE(old_session);
  old_session->SimulateClosed(
      Error(Error::Code::kConnectionFailed, "Transport connection reset"));
  task_runner_.RunTasksUntilIdle();
  EXPECT_FALSE(old_session);
}

TEST_F(SenderSessionTest, NegotiateFromOnNegotiatedDropsStaleDataSession) {
  CreateSessionWithDataChannels();
  NegotiateMirroringWithValidConfigs();

  // Renegotiating from inside OnNegotiated() supersedes the answer being
  // handled, so its data session must be dropped and never reported.
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _))
      .WillOnce([this](const SenderSession*, SenderSession::ConfiguredSenders,
                       capture_recommendations::Recommendations) {
        NegotiateMirroringWithValidConfigs();
      });
  message_port_->ReceiveMessage(
      ConstructAnswerFromOffer(CastMode::kMirroring, true));
  task_runner_.RunTasksUntilIdle();

  EXPECT_FALSE(web_transport_client_);
  EXPECT_EQ(2u, message_port_->posted_messages().size());
}

TEST_F(SenderSessionTest, OfferOmitsDataTransportWhenDisabled) {
  NegotiateMirroringWithValidConfigs();

  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  EXPECT_TRUE(message_body.value()["offer"]["dataTransport"].isNull());
}

TEST_F(SenderSessionTest, DoesNotOpenDataSessionWhenDisabled) {
  NegotiateMirroringWithValidConfigs();

  // Even if the receiver answers with a data transport, no data session is
  // set up, so the client only sees OnNegotiated().
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _));
  message_port_->ReceiveMessage(
      ConstructAnswerFromOffer(CastMode::kMirroring, true));
  task_runner_.RunTasksUntilIdle();
}

TEST_F(SenderSessionTest, DoesNotOpenDataSessionWithoutClient) {
  SenderSession::Configuration config{IPAddress::kV4LoopbackAddress(),
                                      client_,
                                      environment_.get(),
                                      message_port_.get(),
                                      "sender-12345",
                                      "receiver-12345",
                                      /* use_android_rtp_hack */ true,
                                      /* use_dscp */ true};
  config.create_web_transport_client = [](TaskRunner&) {
    return std::unique_ptr<WebTransportClient>();
  };
  session_.reset();
  session_ = std::make_unique<SenderSession>(std::move(config));
  NegotiateMirroringWithValidConfigs();

  // No WebTransport client could be created, so neither
  // OnDataSessionOpened() nor OnDataSessionClosed() is called.
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _));
  message_port_->ReceiveMessage(
      ConstructAnswerFromOffer(CastMode::kMirroring, true));
  task_runner_.RunTasksUntilIdle();
}

}  // namespace openscreen::cast
