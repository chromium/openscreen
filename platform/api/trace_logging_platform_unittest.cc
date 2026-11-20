// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/trace_logging_platform.h"

#include <optional>

#include "gtest/gtest.h"
#include "platform/base/trace_logging_activation.h"

namespace openscreen {
namespace {

class FakeTraceLoggingPlatform : public TraceLoggingPlatform {
 public:
  ~FakeTraceLoggingPlatform() override = default;

  bool IsTraceLoggingEnabled(TraceCategory category) override { return true; }
  void LogTrace(TraceEvent event, Clock::time_point end_time) override {}
  void LogAsyncStart(TraceEvent event) override {}
  void LogAsyncEnd(TraceEvent event) override {}
};

class TraceLoggingActivationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // The test main can turn on tracing for the whole run (for example with
    // --tracing). These tests need to control tracing themselves.
    if (CurrentTracingDestination()) {
      GTEST_SKIP() << "Tracing is already active.";
    }
  }
};

TEST_F(TraceLoggingActivationTest, NoDestinationWhenTracingIsInactive) {
  const CurrentTracingDestination destination;
  EXPECT_FALSE(destination);
}

TEST_F(TraceLoggingActivationTest, ReturnsDestinationWhileTracing) {
  FakeTraceLoggingPlatform platform;
  StartTracing(&platform);
  {
    const CurrentTracingDestination destination;
    ASSERT_TRUE(destination);
    EXPECT_EQ(destination.operator->(), &platform);
  }
  StopTracing();

  const CurrentTracingDestination destination;
  EXPECT_FALSE(destination);
}

// A destination that was looked up while tracing was inactive holds no pin, so
// it must not keep StopTracing() waiting. If it did, StopTracing() would never
// return while `inactive_destination` is alive.
TEST_F(TraceLoggingActivationTest, InactiveDestinationDoesNotBlockStopTracing) {
  std::optional<CurrentTracingDestination> inactive_destination;
  inactive_destination.emplace();
  ASSERT_FALSE(*inactive_destination);

  FakeTraceLoggingPlatform platform;
  StartTracing(&platform);
  StopTracing();

  // The destination still reports the state from when it was created.
  EXPECT_FALSE(*inactive_destination);
  inactive_destination.reset();
}

}  // namespace
}  // namespace openscreen
