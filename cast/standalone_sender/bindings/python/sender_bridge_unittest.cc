// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/bindings/python/sender_bridge.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "cast/standalone_sender/bindings/python/cast_runtime.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen::cast {

class SenderBridgeTest : public ::testing::Test {
 protected:
  SenderBridgeTest()
      : clock_(Clock::now()),
        task_runner_(clock_),
        bridge_(&task_runner_,
                IPAddress::Parse("127.0.0.1").value(),
                8009,
                "") {}

  FakeClock clock_;
  FakeTaskRunner task_runner_;
  CastSenderBridge bridge_;
};

TEST_F(SenderBridgeTest, ConstructionSucceedsWithValidIP) {
  CastSenderBridge bridge1(&task_runner_,
                           IPAddress::Parse("192.168.1.1").value(), 8009, "");
  CastSenderBridge bridge2(&task_runner_, IPAddress::Parse("::1").value(), 8009,
                           "");
}

TEST_F(SenderBridgeTest, StreamVideoFailsOnInvalidCodec) {
  CastSenderBridge::StreamConfig config;
  config.file_path = "test.mp4";
  config.codec_name = "invalid_codec";
  EXPECT_FALSE(bridge_.StreamVideo(config, /*is_debug=*/false));

  // Verify non-standard h265 alias is rejected in favor of standard hevc.
  config.codec_name = "h265";
  EXPECT_FALSE(bridge_.StreamVideo(config, /*is_debug=*/false));
}

TEST_F(SenderBridgeTest, StreamVideoAcceptsAllSupportedCodecs) {
  CastRuntime runtime;
  const std::vector<std::string> kCodecs = {
#if defined(CAST_STANDALONE_SENDER_HAVE_LIBVPX)
      "vp8",  "vp9",
#endif
#if defined(CAST_STANDALONE_SENDER_HAVE_LIBAOM)
      "av1",
#endif
#if defined(CAST_STANDALONE_SENDER_HAVE_FFMPEG)
      "h264", "hevc",
#endif
  };

  for (const auto& codec : kCodecs) {
    CastSenderBridge bridge(runtime.task_runner(),
                            IPAddress::Parse("127.0.0.1").value(), 8009, "");
    CastSenderBridge::StreamConfig config;
    config.file_path = "test.mp4";
    config.codec_name = codec;
    EXPECT_TRUE(bridge.StreamVideo(config, /*is_debug=*/false))
        << "Failed for codec: " << codec;
  }
}

TEST_F(SenderBridgeTest, TeardownSafeWhenNotStreaming) {
  bridge_.Teardown();
}

TEST_F(SenderBridgeTest, GetStreamingStartTimeReturnsNulloptWhenNotStreaming) {
  EXPECT_FALSE(bridge_.GetStreamingStartTime().has_value());
}

TEST_F(SenderBridgeTest, GetStreamingStartTimeReturnsNulloptBeforeNegotiation) {
  CastRuntime runtime;
  CastSenderBridge bridge(runtime.task_runner(),
                          IPAddress::Parse("127.0.0.1").value(), 8009, "");
  CastSenderBridge::StreamConfig config;
  config.file_path = "test.mp4";
  config.codec_name = "vp8";
  ASSERT_TRUE(bridge.StreamVideo(config, /*is_debug=*/false));
  EXPECT_FALSE(bridge.GetStreamingStartTime().has_value());
  bridge.Teardown();
}

TEST_F(SenderBridgeTest, StreamVideoValidatesPacketDropSettings) {
  const auto make_drop_config = [](int64_t delay_ms, int64_t total_ms,
                                   double drop_ratio, int64_t burst_ms) {
    return PacketDropConfig{
        .start_delay = std::chrono::milliseconds(delay_ms),
        .total_duration = std::chrono::milliseconds(total_ms),
        .drop_ratio = drop_ratio,
        .burst_duration = std::chrono::milliseconds(burst_ms),
    };
  };

  for (const auto& invalid : {
           make_drop_config(-1, 60000, 0.2, 250),
           make_drop_config(30000, 0, 0.2, 250),
           make_drop_config(30000, -1, 0.2, 250),
           make_drop_config(30000, 60000, 0.0, 250),
           make_drop_config(30000, 60000, -0.1, 250),
           make_drop_config(30000, 60000, 1.01, 250),
           make_drop_config(30000, 60000,
                            std::numeric_limits<double>::quiet_NaN(), 250),
           make_drop_config(30000, 60000, 0.2, 0),
           make_drop_config(30000, 60000, 0.2, -1),
       }) {
    EXPECT_FALSE(invalid.IsValid());
  }
  EXPECT_TRUE(make_drop_config(0, 1, 0.2, 250).IsValid());
  EXPECT_TRUE(make_drop_config(30000, 60000, 1.0, 1).IsValid());

  CastSenderBridge::StreamConfig config{
      .file_path = "test.mp4",
      .codec_name = "vp8",
      .packet_drop_config = make_drop_config(30000, 0, 0.2, 250),
  };
  EXPECT_FALSE(bridge_.StreamVideo(config, /*is_debug=*/false));
}

}  // namespace openscreen::cast
