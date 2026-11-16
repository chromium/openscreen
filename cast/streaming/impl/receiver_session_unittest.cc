// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/public/receiver_session.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "cast/streaming/impl/message_constants.h"
#include "cast/streaming/input.pb.h"
#include "cast/streaming/public/data_channel.h"
#include "cast/streaming/public/receiver.h"
#include "cast/streaming/testing/mock_environment.h"
#include "cast/streaming/testing/simple_message_port.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/web_transport.h"
#include "platform/base/ip_address.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "util/chrono_helpers.h"
#include "util/json/json_serialization.h"
#include "util/std_util.h"
#include "util/weak_ptr.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace openscreen::cast {

namespace {

constexpr char kValidOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "supportedStreams": [
      {
        "index": 31337,
        "type": "video_source",
        "codecName": "vp9",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088743,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "bbf109bf84513b456b13a184453b66ce",
        "aesIvMask": "edaf9e4536e2b66191f560d9c04b2a69",
        "receiverRtcpDscp": 46,
        "resolutions": [
          {
            "width": 1280,
            "height": 720
          }
        ]
      },
      {
        "index": 31338,
        "type": "video_source",
        "codecName": "vp8",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088745,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "040d756791711fd3adb939066e6d8690",
        "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed",
        "receiverRtcpDscp": 46,
        "resolutions": [
          {
            "width": 1280,
            "height": 720
          }
        ]
      },
      {
        "index": 31339,
        "type": "video_source",
        "codecName": "hevc",
        "codecParameter": "hev1.1.6.L150.B0",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088746,
        "maxFrameRate": "120",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "aesKey": "040d756791711fd3adb939066e6d8690",
        "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed",
        "receiverRtcpDscp": 46,
        "resolutions": [
          {
            "width": 1920,
            "height": 1080
          }
        ]
      },
      {
        "index": 1337,
        "type": "audio_source",
        "codecName": "opus",
        "rtpProfile": "cast",
        "rtpPayloadType": 97,
        "ssrc": 19088747,
        "bitRate": 124000,
        "timeBase": "1/48000",
        "channels": 2,
        "aesKey": "51027e4e2347cbcb49d57ef10177aebc",
        "aesIvMask": "7f12a19be62a36c04ae4116caaeff6d1",
        "receiverRtcpDscp": 46
      }
    ]
  }
})";

constexpr char kValidRemotingOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 419,
  "offer": {
    "castMode": "remoting",
    "supportedStreams": [
      {
        "index": 31339,
        "type": "video_source",
        "codecName": "REMOTE_VIDEO",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088745,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5432101,
        "aesKey": "040d756791711fd3adb939066e6d8690",
        "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed",
        "resolutions": [
          {
            "width": 1920,
            "height":1080
          }
        ]
      },
      {
        "index": 31340,
        "type": "audio_source",
        "codecName": "REMOTE_AUDIO",
        "rtpProfile": "cast",
        "rtpPayloadType": 97,
        "ssrc": 19088747,
        "bitRate": 125000,
        "timeBase": "1/48000",
        "channels": 2,
        "aesKey": "51027e4e2347cbcb49d57ef10177aebc",
        "aesIvMask": "7f12a19be62a36c04ae4116caaeff6d1"
      }
    ]
  }
})";

constexpr char kNoAudioOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "supportedStreams": [
      {
        "index": 31338,
        "type": "video_source",
        "codecName": "vp8",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088745,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "040d756791711fd3adb939066e6d8690",
        "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed",
        "resolutions": [
          {
            "width": 1280,
            "height": 720
          }
        ]
      }
    ]
  }
})";

// An offer with one audio stream and one video stream that lists several
// resolutions, largest first. The audio stream lets the ANSWER include
// constraints, which need both audio and video limits.
constexpr char kMultiResolutionOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "supportedStreams": [
      {
        "index": 31338,
        "type": "video_source",
        "codecName": "vp8",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088745,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "040d756791711fd3adb939066e6d8690",
        "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed",
        "resolutions": [
          {
            "width": 3840,
            "height": 2160
          },
          {
            "width": 1920,
            "height": 1080
          },
          {
            "width": 1280,
            "height": 720
          }
        ]
      },
      {
        "index": 1337,
        "type": "audio_source",
        "codecName": "opus",
        "rtpProfile": "cast",
        "rtpPayloadType": 97,
        "ssrc": 19088747,
        "bitRate": 124000,
        "timeBase": "1/48000",
        "channels": 2,
        "aesKey": "51027e4e2347cbcb49d57ef10177aebc",
        "aesIvMask": "7f12a19be62a36c04ae4116caaeff6d1"
      }
    ]
  }
})";

// Legacy senders may omit "resolutions" entirely.
constexpr char kNoResolutionsOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "supportedStreams": [
      {
        "index": 31338,
        "type": "video_source",
        "codecName": "vp8",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088745,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "040d756791711fd3adb939066e6d8690",
        "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed"
      }
    ]
  }
})";

constexpr char kInvalidCodecOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "supportedStreams": [
      {
        "index": 31338,
        "type": "video_source",
        "codecName": "vp12",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088745,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "040d756791711fd3adb939066e6d8690",
        "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed",
        "resolutions": [
          {
            "width": 1280,
            "height": 720
          }
        ]
      }
    ]
  }
})";

constexpr char kNoVideoOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "supportedStreams": [
      {
        "index": 1337,
        "type": "audio_source",
        "codecName": "opus",
        "rtpProfile": "cast",
        "rtpPayloadType": 97,
        "ssrc": 19088747,
        "bitRate": 124000,
        "timeBase": "1/48000",
        "channels": 2,
        "aesKey": "51027e4e2347cbcb49d57ef10177aebc",
        "aesIvMask": "7f12a19be62a36c04ae4116caaeff6d1"
      }
    ]
  }
})";

constexpr char kNoAudioOrVideoOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "supportedStreams": []
  }
})";

constexpr char kInvalidJsonOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "supportedStreams": [
  }
})";

constexpr char kMissingMandatoryFieldOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337
})";

constexpr char kMissingSeqNumOfferMessage[] = R"({
  "type": "OFFER",
  "offer": {
    "castMode": "mirroring",
    "supportedStreams": []
  }
})";

constexpr char kValidJsonInvalidFormatOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "supportedStreams": "anything"
  }
})";

constexpr char kNullJsonOfferMessage[] = R"({
  "type": "OFFER",
  "seqNum": 1337
})";

constexpr char kInvalidSequenceNumberMessage[] = R"({
  "type": "OFFER",
  "seqNum": "not actually a number"
})";

constexpr char kUnknownTypeMessage[] = R"({
  "type": "OFFER_VERSION_2",
  "seqNum": 1337
})";

constexpr char kInvalidTypeMessage[] = R"({
  "type": 39,
  "seqNum": 1337
})";

constexpr char kGetCapabilitiesMessage[] = R"({
  "seqNum": 820263770,
  "type": "GET_CAPABILITIES"
})";

constexpr char kRpcMessage[] = R"({
  "rpc" : "CGQQnBiCGQgSAggMGgIIBg==",
  "seqNum" : 2,
  "type" : "RPC"
})";

constexpr char kValidOfferMessageWithInput[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "supportedStreams": [
      {
        "index": 31338,
        "type": "video_source",
        "codecName": "vp8",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088745,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "040d756791711fd3adb939066e6d8690",
        "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed",
        "rtpExtensions": ["input_events"],
        "resolutions": [
          {
            "width": 1280,
            "height": 720
          }
        ]
      }
    ]
  }
})";

constexpr char kInputMessage[] = R"({
  "input" : "CGQQnBiCGQgSAggMGgIIBg==",
  "seqNum" : 3,
  "type" : "INPUT"
})";

class FakeClient : public ReceiverSession::Client {
 public:
  MOCK_METHOD(void,
              OnNegotiated,
              (const ReceiverSession*, ReceiverSession::ConfiguredReceivers),
              (override));
  MOCK_METHOD(void,
              OnRemotingNegotiated,
              (const ReceiverSession*, ReceiverSession::RemotingNegotiation),
              (override));
  MOCK_METHOD(void,
              OnReceiversDestroying,
              (const ReceiverSession*, ReceiversDestroyingReason),
              (override));
  MOCK_METHOD(void,
              OnError,
              (const ReceiverSession*, const Error& error),
              (override));
  MOCK_METHOD(void,
              OnNewDataChannel,
              (const ReceiverSession*, std::unique_ptr<DataChannel> channel),
              (override));
  MOCK_METHOD(void,
              OnDataSessionClosed,
              (const ReceiverSession*, const Error& error),
              (override));
  MOCK_METHOD(bool,
              SupportsCodecParameter,
              (const std::string& parameter),
              (override));
};

void ExpectIsErrorAnswer(const std::string& raw_message) {
  ErrorOr<json::Value> message_or_error = json::Parse(raw_message);
  ASSERT_TRUE(message_or_error.is_value());
  const json::Value message = std::move(message_or_error.value());
  EXPECT_TRUE(message["answer"].isNull());
  EXPECT_EQ("error", message["result"].asString());
  EXPECT_EQ(1337, message["seqNum"].asInt());
  EXPECT_EQ("ANSWER", message["type"].asString());

  const json::Value& error = message["error"];
  EXPECT_TRUE(error.isObject());
  EXPECT_GT(error["code"].asInt(), 0);
}

// Returns the parsed and validated ANSWER message for additional field
// validation.
json::Value ExpectIsValidAnswer(const std::string& raw_message) {
  ErrorOr<json::Value> message_or_error = json::Parse(raw_message);
  EXPECT_TRUE(message_or_error.is_value());
  json::Value message = std::move(message_or_error.value());

  EXPECT_EQ("ANSWER", message["type"].asString());
  EXPECT_EQ("ok", message["result"].asString());
  EXPECT_FALSE(message["answer"].isNull());
  EXPECT_TRUE(message["answer"].isObject());
  return message;
}

MATCHER_P(
    NegotiatedWithSender,
    sender_id,
    "Checks that a set of ConfiguredReceivers are from a specific sender") {
  return sender_id == arg.sender_id;
}

MATCHER_P(CodeEquals, code, "Checks error codes but not messages.") {
  return arg.code() == code;
}

constexpr char kValidOfferMessageWithDataChannel[] = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "supportedStreams": [
      {
        "index": 31338,
        "type": "video_source",
        "codecName": "vp8",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088745,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "040d756791711fd3adb939066e6d8690",
        "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed",
        "receiverRtcpDscp": 46,
        "resolutions": [
          {
            "width": 1280,
            "height": 720
          }
        ]
      },
      {
        "index": 1337,
        "type": "audio_source",
        "codecName": "opus",
        "rtpProfile": "cast",
        "rtpPayloadType": 97,
        "ssrc": 19088747,
        "bitRate": 124000,
        "timeBase": "1/48000",
        "channels": 2,
        "aesKey": "51027e4e2347cbcb49d57ef10177aebc",
        "aesIvMask": "7f12a19be62a36c04ae4116caaeff6d1",
        "receiverRtcpDscp": 46
      }
    ],
    "dataTransport": {
      "protocol": "webtransport"
    }
  }
})";

}  // namespace

class ReceiverSessionTest : public ::testing::Test {
 public:
  ReceiverSessionTest() : clock_(Clock::time_point{}), task_runner_(clock_) {}

  std::unique_ptr<MockEnvironment> MakeEnvironment() {
    auto environment = std::make_unique<NiceMock<MockEnvironment>>(
        &FakeClock::now, task_runner_);
    ON_CALL(*environment, GetBoundLocalEndpoint())
        .WillByDefault(Return(IPEndpoint{{127, 0, 0, 1}, 12345}));
    environment->SetSocketStateForTesting(Environment::SocketState::kReady);
    return environment;
  }

  void SetUp() { SetUpWithConstraints(ReceiverConstraints{}); }

  // Since constraints are constant throughout the life of a session,
  // changing them requires configuring a new session.
  void SetUpWithConstraints(ReceiverConstraints constraints) {
    session_.reset();
    message_port_ = std::make_unique<SimpleMessagePort>("sender-12345");
    environment_ = MakeEnvironment();
    session_ = std::make_unique<ReceiverSession>(
        client_, *environment_, *message_port_, std::move(constraints));
  }

 protected:
  // Constraints for a VP8 receiver advertising a display of the given size.
  static ReceiverConstraints ConstraintsWithDisplay(int width, int height) {
    auto display = std::make_unique<Display>(
        Display{{width, height, {60, 1}}, true /* can scale content */});
    return ReceiverConstraints({VideoCodec::kVp8}, {AudioCodec::kOpus}, {}, {},
                               std::move(display));
  }

  // Negotiates `offer` with `constraints` and returns the video resolutions
  // given to the embedder to set up its decoder.
  std::vector<Resolution> NegotiatedDecodeResolutions(
      ReceiverConstraints constraints,
      const char* offer) {
    ReceiverSession session(client_, *environment_, *message_port_,
                            std::move(constraints));
    std::vector<Resolution> resolutions;
    InSequence s;
    EXPECT_CALL(client_, OnNegotiated(&session, _))
        .WillOnce([&resolutions](const ReceiverSession* session_,
                                 ReceiverSession::ConfiguredReceivers cr) {
          EXPECT_TRUE(cr.video_receiver);
          resolutions = cr.video_config.resolutions;
        });
    EXPECT_CALL(client_, OnReceiversDestroying(
                             &session, ReceiverSession::Client::kEndOfSession));
    message_port_->ReceiveMessage(offer);
    return resolutions;
  }

  void AssertGotAnErrorAnswerResponse() {
    const std::vector<std::string>& messages = message_port_->posted_messages();
    ASSERT_EQ(1u, messages.size());

    ExpectIsErrorAnswer(messages[0]);
  }

  StrictMock<FakeClient> client_;
  FakeClock clock_;
  std::unique_ptr<MockEnvironment> environment_;
  std::unique_ptr<SimpleMessagePort> message_port_;
  std::unique_ptr<ReceiverSession> session_;
  FakeTaskRunner task_runner_;
};

TEST_F(ReceiverSessionTest, CanNegotiateWithDefaultConstraints) {
  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _))
      .WillOnce([](const ReceiverSession* session_,
                   ReceiverSession::ConfiguredReceivers cr) {
        EXPECT_TRUE(cr.audio_receiver);
        EXPECT_EQ(cr.audio_receiver->config().sender_ssrc, 19088747u);
        EXPECT_EQ(cr.audio_receiver->config().receiver_ssrc, 19088748u);
        EXPECT_EQ(cr.audio_receiver->config().channels, 2);
        EXPECT_EQ(cr.audio_receiver->config().rtp_timebase, 48000);

        // We should have chosen opus
        EXPECT_EQ(cr.audio_config.codec, AudioCodec::kOpus);

        EXPECT_TRUE(cr.video_receiver);
        EXPECT_EQ(cr.video_receiver->config().sender_ssrc, 19088745u);
        EXPECT_EQ(cr.video_receiver->config().receiver_ssrc, 19088746u);
        EXPECT_EQ(cr.video_receiver->config().channels, 1);
        EXPECT_EQ(cr.video_receiver->config().rtp_timebase, 90000);

        // We should have chosen vp8
        EXPECT_EQ(cr.video_config.codec, VideoCodec::kVp8);

        // This should be from the default test sender ID.
        EXPECT_EQ("sender-12345", cr.sender_id);
      });
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage(kValidOfferMessage);

  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());

  json::Value message = ExpectIsValidAnswer(messages[0]);

  EXPECT_EQ("ANSWER", message["type"].asString());
  EXPECT_EQ(1337, message["seqNum"].asInt());
  EXPECT_EQ("ok", message["result"].asString());

  const json::Value& answer = message["answer"];
  EXPECT_TRUE(answer.isObject());

  // Spot check the answer body fields. We have more in depth testing
  // of answer behavior in answer_messages_unittest, but here we can
  // ensure that the ReceiverSession properly configured the answer.
  EXPECT_EQ(1337, answer["sendIndexes"][0].asInt());
  EXPECT_EQ(31338, answer["sendIndexes"][1].asInt());
  EXPECT_LT(0, answer["udpPort"].asInt());
  EXPECT_GT(65535, answer["udpPort"].asInt());

  // Constraints and display should not be present with no constraints.
  EXPECT_TRUE(answer["constraints"].isNull());
  EXPECT_TRUE(answer["display"].isNull());
}

TEST_F(ReceiverSessionTest, CanNegotiateWithCustomCodecConstraints) {
  ReceiverSession session(
      client_, *environment_, *message_port_,
      ReceiverConstraints{{VideoCodec::kVp9}, {AudioCodec::kOpus}});

  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(&session, _))
      .WillOnce([](const ReceiverSession* session_,
                   ReceiverSession::ConfiguredReceivers cr) {
        EXPECT_TRUE(cr.audio_receiver);
        EXPECT_EQ(cr.audio_receiver->config().sender_ssrc, 19088747u);
        EXPECT_EQ(cr.audio_receiver->config().receiver_ssrc, 19088748u);
        EXPECT_EQ(cr.audio_receiver->config().channels, 2);
        EXPECT_EQ(cr.audio_receiver->config().rtp_timebase, 48000);
        EXPECT_EQ(cr.audio_config.codec, AudioCodec::kOpus);

        EXPECT_TRUE(cr.video_receiver);
        EXPECT_EQ(cr.video_receiver->config().sender_ssrc, 19088743u);
        EXPECT_EQ(cr.video_receiver->config().receiver_ssrc, 19088744u);
        EXPECT_EQ(cr.video_receiver->config().channels, 1);
        EXPECT_EQ(cr.video_receiver->config().rtp_timebase, 90000);
        EXPECT_EQ(cr.video_config.codec, VideoCodec::kVp9);
      });
  EXPECT_CALL(client_, OnReceiversDestroying(
                           &session, ReceiverSession::Client::kEndOfSession));
  message_port_->ReceiveMessage(kValidOfferMessage);
}

TEST_F(ReceiverSessionTest, RejectsStreamWithUnsupportedCodecParameter) {
  ReceiverConstraints constraints({VideoCodec::kHevc}, {AudioCodec::kOpus});
  EXPECT_CALL(client_, SupportsCodecParameter(_)).WillRepeatedly(Return(false));
  ReceiverSession session(client_, *environment_, *message_port_, constraints);
  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(&session, _))
      .WillOnce([](const ReceiverSession* session_,
                   ReceiverSession::ConfiguredReceivers cr) {
        EXPECT_FALSE(cr.video_receiver);
      });
  EXPECT_CALL(client_, OnReceiversDestroying(
                           &session, ReceiverSession::Client::kEndOfSession));
  message_port_->ReceiveMessage(kValidOfferMessage);
}

TEST_F(ReceiverSessionTest, AcceptsStreamWithNoCodecParameter) {
  ReceiverConstraints constraints({VideoCodec::kHevc, VideoCodec::kVp9},
                                  {AudioCodec::kOpus});
  EXPECT_CALL(client_, SupportsCodecParameter(_)).WillRepeatedly(Return(false));

  ReceiverSession session(client_, *environment_, *message_port_,
                          std::move(constraints));
  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(&session, _))
      .WillOnce([](const ReceiverSession* session_,
                   ReceiverSession::ConfiguredReceivers cr) {
        EXPECT_TRUE(cr.video_receiver);
        EXPECT_EQ(cr.video_config.codec, VideoCodec::kVp9);
      });
  EXPECT_CALL(client_, OnReceiversDestroying(
                           &session, ReceiverSession::Client::kEndOfSession));
  message_port_->ReceiveMessage(kValidOfferMessage);
}

TEST_F(ReceiverSessionTest, AcceptsStreamWithMatchingParameter) {
  ReceiverConstraints constraints({VideoCodec::kHevc}, {AudioCodec::kOpus});
  EXPECT_CALL(client_, SupportsCodecParameter(_))
      .WillRepeatedly(
          [](const std::string& param) { return param == "hev1.1.6.L150.B0"; });

  ReceiverSession session(client_, *environment_, *message_port_,
                          std::move(constraints));
  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(&session, _))
      .WillOnce([](const ReceiverSession* session_,
                   ReceiverSession::ConfiguredReceivers cr) {
        EXPECT_TRUE(cr.video_receiver);
        EXPECT_EQ(cr.video_config.codec, VideoCodec::kHevc);
      });
  EXPECT_CALL(client_, OnReceiversDestroying(
                           &session, ReceiverSession::Client::kEndOfSession));
  message_port_->ReceiveMessage(kValidOfferMessage);
}

TEST_F(ReceiverSessionTest, ConfiguresDecoderWithinDisplayDimensions) {
  const std::vector<Resolution> resolutions = NegotiatedDecodeResolutions(
      ConstraintsWithDisplay(1280, 720), kMultiResolutionOfferMessage);

  ASSERT_EQ(1u, resolutions.size());
  EXPECT_EQ(1280, resolutions[0].width);
  EXPECT_EQ(720, resolutions[0].height);
}

TEST_F(ReceiverSessionTest, SelectsLargestOfferedResolutionWithinDisplay) {
  const std::vector<Resolution> resolutions = NegotiatedDecodeResolutions(
      ConstraintsWithDisplay(1920, 1080), kMultiResolutionOfferMessage);

  ASSERT_EQ(1u, resolutions.size());
  EXPECT_EQ(1920, resolutions[0].width);
  EXPECT_EQ(1080, resolutions[0].height);
}

TEST_F(ReceiverSessionTest, ClampsToDisplayWhenNoOfferedResolutionFits) {
  // No offered resolution fits in 1024x768, so the display size is used.
  const std::vector<Resolution> resolutions = NegotiatedDecodeResolutions(
      ConstraintsWithDisplay(1024, 768), kMultiResolutionOfferMessage);

  ASSERT_EQ(1u, resolutions.size());
  EXPECT_EQ(1024, resolutions[0].width);
  EXPECT_EQ(768, resolutions[0].height);
}

TEST_F(ReceiverSessionTest, UsesDisplayDimensionsWhenOfferHasNoResolutions) {
  const std::vector<Resolution> resolutions = NegotiatedDecodeResolutions(
      ConstraintsWithDisplay(1280, 720), kNoResolutionsOfferMessage);

  ASSERT_EQ(1u, resolutions.size());
  EXPECT_EQ(1280, resolutions[0].width);
  EXPECT_EQ(720, resolutions[0].height);
}

TEST_F(ReceiverSessionTest, RespectsVideoLimitsMaxDimensions) {
  // The ANSWER only has constraints when both audio and video limits are
  // found, so set both.
  std::vector<AudioLimits> audio_limits = {
      {true, AudioCodec::kOpus, 48000, 2, 32000, 256000, milliseconds(1000)}};
  std::vector<VideoLimits> video_limits = {{true,
                                            VideoCodec::kVp8,
                                            62208000,
                                            {1280, 720, {60, 1}},
                                            300000,
                                            90000000,
                                            milliseconds(1000)}};
  const std::vector<Resolution> resolutions = NegotiatedDecodeResolutions(
      ReceiverConstraints({VideoCodec::kVp8}, {AudioCodec::kOpus},
                          std::move(audio_limits), std::move(video_limits),
                          nullptr),
      kMultiResolutionOfferMessage);

  ASSERT_EQ(1u, resolutions.size());
  EXPECT_EQ(1280, resolutions[0].width);
  EXPECT_EQ(720, resolutions[0].height);
}

TEST_F(ReceiverSessionTest, OnlyAppliesLimitsSentInAnswer) {
  // Without audio limits the ANSWER has no constraints, so the sender never
  // sees the 1280x720 video limit and may send up to 1920x1080.
  std::vector<VideoLimits> video_limits = {{true,
                                            VideoCodec::kVp8,
                                            62208000,
                                            {1280, 720, {60, 1}},
                                            300000,
                                            90000000,
                                            milliseconds(1000)}};
  const std::vector<Resolution> resolutions = NegotiatedDecodeResolutions(
      ReceiverConstraints({VideoCodec::kVp8}, {AudioCodec::kOpus}, {},
                          std::move(video_limits), nullptr),
      kMultiResolutionOfferMessage);

  ASSERT_EQ(1u, resolutions.size());
  EXPECT_EQ(1920, resolutions[0].width);
  EXPECT_EQ(1080, resolutions[0].height);
}

TEST_F(ReceiverSessionTest, DefaultsToMaxResolutionWithoutConstraints) {
  // With no display and no limits, the sender is capped at
  // capture_recommendations::kDefaultMaxResolution (1920x1080).
  const std::vector<Resolution> resolutions = NegotiatedDecodeResolutions(
      ReceiverConstraints({VideoCodec::kVp8}, {AudioCodec::kOpus}),
      kMultiResolutionOfferMessage);

  ASSERT_EQ(1u, resolutions.size());
  EXPECT_EQ(1920, resolutions[0].width);
  EXPECT_EQ(1080, resolutions[0].height);
}

TEST_F(ReceiverSessionTest, CanNegotiateWithLimits) {
  std::vector<AudioLimits> audio_limits = {
      {false, AudioCodec::kOpus, 48001, 2, 32001, 32002, milliseconds(3001)}};
  std::vector<VideoLimits> video_limits = {{true,
                                            VideoCodec::kVp9,
                                            62208000,
                                            {1920, 1080, {144, 1}},
                                            300000,
                                            90000000,
                                            milliseconds(1000)}};

  auto display = std::make_unique<Display>(
      Display{{640, 480, {60, 1}}, false /* can scale content */});

  ReceiverSession session(client_, *environment_, *message_port_,
                          ReceiverConstraints{{VideoCodec::kVp9},
                                              {AudioCodec::kOpus},
                                              std::move(audio_limits),
                                              std::move(video_limits),
                                              std::move(display)});

  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(&session, _));
  EXPECT_CALL(client_, OnReceiversDestroying(
                           &session, ReceiverSession::Client::kEndOfSession));
  message_port_->ReceiveMessage(kValidOfferMessage);

  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());

  json::Value message_body = ExpectIsValidAnswer(messages[0]);
  const json::Value& answer_json = message_body["answer"];
  ASSERT_TRUE(answer_json.isObject()) << messages[0];

  // Constraints and display should be valid with valid constraints.
  ASSERT_FALSE(answer_json["constraints"].isNull());
  ASSERT_FALSE(answer_json["display"].isNull());

  const json::Value& display_json = answer_json["display"];
  EXPECT_EQ("60", display_json["dimensions"]["frameRate"].asString());
  EXPECT_EQ(640, display_json["dimensions"]["width"].asInt());
  EXPECT_EQ(480, display_json["dimensions"]["height"].asInt());
  EXPECT_EQ("sender", display_json["scaling"].asString());

  const json::Value& constraints_json = answer_json["constraints"];
  ASSERT_TRUE(constraints_json.isObject());

  const json::Value& audio = constraints_json["audio"];
  ASSERT_TRUE(audio.isObject());
  EXPECT_EQ(32002, audio["maxBitRate"].asInt());
  EXPECT_EQ(2, audio["maxChannels"].asInt());
  EXPECT_EQ(3001, audio["maxDelay"].asInt());
  EXPECT_EQ(48001, audio["maxSampleRate"].asInt());
  EXPECT_EQ(32001, audio["minBitRate"].asInt());

  const json::Value& video = constraints_json["video"];
  ASSERT_TRUE(video.isObject());
  EXPECT_EQ(90000000, video["maxBitRate"].asInt());
  EXPECT_EQ(1000, video["maxDelay"].asInt());
  EXPECT_EQ("144", video["maxDimensions"]["frameRate"].asString());
  EXPECT_EQ(1920, video["maxDimensions"]["width"].asInt());
  EXPECT_EQ(1080, video["maxDimensions"]["height"].asInt());
  EXPECT_EQ(300000, video["minBitRate"].asInt());
}

TEST_F(ReceiverSessionTest, HandlesNoValidAudioStream) {
  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage(kNoAudioOfferMessage);
  const std::vector<std::string>& messages = message_port_->posted_messages();
  EXPECT_EQ(1u, messages.size());
  json::Value answer = ExpectIsValidAnswer(messages[0])["answer"];

  // Should still select video stream.
  EXPECT_EQ(1u, answer["sendIndexes"].size());
  EXPECT_EQ(31338, answer["sendIndexes"][0].asInt());
  EXPECT_EQ(1u, answer["ssrcs"].size());
  EXPECT_EQ(19088746, answer["ssrcs"][0].asInt());
}

TEST_F(ReceiverSessionTest, HandlesInvalidCodec) {
  // We didn't select any streams, but didn't have any errors either.
  message_port_->ReceiveMessage(kInvalidCodecOfferMessage);
  const std::vector<std::string>& messages = message_port_->posted_messages();
  EXPECT_EQ(1u, messages.size());

  // We should have failed to produce a valid answer message due to not
  // selecting any stream.
  AssertGotAnErrorAnswerResponse();
}

TEST_F(ReceiverSessionTest, HandlesNoValidVideoStream) {
  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage(kNoVideoOfferMessage);
  const std::vector<std::string>& messages = message_port_->posted_messages();
  EXPECT_EQ(1u, messages.size());
  const json::Value answer = ExpectIsValidAnswer(messages[0])["answer"];

  // Should still select audio stream.
  EXPECT_EQ(1u, answer["sendIndexes"].size());
  EXPECT_EQ(1337, answer["sendIndexes"][0].asInt());
  EXPECT_EQ(1u, answer["ssrcs"].size());
  EXPECT_EQ(19088748, answer["ssrcs"][0].asInt());
}

TEST_F(ReceiverSessionTest, RejectsOfferIfNewOneComesBeforeNegotiationIsDone) {
  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(session_.get(),
                                    NegotiatedWithSender("first-sender")));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  // If the socket state is pending we don't process OFFERs yet.
  environment_->SetSocketStateForTesting(Environment::SocketState::kStarting);
  message_port_->ReceiveMessage("first-sender", kCastWebrtcNamespace,
                                kValidOfferMessage);
  EXPECT_EQ(0u, message_port_->posted_messages().size());

  // We should process the first OFFER now that the second one has come in.
  message_port_->ReceiveMessage("first-sender", kCastWebrtcNamespace,
                                kValidOfferMessage);
  EXPECT_EQ(1u, message_port_->posted_messages().size());
  ExpectIsErrorAnswer(message_port_->posted_messages()[0]);

  environment_->SetSocketStateForTesting(Environment::SocketState::kReady);
  EXPECT_EQ(2u, message_port_->posted_messages().size());
  ExpectIsValidAnswer(message_port_->posted_messages()[1]);
}

TEST_F(ReceiverSessionTest, HandlesRenegotiationFromSameSender) {
  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(session_.get(),
                                    NegotiatedWithSender("first-sender")));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kRenegotiated));
  EXPECT_CALL(client_, OnNegotiated(session_.get(),
                                    NegotiatedWithSender("first-sender")));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage("first-sender", kCastWebrtcNamespace,
                                kValidOfferMessage);
  EXPECT_EQ(1u, message_port_->posted_messages().size());
  ExpectIsValidAnswer(message_port_->posted_messages()[0]);

  message_port_->ReceiveMessage("first-sender", kCastWebrtcNamespace,
                                kValidOfferMessage);
  EXPECT_EQ(2u, message_port_->posted_messages().size());
  ExpectIsValidAnswer(message_port_->posted_messages()[1]);
}

TEST_F(ReceiverSessionTest, HandlesRenegotiationFromAnotherSender) {
  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(session_.get(),
                                    NegotiatedWithSender("first-sender")));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kRenegotiated));
  EXPECT_CALL(client_, OnNegotiated(session_.get(),
                                    NegotiatedWithSender("some-other-sender")));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage("first-sender", kCastWebrtcNamespace,
                                kValidOfferMessage);
  EXPECT_EQ(1u, message_port_->posted_messages().size());
  ExpectIsValidAnswer(message_port_->posted_messages()[0]);

  message_port_->ReceiveMessage("some-other-sender", kCastWebrtcNamespace,
                                kValidOfferMessage);
  EXPECT_EQ(2u, message_port_->posted_messages().size());
  ExpectIsValidAnswer(message_port_->posted_messages()[1]);
}

TEST_F(ReceiverSessionTest, HandlesErrorOfferFromAnotherSender) {
  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(session_.get(),
                                    NegotiatedWithSender("first-sender")));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage("first-sender", kCastWebrtcNamespace,
                                kValidOfferMessage);
  EXPECT_EQ(1u, message_port_->posted_messages().size());
  ExpectIsValidAnswer(message_port_->posted_messages()[0]);

  message_port_->ReceiveMessage("some-other-sender", kCastWebrtcNamespace,
                                kInvalidCodecOfferMessage);
  EXPECT_EQ(2u, message_port_->posted_messages().size());
  ExpectIsErrorAnswer(message_port_->posted_messages()[1]);
}

TEST_F(ReceiverSessionTest, HandlesNoValidStreams) {
  // We shouldn't call OnNegotiated if we failed to negotiate any streams.
  message_port_->ReceiveMessage(kNoAudioOrVideoOfferMessage);
  AssertGotAnErrorAnswerResponse();
}

TEST_F(ReceiverSessionTest, HandlesMalformedOffer) {
  // Note that unlike when we simply don't select any streams, when the offer
  // is not valid JSON we actually have no way of knowing it's an offer at all,
  // so we call OnError and do not reply with an Answer.
  EXPECT_CALL(client_, OnError(session_.get(), _));
  message_port_->ReceiveMessage(kInvalidJsonOfferMessage);
}

TEST_F(ReceiverSessionTest, HandlesMissingSeqNumInOffer) {
  // If the OFFER is missing a sequence number it gets rejected before being
  // parsed as an OFFER, since the sender expects all messages to come back
  // with a sequence number.
  message_port_->ReceiveMessage(kMissingSeqNumOfferMessage);
}

TEST_F(ReceiverSessionTest, HandlesOfferMissingMandatoryFields) {
  // If the OFFER is missing mandatory fields, we notify the client as well as
  // reply with an error-case Answer.
  EXPECT_CALL(client_, OnError(session_.get(), _));

  message_port_->ReceiveMessage(kMissingMandatoryFieldOfferMessage);
  AssertGotAnErrorAnswerResponse();
}

TEST_F(ReceiverSessionTest, HandlesImproperlyFormattedOffer) {
  EXPECT_CALL(client_, OnError(session_.get(), _));
  message_port_->ReceiveMessage(kValidJsonInvalidFormatOfferMessage);
  AssertGotAnErrorAnswerResponse();
}

TEST_F(ReceiverSessionTest, HandlesNullOffer) {
  EXPECT_CALL(client_, OnError(session_.get(), _));
  message_port_->ReceiveMessage(kNullJsonOfferMessage);
  AssertGotAnErrorAnswerResponse();
}

// The client is allowed to destroy the session from OnError(), so the session
// must only notify the client once and not access itself afterwards.
TEST_F(ReceiverSessionTest, HandlesClientDestroyingSessionOnMalformedOffer) {
  EXPECT_CALL(client_, OnError(session_.get(), _))
      .WillOnce(
          [this](const ReceiverSession*, const Error&) { session_.reset(); });
  message_port_->ReceiveMessage(kValidJsonInvalidFormatOfferMessage);
  EXPECT_FALSE(session_);
  AssertGotAnErrorAnswerResponse();
}

// We can't respond to an OFFER without a sender ID, so it should be dropped
// without notifying the client. Previously this resulted in the client being
// notified twice, which caused a use-after-free if the client destroyed the
// session in OnError().
TEST_F(ReceiverSessionTest, DropsMalformedOfferWithEmptySenderId) {
  ON_CALL(client_, OnError(_, _))
      .WillByDefault(
          [this](const ReceiverSession*, const Error&) { session_.reset(); });
  EXPECT_CALL(client_, OnError(_, _)).Times(0);
  message_port_->ReceiveMessage("", kCastWebrtcNamespace,
                                kValidJsonInvalidFormatOfferMessage);
  EXPECT_TRUE(session_);
  EXPECT_TRUE(message_port_->posted_messages().empty());
}

TEST_F(ReceiverSessionTest, DropsValidOfferWithEmptySenderId) {
  message_port_->ReceiveMessage("", kCastWebrtcNamespace, kValidOfferMessage);
  EXPECT_TRUE(message_port_->posted_messages().empty());
}

TEST_F(ReceiverSessionTest, HandlesInvalidSequenceNumber) {
  // We should just discard messages with an invalid sequence number.
  message_port_->ReceiveMessage(kInvalidSequenceNumberMessage);
}

TEST_F(ReceiverSessionTest, HandlesUnknownTypeMessage) {
  // We should just discard messages with an unknown message type.
  message_port_->ReceiveMessage(kUnknownTypeMessage);
}

TEST_F(ReceiverSessionTest, HandlesInvalidTypeMessage) {
  // We should just discard messages with an invalid message type.
  message_port_->ReceiveMessage(kInvalidTypeMessage);
}

TEST_F(ReceiverSessionTest, DoesNotCrashOnMessagePortError) {
  // We should report message port errors.
  EXPECT_CALL(client_, OnError(session_.get(), _));
  message_port_->ReceiveError(Error(Error::Code::kUnknownError));
}

TEST_F(ReceiverSessionTest, NotifiesReceiverDestruction) {
  InSequence s;
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kRenegotiated));
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage(kNoAudioOfferMessage);
  message_port_->ReceiveMessage(kValidOfferMessage);
}

TEST_F(ReceiverSessionTest, HandlesInvalidAnswer) {
  // Simulate an unbound local endpoint.
  EXPECT_CALL(*environment_, GetBoundLocalEndpoint)
      .WillOnce(Return(IPEndpoint{}));

  message_port_->ReceiveMessage(kValidOfferMessage);
  AssertGotAnErrorAnswerResponse();
}

TEST_F(ReceiverSessionTest, DelaysAnswerUntilEnvironmentIsReady) {
  environment_->SetSocketStateForTesting(Environment::SocketState::kStarting);

  // We should not have sent an answer yet--the UDP socket is not ready.
  message_port_->ReceiveMessage(kValidOfferMessage);
  ASSERT_TRUE(message_port_->posted_messages().empty());

  // Simulate the environment calling back into us with the socket being ready.
  // state() will not be called again--we just need to get the bind event.
  EXPECT_CALL(*environment_, GetBoundLocalEndpoint())
      .WillOnce(Return(IPEndpoint{{10, 0, 0, 2}, 4567}));
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
  session_->OnSocketReady();
  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());

  // We should have set the UDP port based on the ready socket value.
  const json::Value& message_body = ExpectIsValidAnswer(messages[0]);
  EXPECT_EQ(4567, message_body["answer"]["udpPort"].asInt());
}

TEST_F(ReceiverSessionTest,
       ReturnsErrorAnswerIfEnvironmentIsAlreadyInvalidated) {
  EXPECT_CALL(client_, OnError(session_.get(), _));
  environment_->SetSocketStateForTesting(Environment::SocketState::kInvalid);

  // If the environment is already in a bad state, we can respond immediately.
  message_port_->ReceiveMessage(kValidOfferMessage);
  AssertGotAnErrorAnswerResponse();
}

TEST_F(ReceiverSessionTest, ReturnsErrorAnswerIfEnvironmentIsInvalidated) {
  EXPECT_CALL(client_, OnError(session_.get(), _));
  environment_->SetSocketStateForTesting(Environment::SocketState::kStarting);

  // We should not have sent an answer yet--the environment is not ready.
  message_port_->ReceiveMessage(kValidOfferMessage);
  ASSERT_TRUE(message_port_->posted_messages().empty());

  // Simulate the environment calling back into us with invalidation.
  environment_->SetSocketStateForTesting(Environment::SocketState::kInvalid);

  AssertGotAnErrorAnswerResponse();
}

TEST_F(ReceiverSessionTest, ReturnsErrorCapabilitiesIfRemotingDisabled) {
  message_port_->ReceiveMessage(kGetCapabilitiesMessage);
  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());

  // We should have an error response.
  ErrorOr<json::Value> message_body = json::Parse(messages[0]);
  EXPECT_TRUE(message_body.is_value());
  EXPECT_EQ("CAPABILITIES_RESPONSE", message_body.value()["type"].asString());
  EXPECT_EQ("error", message_body.value()["result"].asString());
}

TEST_F(ReceiverSessionTest, ReturnsCapabilitiesWithRemotingDefaults) {
  ReceiverConstraints constraints;
  constraints.remoting = std::make_unique<RemotingConstraints>();

  SetUpWithConstraints(std::move(constraints));
  message_port_->ReceiveMessage(kGetCapabilitiesMessage);
  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());

  const ErrorOr<json::Value> message_body = json::Parse(messages[0]);
  EXPECT_TRUE(message_body.is_value());
  EXPECT_EQ("CAPABILITIES_RESPONSE", message_body.value()["type"].asString());
  EXPECT_EQ("ok", message_body.value()["result"].asString());
  const ReceiverCapability response =
      ReceiverCapability::Parse(message_body.value()["capabilities"]).value();

  EXPECT_THAT(
      response.media_capabilities,
      testing::ElementsAre(MediaCapability::kOpus, MediaCapability::kAac,
                           MediaCapability::kVp8, MediaCapability::kH264));
}

TEST_F(ReceiverSessionTest, ReturnsCapabilitiesWithRemotingConstraints) {
  ReceiverConstraints constraints;
  constraints.video_codecs = {VideoCodec::kH264};
  constraints.remoting = std::make_unique<RemotingConstraints>();
  constraints.remoting->supports_chrome_audio_codecs = true;
  constraints.remoting->supports_4k = true;

  SetUpWithConstraints(std::move(constraints));
  message_port_->ReceiveMessage(kGetCapabilitiesMessage);
  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());

  const ErrorOr<json::Value> message_body = json::Parse(messages[0]);
  EXPECT_TRUE(message_body.is_value());
  EXPECT_EQ("CAPABILITIES_RESPONSE", message_body.value()["type"].asString());
  EXPECT_EQ("ok", message_body.value()["result"].asString());
  const ReceiverCapability response =
      ReceiverCapability::Parse(message_body.value()["capabilities"]).value();

  EXPECT_THAT(
      response.media_capabilities,
      testing::ElementsAre(MediaCapability::kOpus, MediaCapability::kAac,
                           MediaCapability::kH264, MediaCapability::kAudio,
                           MediaCapability::k4k));
}

TEST_F(ReceiverSessionTest, CanNegotiateRemoting) {
  ReceiverConstraints constraints;
  constraints.remoting = std::make_unique<RemotingConstraints>();
  constraints.remoting->supports_chrome_audio_codecs = true;
  constraints.remoting->supports_4k = true;
  SetUpWithConstraints(std::move(constraints));

  InSequence s;
  EXPECT_CALL(client_, OnRemotingNegotiated(session_.get(), _))
      .WillOnce([](const ReceiverSession* session_,
                   ReceiverSession::RemotingNegotiation negotiation) {
        const ReceiverSession::ConfiguredReceivers& cr = negotiation.receivers;
        EXPECT_TRUE(cr.audio_receiver);
        EXPECT_EQ(cr.audio_receiver->config().sender_ssrc, 19088747u);
        EXPECT_EQ(cr.audio_receiver->config().receiver_ssrc, 19088748u);
        EXPECT_EQ(cr.audio_receiver->config().channels, 2);
        EXPECT_EQ(cr.audio_receiver->config().rtp_timebase, 48000);
        EXPECT_EQ(cr.audio_config.codec, AudioCodec::kNotSpecified);

        EXPECT_TRUE(cr.video_receiver);
        EXPECT_EQ(cr.video_receiver->config().sender_ssrc, 19088745u);
        EXPECT_EQ(cr.video_receiver->config().receiver_ssrc, 19088746u);
        EXPECT_EQ(cr.video_receiver->config().channels, 1);
        EXPECT_EQ(cr.video_receiver->config().rtp_timebase, 90000);
        EXPECT_EQ(cr.video_config.codec, VideoCodec::kNotSpecified);
      });
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage(kValidRemotingOfferMessage);
}

TEST_F(ReceiverSessionTest, HandlesRpcMessage) {
  ReceiverConstraints constraints;
  constraints.remoting = std::make_unique<RemotingConstraints>();
  constraints.remoting->supports_chrome_audio_codecs = true;
  constraints.remoting->supports_4k = true;
  SetUpWithConstraints(std::move(constraints));

  message_port_->ReceiveMessage(kRpcMessage);
  const std::vector<std::string>& messages = message_port_->posted_messages();
  // Nothing should happen yet, we haven't negotiated with a sender.
  ASSERT_EQ(0u, messages.size());

  // We don't need to fully test that the subscription model on the RpcMessenger
  // works, but we do want to test that the ReceiverSession has properly wired
  // the RpcMessenger up to the backing SessionMessenger and can properly
  // handle received RPC messages.
  InSequence s;
  bool received_initialize_message = false;
  EXPECT_CALL(client_, OnRemotingNegotiated(session_.get(), _))
      .WillOnce([this, &received_initialize_message](
                    const ReceiverSession* session_,
                    ReceiverSession::RemotingNegotiation negotiation) mutable {
        negotiation.messenger->RegisterMessageReceiverCallback(
            100, [&received_initialize_message](
                     std::unique_ptr<RpcMessage> message) mutable {
              ASSERT_EQ(100, message->handle());
              ASSERT_EQ(RpcMessage::RPC_DS_INITIALIZE_CALLBACK,
                        message->proc());
              ASSERT_EQ(0, message->integer_value());
              received_initialize_message = true;
            });

        message_port_->ReceiveMessage(kRpcMessage);
      });
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage(kValidRemotingOfferMessage);
  ASSERT_TRUE(received_initialize_message);
}

TEST_F(ReceiverSessionTest,
       RpcMessengerPersistsAndClearsCallbacksOnRenegotiation) {
  ReceiverConstraints constraints;
  constraints.remoting = std::make_unique<RemotingConstraints>();
  SetUpWithConstraints(std::move(constraints));

  RpcMessenger* const initial_messenger = &session_->rpc_messenger();

  // First negotiate mirroring.
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  message_port_->ReceiveMessage(kValidOfferMessage);
  EXPECT_EQ(&session_->rpc_messenger(), initial_messenger);

  // Then negotiate remoting.
  EXPECT_CALL(client_, OnRemotingNegotiated(session_.get(), _))
      .WillOnce([](const ReceiverSession*,
                   ReceiverSession::RemotingNegotiation negotiation) {
        negotiation.messenger->RegisterMessageReceiverCallback(
            RpcMessenger::kAcquireRendererHandle,
            [](std::unique_ptr<RpcMessage>) {});
      });
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kRenegotiated));
  message_port_->ReceiveMessage(kValidRemotingOfferMessage);
  EXPECT_EQ(&session_->rpc_messenger(), initial_messenger);
  EXPECT_TRUE(session_->rpc_messenger().IsRegisteredForTesting(
      RpcMessenger::kAcquireRendererHandle));

  // Renegotiate mirroring. ResetReceivers must clear registered callbacks.
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kRenegotiated));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
  message_port_->ReceiveMessage(kValidOfferMessage);
  EXPECT_EQ(&session_->rpc_messenger(), initial_messenger);
  EXPECT_FALSE(session_->rpc_messenger().IsRegisteredForTesting(
      RpcMessenger::kAcquireRendererHandle));

  // Re-registering the predefined handle on the next remoting session
  // succeeds without crashing on duplicate handle registration.
  session_->rpc_messenger().RegisterMessageReceiverCallback(
      RpcMessenger::kAcquireRendererHandle, [](std::unique_ptr<RpcMessage>) {});
  EXPECT_TRUE(session_->rpc_messenger().IsRegisteredForTesting(
      RpcMessenger::kAcquireRendererHandle));
}

TEST_F(ReceiverSessionTest, DropsRpcMessagesWhenNotRemoting) {
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
  message_port_->ReceiveMessage(kValidOfferMessage);

  // Register a callback on the session's RpcMessenger.
  bool received_rpc = false;
  session_->rpc_messenger().RegisterMessageReceiverCallback(
      100,
      [&received_rpc](std::unique_ptr<RpcMessage>) { received_rpc = true; });

  // Send an RPC message while mirroring. It must be dropped.
  message_port_->ReceiveMessage(kRpcMessage);
  EXPECT_FALSE(received_rpc);
}

TEST_F(ReceiverSessionTest, EnablesDscpInAnswer) {
  ReceiverConstraints constraints;
  constraints.enable_dscp = true;
  SetUpWithConstraints(std::move(constraints));
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
  message_port_->ReceiveMessage(kValidOfferMessage);
  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  json::Value message = ExpectIsValidAnswer(messages[0]);
  const json::Value& answer = message["answer"];
  ASSERT_TRUE(answer.isObject());
  const json::Value& dscp = answer["receiverRtcpDscp"];
  ASSERT_FALSE(dscp.empty());
  EXPECT_EQ(dscp[0], 1337);
  EXPECT_EQ(dscp[1], 31338);

  message_port_->clear();
  ReceiverConstraints constraints2;
  constraints2.enable_dscp = false;
  SetUpWithConstraints(std::move(constraints2));
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
  message_port_->ReceiveMessage(kValidOfferMessage);  // Re-send offer message
  const std::vector<std::string>& messages2 = message_port_->posted_messages();
  ASSERT_EQ(1u, messages2.size());
  message = ExpectIsValidAnswer(messages2[0]);
  const json::Value& answer2 = message["answer"];
  ASSERT_TRUE(answer2.isObject());
  const json::Value& dscp2 = answer2["receiverRtcpDscp"];
  ASSERT_TRUE(dscp2.empty());
}

TEST_F(ReceiverSessionTest, InputEventsOptIn) {
  ReceiverConstraints constraints;
  constraints.supports_input_events = true;
  SetUpWithConstraints(std::move(constraints));

  EXPECT_CALL(client_, OnNegotiated(session_.get(), _))
      .WillOnce([](const ReceiverSession* session,
                   ReceiverSession::ConfiguredReceivers cr) {
        EXPECT_TRUE(cr.input_enabled);
      });
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage(kValidOfferMessageWithInput);

  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  json::Value message = ExpectIsValidAnswer(messages[0]);
  const json::Value& answer = message["answer"];
  const json::Value& extensions = answer["rtpExtensions"];
  EXPECT_TRUE(
      std::ranges::any_of(extensions, [](const json::Value& stream_extensions) {
        return Contains(stream_extensions, kInputEventsRtpExtension);
      }));
}

TEST_F(ReceiverSessionTest, HandlesInputMessage) {
  ReceiverConstraints constraints;
  constraints.supports_input_events = true;
  SetUpWithConstraints(std::move(constraints));

  message_port_->ReceiveMessage(kInputMessage);
  // Nothing should happen yet, the session doesn't have a messenger.
  ASSERT_EQ(0u, message_port_->posted_messages().size());

  InSequence s;
  bool received_negotiation = false;
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _))
      .WillOnce([&received_negotiation](
                    const ReceiverSession* session,
                    ReceiverSession::ConfiguredReceivers receivers) mutable {
        ASSERT_TRUE(receivers.input_enabled);
        received_negotiation = true;
      });
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage(kValidOfferMessageWithInput);
  ASSERT_TRUE(received_negotiation);
}

TEST_F(ReceiverSessionTest, HandlesInputMessengerNotNegotiated) {
  ReceiverConstraints constraints;
  constraints.supports_input_events = false;
  SetUpWithConstraints(std::move(constraints));

  EXPECT_CALL(client_, OnNegotiated(session_.get(), _))
      .WillOnce([](const ReceiverSession* session,
                   ReceiverSession::ConfiguredReceivers cr) {
        EXPECT_FALSE(cr.input_enabled);
      });
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage(kValidOfferMessageWithInput);
}

TEST_F(ReceiverSessionTest, HandlesInputMessengerSendsMessage) {
  ReceiverConstraints constraints;
  constraints.supports_input_events = true;
  SetUpWithConstraints(std::move(constraints));

  EXPECT_CALL(client_, OnNegotiated(session_.get(), _))
      .WillOnce([this](const ReceiverSession* session,
                       ReceiverSession::ConfiguredReceivers cr) {
        ASSERT_TRUE(cr.input_enabled);

        InputMessage message;
        auto* event = message.add_events();
        event->set_type(InputMessage::INPUT_TYPE_KEY_DOWN);
        this->session_->SendInputMessage(message);
      });
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));

  message_port_->ReceiveMessage(kValidOfferMessageWithInput);

  // Verify message was sent through message port.
  const auto& messages = message_port_->posted_messages();

  // We expect at least 2 messages: ANSWER and INPUT.
  ASSERT_GE(messages.size(), 2u);

  // Index 0 is ANSWER.
  ExpectIsValidAnswer(messages[0]);

  // Index 1 should be INPUT.
  ErrorOr<json::Value> input_json = json::Parse(messages[1]);
  ASSERT_TRUE(input_json.is_value());
  EXPECT_EQ("INPUT", input_json.value()["type"].asString());
  EXPECT_FALSE(input_json.value()["input"].asString().empty());
}

TEST_F(ReceiverSessionTest, HandlesInputMessengerReceivesMessage) {
  ReceiverConstraints constraints;
  constraints.supports_input_events = true;
  SetUpWithConstraints(std::move(constraints));

  bool received_input = false;
  session_->SetInputCallback(
      [&received_input](InputMessage message) { received_input = true; });

  EXPECT_CALL(client_, OnNegotiated(session_.get(), _))
      .WillOnce([](const ReceiverSession* session,
                   ReceiverSession::ConfiguredReceivers cr) {
        ASSERT_TRUE(cr.input_enabled);
      });

  message_port_->ReceiveMessage(kValidOfferMessageWithInput);

  message_port_->ReceiveMessage(kInputMessage);
  ASSERT_TRUE(received_input);

  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
}

TEST_F(ReceiverSessionTest, UnsubscribesFromEnvironmentOnDestruction) {
  session_.reset();
  environment_->SetSocketStateForTesting(Environment::SocketState::kReady);
}

class FakeWebTransportStream : public WebTransportStream {
 public:
  void SetDelegate(Delegate* delegate) override {
    delegate_ = delegate;
    FlushIncomingBytes();
  }

  bool Write(ByteView data) override { return true; }
  void Close() override {}

  void BufferIncomingBytes(std::vector<uint8_t> bytes) {
    incoming_buffer_.insert(incoming_buffer_.end(), bytes.begin(), bytes.end());
    FlushIncomingBytes();
  }

 private:
  void FlushIncomingBytes() {
    if (delegate_ && !incoming_buffer_.empty()) {
      std::vector<uint8_t> data = std::move(incoming_buffer_);
      incoming_buffer_.clear();
      delegate_->OnRead(this, ByteView(data.data(), data.size()));
    }
  }

  raw_ptr<Delegate> delegate_ = nullptr;
  std::vector<uint8_t> incoming_buffer_;
};

class FakeWebTransportSession : public WebTransportSession {
 public:
  // If set, `close_error` records the error passed to Close(); it must outlive
  // this session.
  explicit FakeWebTransportSession(std::optional<Error>* close_error = nullptr)
      : close_error_(close_error) {}

  void SetDelegate(Delegate* delegate) override { delegate_ = delegate; }

  ErrorOr<std::unique_ptr<WebTransportStream>> CreateOutgoingStream() override {
    return std::unique_ptr<WebTransportStream>(
        std::make_unique<FakeWebTransportStream>());
  }

  void Close(const Error& error) override {
    if (close_error_) {
      *close_error_ = error;
    }
    SimulateClosed(error);
  }

  // Like a real transport, keeps using its own state after notifying the
  // delegate, so destroying this session from inside the notification is
  // caught by ASAN.
  void SimulateClosed(const Error& error) {
    if (delegate_) {
      delegate_->OnSessionClosed(this, error);
    }
    ++close_notification_count_;
  }

  void AddIncomingStream(std::unique_ptr<WebTransportStream> stream) {
    auto* ptr = stream.get();
    incoming_streams_.push_back(std::move(stream));
    if (delegate_) {
      delegate_->OnIncomingStream(ptr);
    }
  }

  WeakPtr<FakeWebTransportSession> GetWeakPtr() const {
    return weak_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<std::optional<Error>> close_error_ = nullptr;
  raw_ptr<Delegate> delegate_ = nullptr;
  int close_notification_count_ = 0;
  std::vector<std::unique_ptr<WebTransportStream>> incoming_streams_;
  WeakPtrFactory<FakeWebTransportSession> weak_factory_{this};
};

class FakeWebTransportServer : public WebTransportServer {
 public:
  // If set, `stop_count` counts calls to Stop(); it must outlive this server.
  FakeWebTransportServer(std::string fingerprint, int* stop_count)
      : fingerprint_(std::move(fingerprint)), stop_count_(stop_count) {}

  void SetDelegate(Delegate* delegate) override { delegate_ = delegate; }
  uint16_t Start(const IPEndpoint& local_endpoint) override { return 33534; }
  void Stop() override {
    if (stop_count_) {
      ++*stop_count_;
    }
  }
  std::string GetCertificateFingerprint() const override {
    return fingerprint_;
  }

  // Simulates a sender connecting to this server.
  void AcceptSession(std::unique_ptr<WebTransportSession> session) {
    ASSERT_TRUE(delegate_);
    delegate_->OnSessionCreated(std::move(session));
  }

  WeakPtr<FakeWebTransportServer> GetWeakPtr() const {
    return weak_factory_.GetWeakPtr();
  }

 private:
  const std::string fingerprint_;
  raw_ptr<int> stop_count_ = nullptr;
  raw_ptr<Delegate> delegate_ = nullptr;
  WeakPtrFactory<FakeWebTransportServer> weak_factory_{this};
};

// Returns a callback whose most recently created server is exposed through
// `server`. `stop_count`, if set, counts calls to Stop() on the servers and
// must outlive them.
CreateWebTransportServerCallback MakeFakeServerCallback(
    WeakPtr<FakeWebTransportServer>* server,
    int* stop_count = nullptr,
    std::string fingerprint = "fingerprint") {
  return [server, stop_count, fingerprint](TaskRunner&) {
    auto fake =
        std::make_unique<FakeWebTransportServer>(fingerprint, stop_count);
    *server = fake->GetWeakPtr();
    return fake;
  };
}

// Returns default constraints with DataChannels enabled through
// `create_server`.
ReceiverConstraints MakeDataChannelConstraints(
    CreateWebTransportServerCallback create_server) {
  ReceiverConstraints constraints;
  constraints.create_web_transport_server = std::move(create_server);
  return constraints;
}

TEST_F(ReceiverSessionTest, CanNegotiateWithDataChannel) {
  WeakPtr<FakeWebTransportServer> server;
  SetUpWithConstraints(
      MakeDataChannelConstraints(MakeFakeServerCallback(&server)));

  EXPECT_CALL(client_, OnNegotiated(session_.get(), _))
      .WillOnce([](const ReceiverSession* session_,
                   ReceiverSession::ConfiguredReceivers cr) {
        EXPECT_TRUE(cr.audio_receiver);
        EXPECT_TRUE(cr.video_receiver);
      });

  message_port_->ReceiveMessage(kValidOfferMessageWithDataChannel);

  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());

  json::Value message = ExpectIsValidAnswer(messages[0]);
  EXPECT_EQ("ANSWER", message["type"].asString());
  EXPECT_EQ(1337, message["seqNum"].asInt());

  const json::Value& answer = message["answer"];
  EXPECT_TRUE(answer.isObject());
  EXPECT_FALSE(answer["dataTransport"].isNull());
  EXPECT_TRUE(answer["dataTransport"].isObject());
  EXPECT_EQ("webtransport", answer["dataTransport"]["protocol"].asString());
  EXPECT_EQ(33534, answer["dataTransport"]["port"].asInt());
  EXPECT_EQ("fingerprint",
            answer["dataTransport"]["certificateFingerprint"].asString());

  ASSERT_TRUE(server);
  auto session = std::make_unique<FakeWebTransportSession>();
  WeakPtr<FakeWebTransportSession> weak_session = session->GetWeakPtr();
  server->AcceptSession(std::move(session));
  ASSERT_TRUE(weak_session);

  auto incoming_stream = std::make_unique<FakeWebTransportStream>();
  // Send Open Frame: Type (0x01) + Length (0x0005) + "label"
  incoming_stream->BufferIncomingBytes({1, 0, 5, 'l', 'a', 'b', 'e', 'l'});

  EXPECT_CALL(client_, OnNewDataChannel(session_.get(), _))
      .WillOnce(
          [](const ReceiverSession*, std::unique_ptr<DataChannel> channel) {
            ASSERT_NE(channel, nullptr);
            EXPECT_EQ(channel->label(), "label");
            EXPECT_EQ(channel->state(), DataChannel::State::kOpen);
          });
  weak_session->AddIncomingStream(std::move(incoming_stream));

  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
}

TEST_F(ReceiverSessionTest, RejectsSecondDataSessionWhenActive) {
  WeakPtr<FakeWebTransportServer> server;
  SetUpWithConstraints(
      MakeDataChannelConstraints(MakeFakeServerCallback(&server)));

  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  message_port_->ReceiveMessage(kValidOfferMessageWithDataChannel);
  ASSERT_TRUE(server);

  std::optional<Error> first_close_error;
  auto first_session =
      std::make_unique<FakeWebTransportSession>(&first_close_error);
  WeakPtr<FakeWebTransportSession> weak_first_session =
      first_session->GetWeakPtr();
  server->AcceptSession(std::move(first_session));

  std::optional<Error> second_close_error;
  server->AcceptSession(
      std::make_unique<FakeWebTransportSession>(&second_close_error));

  // The second session is closed and the first one is kept.
  ASSERT_TRUE(second_close_error.has_value());
  EXPECT_EQ(Error::Code::kSocketInvalidState, second_close_error->code());
  EXPECT_TRUE(weak_first_session);
  EXPECT_FALSE(first_close_error.has_value());

  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
}

TEST_F(ReceiverSessionTest, AnswerOmitsDataTransportWhenFingerprintEmpty) {
  WeakPtr<FakeWebTransportServer> server;
  int stop_count = 0;
  SetUpWithConstraints(MakeDataChannelConstraints(
      MakeFakeServerCallback(&server, &stop_count, /*fingerprint=*/"")));

  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  message_port_->ReceiveMessage(kValidOfferMessageWithDataChannel);

  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  json::Value message = ExpectIsValidAnswer(messages[0]);
  EXPECT_TRUE(message["answer"]["dataTransport"].isNull());

  // The unusable server is stopped exactly once and discarded.
  EXPECT_FALSE(server);
  EXPECT_EQ(1, stop_count);

  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
}

TEST_F(ReceiverSessionTest, AnswerOmitsDataTransportWhenServerCreationFails) {
  SetUpWithConstraints(MakeDataChannelConstraints(
      [](TaskRunner&) { return std::unique_ptr<WebTransportServer>(); }));

  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  message_port_->ReceiveMessage(kValidOfferMessageWithDataChannel);

  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  json::Value message = ExpectIsValidAnswer(messages[0]);
  EXPECT_TRUE(message["answer"]["dataTransport"].isNull());

  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
}

TEST_F(ReceiverSessionTest, HandlesDataSessionGracefulClosure) {
  WeakPtr<FakeWebTransportServer> server;
  SetUpWithConstraints(
      MakeDataChannelConstraints(MakeFakeServerCallback(&server)));
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  message_port_->ReceiveMessage(kValidOfferMessageWithDataChannel);

  ASSERT_TRUE(server);
  auto session = std::make_unique<FakeWebTransportSession>();
  WeakPtr<FakeWebTransportSession> weak_session = session->GetWeakPtr();
  server->AcceptSession(std::move(session));
  ASSERT_TRUE(weak_session);

  // A graceful close is reported through OnDataSessionClosed() only, not as
  // an error.
  EXPECT_CALL(client_, OnDataSessionClosed(session_.get(),
                                           CodeEquals(Error::Code::kNone)));
  weak_session->SimulateClosed(Error::None());
  task_runner_.RunTasksUntilIdle();
  EXPECT_FALSE(weak_session);
  EXPECT_FALSE(server);

  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
}

TEST_F(ReceiverSessionTest, HandlesDataSessionErrorClosureWithoutOnError) {
  WeakPtr<FakeWebTransportServer> server;
  SetUpWithConstraints(
      MakeDataChannelConstraints(MakeFakeServerCallback(&server)));
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  message_port_->ReceiveMessage(kValidOfferMessageWithDataChannel);

  ASSERT_TRUE(server);
  auto session = std::make_unique<FakeWebTransportSession>();
  WeakPtr<FakeWebTransportSession> weak_session = session->GetWeakPtr();
  server->AcceptSession(std::move(session));
  ASSERT_TRUE(weak_session);

  // An error close is reported through OnDataSessionClosed() only, not
  // OnError().
  EXPECT_CALL(client_,
              OnDataSessionClosed(session_.get(),
                                  CodeEquals(Error::Code::kConnectionFailed)));
  EXPECT_CALL(client_, OnError(_, _)).Times(0);
  weak_session->SimulateClosed(
      Error(Error::Code::kConnectionFailed, "Session reset"));
  task_runner_.RunTasksUntilIdle();
  EXPECT_FALSE(weak_session);
  EXPECT_FALSE(server);

  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
}

TEST_F(ReceiverSessionTest, DestroyFromOnDataSessionClosedIsSafe) {
  WeakPtr<FakeWebTransportServer> server;
  SetUpWithConstraints(
      MakeDataChannelConstraints(MakeFakeServerCallback(&server)));
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  message_port_->ReceiveMessage(kValidOfferMessageWithDataChannel);

  ASSERT_TRUE(server);
  auto session = std::make_unique<FakeWebTransportSession>();
  WeakPtr<FakeWebTransportSession> weak_session = session->GetWeakPtr();
  server->AcceptSession(std::move(session));
  ASSERT_TRUE(weak_session);

  // Destroying ReceiverSession from inside OnDataSessionClosed() must not
  // cause a use-after-free while the data session is reporting the closure.
  EXPECT_CALL(client_,
              OnDataSessionClosed(session_.get(),
                                  CodeEquals(Error::Code::kConnectionFailed)))
      .WillOnce([this](const ReceiverSession*, const Error&) {
        EXPECT_CALL(client_, OnReceiversDestroying(
                                 session_.get(),
                                 ReceiverSession::Client::kEndOfSession));
        session_.reset();
      });
  weak_session->SimulateClosed(
      Error(Error::Code::kConnectionFailed, "Session reset"));
  task_runner_.RunTasksUntilIdle();
  EXPECT_FALSE(session_);
  EXPECT_FALSE(weak_session);
}

TEST_F(ReceiverSessionTest, RenegotiationDropsPreviousDataSession) {
  WeakPtr<FakeWebTransportServer> server;
  SetUpWithConstraints(
      MakeDataChannelConstraints(MakeFakeServerCallback(&server)));
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  message_port_->ReceiveMessage(kValidOfferMessageWithDataChannel);

  ASSERT_TRUE(server);
  auto session = std::make_unique<FakeWebTransportSession>();
  WeakPtr<FakeWebTransportSession> weak_session = session->GetWeakPtr();
  server->AcceptSession(std::move(session));
  ASSERT_TRUE(weak_session);

  // A new OFFER without data transport renegotiates and drops the previous
  // data session.
  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kRenegotiated));
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  message_port_->ReceiveMessage(kValidOfferMessage);

  // Events from the previous data session must not be reported to the client.
  ASSERT_TRUE(weak_session);
  weak_session->SimulateClosed(
      Error(Error::Code::kConnectionFailed, "Session reset"));
  task_runner_.RunTasksUntilIdle();
  EXPECT_FALSE(weak_session);
  EXPECT_FALSE(server);

  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
}

TEST_F(ReceiverSessionTest, OfferFromOnDataSessionClosedWithErrorIsSafe) {
  WeakPtr<FakeWebTransportServer> server;
  SetUpWithConstraints(
      MakeDataChannelConstraints(MakeFakeServerCallback(&server)));
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
  message_port_->ReceiveMessage(kValidOfferMessageWithDataChannel);

  ASSERT_TRUE(server);
  auto session = std::make_unique<FakeWebTransportSession>();
  WeakPtr<FakeWebTransportSession> weak_session = session->GetWeakPtr();
  server->AcceptSession(std::move(session));
  ASSERT_TRUE(weak_session);

  // A new OFFER handled from inside OnDataSessionClosed() sets up a new data
  // session without destroying the old one while it is reporting the closure.
  EXPECT_CALL(client_,
              OnDataSessionClosed(session_.get(),
                                  CodeEquals(Error::Code::kConnectionFailed)))
      .WillOnce([this](const ReceiverSession*, const Error&) {
        EXPECT_CALL(client_, OnReceiversDestroying(
                                 session_.get(),
                                 ReceiverSession::Client::kRenegotiated));
        EXPECT_CALL(client_, OnNegotiated(session_.get(), _));
        message_port_->ReceiveMessage(kValidOfferMessageWithDataChannel);
      });

  weak_session->SimulateClosed(
      Error(Error::Code::kConnectionFailed, "Session reset"));
  task_runner_.RunTasksUntilIdle();
  EXPECT_FALSE(weak_session);

  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(2u, messages.size());
  json::Value message = ExpectIsValidAnswer(messages[1]);
  EXPECT_FALSE(message["answer"]["dataTransport"].isNull());

  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
}

TEST_F(ReceiverSessionTest, AnswerOmitsDataTransportWhenDisabled) {
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));

  message_port_->ReceiveMessage(kValidOfferMessageWithDataChannel);

  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  json::Value message = ExpectIsValidAnswer(messages[0]);
  EXPECT_TRUE(message["answer"]["dataTransport"].isNull());

  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
}

TEST_F(ReceiverSessionTest, AnswerOmitsDataTransportWhenNotOffered) {
  WeakPtr<FakeWebTransportServer> server;
  SetUpWithConstraints(
      MakeDataChannelConstraints(MakeFakeServerCallback(&server)));
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _));

  message_port_->ReceiveMessage(kValidOfferMessage);

  const std::vector<std::string>& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  json::Value message = ExpectIsValidAnswer(messages[0]);
  EXPECT_TRUE(message["answer"]["dataTransport"].isNull());
  // No server is created for an OFFER without a data transport.
  EXPECT_FALSE(server);

  EXPECT_CALL(client_,
              OnReceiversDestroying(session_.get(),
                                    ReceiverSession::Client::kEndOfSession));
}

}  // namespace openscreen::cast
