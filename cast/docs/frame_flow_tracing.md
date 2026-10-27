# Frame Flow Tracing in Open Screen

Open Screen utilizes **Flow Events** to track the lifecycle of audio and video
frames as they travel through the system. This allows developers to visualize
the journey of a single frame from generation (at the Sender) to presentation
(at the Receiver) across thread and process boundaries in the tracing tool of
the embedder's choice.

*** aside
The concept of flows in Open Screen is inspired by Perfetto, which is
the default tracing tool of the Chromium project as well as other consumers of
libcast. Flows as a concept may or may not be applicable to a given tracing
tool.
***

## The Concept

A "Flow" connects distinct trace events that share a common flow ID. Tracing
backends such as Perfetto treat flow IDs as global to the whole trace, while
the natural per-frame identifiers in Cast Streaming (RTP timestamps and
`FrameId`s) start from zero for every stream. To keep audio, video, and
concurrent sessions from colliding, flow IDs are always computed with the
helpers in [`cast/streaming/public/frame_trace_ids.h`](../streaming/public/frame_trace_ids.h),
which namespace the identifier by the stream's sender SSRC
(`SessionConfig::sender_ssrc`) and by identifier kind:

* **`GetRtpTimestampFlowId(sender_ssrc, rtp_timestamp)`**: Used for the
  sender-side flow of a frame. The RTP timestamp is known before the `FrameId`
  is assigned (e.g., during capture and encode), and is known on both sides of
  the network.
* **`GetFrameIdFlowId(sender_ssrc, frame_id)`**: Used for the receiver-side
  flow of a frame, from reassembly through decode, render, and playout.

The two flows are joined at the moment the Receiver completes reassembly of the
frame: the Receiver emits both `Frame.Complete.Rtp` (on the sender's RTP-keyed
flow) and `Frame.Complete` (starting the receiver's `FrameId`-keyed flow) at the
same timestamp.

## The Flow Lifecycle

### 1. Origin (Embedder Responsibility)

The flow typically begins when a frame is captured. Since this happens outside
the Open Screen library (in the Embedder's code), the Embedder is responsible
for providing the capture timestamps.

**Mechanism:** Provide `capture_begin_time` (and optionally
`capture_end_time`) in `EncodedFrame`.

When the frame is accepted by `Sender::EnqueueFrame()`, the library emits
`Frame.Capture` and `Frame.Capture.End`, back-dated to the provided capture
times. This ensures the flow visually starts at the correct moment of capture,
even though the frame is enqueued later. If `capture_begin_time` is not set, the
flow begins at `Frame.Enqueuing` instead.

```cpp
// Example: Inside your Encoder or Capture mechanism
EncodedFrame frame;
frame.capture_begin_time = my_capture_clock.now();
// ... encode, emitting encode steps on the same flow ...
const TraceId flow_id = GetRtpTimestampFlowId(
    sender->config().sender_ssrc, frame.rtp_timestamp);
TRACE_FLOW_STEP_WITH_TIME(TraceCategory::kSender, "Frame.Encode.Begin",
                          flow_id, encode_start_time);
// ...
sender->EnqueueFrame(frame);
// The library emits "Frame.Capture" back-dated to capture_begin_time.
```

### 2. Sender Library

Once the frame is accepted by the `Sender`, the library emits the following
events on the frame's RTP-keyed flow:

* **`Frame.Capture`**: The start of the flow, back-dated to
  `capture_begin_time` (if provided by the Embedder).
* **`Frame.Capture.End`**: The end of capture (if both capture times are
  provided).
* **`Frame.Enqueuing`**: `EnqueueFrame()` was called for the frame. This is the
  start of the flow if `capture_begin_time` was not provided.
* **`Frame.Enqueued`**: The frame has been encrypted, packetized, and entered
  the Sender's queue.
* **`Frame.Enqueuing.End`**: `EnqueueFrame()` has finished processing the
  frame.
* **`Frame.Acked`**: The Receiver has acknowledged receipt of this frame. This
  does not terminate the flow, so that Receiver-side events on the same flow
  (e.g., `Frame.Complete.Rtp`) still link up when the Sender's and Receiver's
  clocks are not perfectly synchronized.
* **`Frame.Cancelled`** (flow end): The frame was cancelled by
  `Sender::CancelInFlightData()` before being acknowledged. Frames that the
  Receiver skips are reported through its checkpoint, so they emit
  `Frame.Acked` instead.

Frames rejected by `EnqueueFrame()` do not emit any flow events.

### 3. Receiver Library

When packets arrive at the `Receiver`, the library tracks the reassembly and
availability of the frame:

* **`Frame.Complete.Rtp`**: All packets for this frame have been received and
  reassembled. Emitted on the sender's RTP-keyed flow, to join the sender's and
  receiver's traces.
* **`Frame.Complete`** (flow begin): Same as above, but starting the frame's
  receiver-side `FrameId`-keyed flow.
* **`Frame.Ready`**: The frame is the next in sequence (or an independent frame
  being skipped to) and is ready for the application to consume.
* **`Frame.Consumed`**: The application has popped the frame from the queue.
* **`Frame.PlayedOut`** (flow end): The application called
  `ReportPlayoutEvent()` for the frame.

### 4. Presentation (Embedder Responsibility)

After the frame leaves the Receiver library, the Embedder (Player) takes over.
To complete the picture, Embedders should continue the receiver-side flow
through decoding and rendering.

**Macro:** `TRACE_FLOW_STEP`

```cpp
// Example: Inside your Player's OnFramesReady()
EncodedFrame frame = receiver.ConsumeNextFrame(buffer);
// 'Frame.Consumed' is emitted by the library here.
const TraceId flow_id =
    GetFrameIdFlowId(receiver.config().sender_ssrc, frame.frame_id);

// 1. Embedder acknowledges receipt
TRACE_FLOW_STEP(TraceCategory::kReceiver, "Frame.Received", flow_id);

// ... Decoding ...

// 2. Rendering Start
TRACE_FLOW_STEP(TraceCategory::kReceiver, "Frame.Render.Begin", flow_id);

// ... Rendering ...

// 3. Rendering End
TRACE_FLOW_STEP(TraceCategory::kReceiver, "Frame.Render.End", flow_id);

// 4. Playout (Flow End)
// Call ReportPlayoutEvent() to notify the library. The library will emit
// TRACE_FLOW_END_WITH_TIME("Frame.PlayedOut", ..., playout_time).
receiver.ReportPlayoutEvent(frame.frame_id, frame.rtp_timestamp, playout_time);
```

## Debugging & Diagnostics

By visualizing these flows in a tool like
[ui.perfetto.dev](https://ui.perfetto.dev/), you can diagnose:

* **Capture/Encode Latency**: Time between `Frame.Capture` and
  `Frame.Enqueuing`.

* **Sender Blocking**: Long gaps between `Frame.Enqueuing` and
  `Frame.Enqueued`.

* **Network Latency**: Time between `Frame.Enqueued` and `Frame.Complete.Rtp`
  (approximate, since the sender and receiver clocks may differ).

* **Receiver Queueing**: Time between `Frame.Complete` and `Frame.Ready`.
  * *High duration here suggests the Receiver application is not pulling frames
    fast enough or the jitter buffer is holding them.*

* **Application Latency**: Time between `Frame.Ready` and `Frame.PlayedOut`.

## Integration Tips

1. **Use the flow ID helpers**: Always derive flow IDs with
   `GetRtpTimestampFlowId()` or `GetFrameIdFlowId()` from
   `cast/streaming/public/frame_trace_ids.h`. Do not use raw RTP timestamps or
   `FrameId::value()` as flow IDs, since they collide across streams.

2. **Category Consistency**: While flows can span categories, it is helpful to
   use `TraceCategory::kSender` for sender-side events and
   `TraceCategory::kReceiver` for receiver-side events.
