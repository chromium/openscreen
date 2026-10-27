// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_TESTING_FLOW_EVENT_RECORDER_H_
#define CAST_STREAMING_TESTING_FLOW_EVENT_RECORDER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "platform/api/time.h"
#include "platform/api/trace_event.h"
#include "platform/base/trace_logging_types.h"
#include "platform/test/trace_logging_helpers.h"

namespace openscreen::cast {

// Installs a mock tracing platform for its lifetime, and records all of the
// flow trace events (TRACE_FLOW_*) emitted while it is alive.
class FlowEventRecorder {
 public:
  struct Event {
    std::string name;
    uint64_t flow_id;
    FlowType type;
    Clock::time_point time;
  };

  FlowEventRecorder() {
    ON_CALL(platform_, LogFlow(::testing::_, ::testing::_))
        .WillByDefault([this](TraceEvent event, FlowType type) {
          ASSERT_EQ(1u, event.flow_ids.size());
          events_.push_back(
              {event.name, event.flow_ids[0], type, event.start_time});
        });
  }

  // Not copyable or movable, since the mock's action captures `this`.
  FlowEventRecorder(const FlowEventRecorder&) = delete;
  FlowEventRecorder& operator=(const FlowEventRecorder&) = delete;

  const std::vector<Event>& events() const { return events_; }

  // Returns the events emitted for `flow_id`, in emission order.
  std::vector<Event> EventsFor(uint64_t flow_id) const {
    std::vector<Event> result;
    for (const Event& event : events_) {
      if (event.flow_id == flow_id) {
        result.push_back(event);
      }
    }
    return result;
  }

  // Returns the names of the events emitted for `flow_id`, in emission order.
  // Consecutive duplicates are collapsed into one, since some events (e.g.,
  // "Frame.Ready") may be legitimately emitted more than once in a row.
  std::vector<std::string> NamesFor(uint64_t flow_id) const {
    std::vector<std::string> names;
    for (const Event& event : EventsFor(flow_id)) {
      if (names.empty() || names.back() != event.name) {
        names.push_back(event.name);
      }
    }
    return names;
  }

 private:
  ::testing::NiceMock<MockLoggingPlatform> platform_;
  std::vector<Event> events_;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_TESTING_FLOW_EVENT_RECORDER_H_
