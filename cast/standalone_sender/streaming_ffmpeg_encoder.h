// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_SENDER_STREAMING_FFMPEG_ENCODER_H_
#define CAST_STANDALONE_SENDER_STREAMING_FFMPEG_ENCODER_H_

#include <condition_variable>  // NOLINT
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "cast/standalone_common/ffmpeg_glue.h"
#include "cast/standalone_sender/streaming_video_encoder.h"
#include "cast/streaming/public/constants.h"
#include "cast/streaming/public/frame_id.h"
#include "cast/streaming/rtp_time.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/base/trace_logging_types.h"
#include "util/thread_annotations.h"
#include "util/weak_ptr.h"

namespace openscreen {

class TaskRunner;

namespace cast {

class Sender;

// libx265 does not support updating the bitrate on an open AVCodecContext.
// Recreate the encoder immediately when the bitrate changes by 2x or more, or
// once `target_bitrate` stays unchanged for 60 frames (~2s at 30 FPS) while
// differing from `current_bitrate` by at least 20%.
inline constexpr int64_t kHevcBitrateReinitRatio = 2;
inline constexpr int kHevcBitrateNumFramesBeforeSettling = 60;
inline constexpr double kHevcBitrateSettlingFactor = 0.2;

bool ShouldReinitHevcEncoder(int64_t current_bitrate,
                             int64_t target_bitrate,
                             int stable_target_frames);

// Uses FFmpeg (libavcodec) to encode H.264 or H.265 (HEVC) video and streams it
// to a Sender. Includes logic for fine-tuning the encoder parameters in
// real-time to provide the best quality results given external, uncontrollable
// factors: CPU/network availability, and the complexity of the video frame
// content.
//
// Internally, a separate encode thread is created and used to prevent blocking
// the main thread while frames are being encoded. All public API methods are
// assumed to be called on the same sequence/thread as the main TaskRunner
// (injected via the constructor).
//
// Usage:
//
// 1. EncodeAndSend() is used to queue-up video frames for encoding and sending,
// which will be done on a best-effort basis.
//
// 2. The client is expected to call SetTargetBitrate() frequently based on its
// own bandwidth estimates and congestion control logic. In addition, a client
// may provide a callback for each frame's encode statistics, which can be used
// to further optimize the user experience. For example, the stats can be used
// as a signal to reduce the data volume (i.e., resolution and/or frame rate)
// coming from the video capture source.
class StreamingFfmpegEncoder : public StreamingVideoEncoder {
 public:
  StreamingFfmpegEncoder(const Parameters& params,
                         TaskRunner& task_runner,
                         std::unique_ptr<Sender> sender);

  ~StreamingFfmpegEncoder() override;

  int GetTargetBitrate() const override;
  void SetTargetBitrate(int new_bitrate) override;
  void EncodeAndSend(const VideoFrame& frame,
                     Clock::time_point reference_time,
                     std::function<void(Stats)> stats_callback) override;

 private:
  // Represents the state of one frame encode. This is created in
  // EncodeAndSend(), and passed to the encode thread via the `encode_queue_`.
  struct WorkUnit {
    AVFrameUniquePtr image;
    Clock::duration duration;
    Clock::time_point capture_begin_time;
    Clock::time_point capture_end_time;
    Clock::time_point reference_time;
    RtpTimeTicks rtp_timestamp;
    // Trace flow ID for this frame (see
    // cast/streaming/public/frame_trace_ids.h).
    TraceId flow_id = kUnsetTraceId;
    std::function<void(Stats)> stats_callback;
  };

  // Same as WorkUnit, but with additional fields to carry the encode results.
  struct WorkUnitWithResults : public WorkUnit {
    WorkUnitWithResults& operator=(WorkUnit&& base) {
      WorkUnit::operator=(std::move(base));
      return *this;
    }

    std::vector<uint8_t> payload;
    bool is_key_frame = false;
    Stats stats;
  };

  // Groups the FFmpeg libavcodec encoder context and output packet. Only the
  // encode thread accesses this instance.
  struct EncoderContext {
    AVCodecContextUniquePtr context;
    AVPacketUniquePtr packet;
    int last_target_bitrate = 0;
    int stable_target_frames = 0;
  };

  bool is_encoder_initialized() const { return encoder_ != nullptr; }

  // Destroys the FFmpeg encoder context if it has been initialized.
  void DestroyEncoder();

  // The procedure for the `encode_thread_` that loops, processing work units
  // from the `encode_queue_` by calling EncodeFrame() until it's time to end
  // the thread.
  void ProcessWorkUnitsUntilTimeToQuit() OSP_NO_THREAD_SAFETY_ANALYSIS;

  // If the `encoder_` is live, attempt reconfiguration to allow it to encode
  // frames at a new frame size or target bitrate. If reconfiguration is not
  // possible, destroy the existing instance and re-create a new encoder
  // instance.
  void PrepareEncoder(int width, int height, int target_bitrate);

  // Wraps the FFmpeg avcodec_send_frame() and avcodec_receive_packet() calls
  // using inputs from `work_unit` and populating results there.
  void EncodeFrame(bool force_key_frame, WorkUnitWithResults& work_unit);

  // Computes and populates `work_unit.stats` after the call to EncodeFrame().
  void ComputeFrameEncodeStats(Clock::duration encode_wall_time,
                               int target_bitrate,
                               WorkUnitWithResults& work_unit);

  // Assembles and enqueues an EncodedFrame with the Sender on the main thread.
  void SendEncodedFrame(WorkUnitWithResults results);

  // The reference time of the first frame passed to EncodeAndSend().
  Clock::time_point start_time_ = Clock::time_point::min();

  // The RTP timestamp of the last frame that was pushed into the
  // `encode_queue_` by EncodeAndSend(). This is used to check whether
  // timestamps are monotonically increasing.
  RtpTimeTicks last_enqueued_rtp_timestamp_;

  // Guards a few members shared by both the main and encode threads.
  mutable std::mutex mutex_;

  // Used by the encode thread to sleep until more work is available.
  std::condition_variable cv_;

  // These encode parameters are not passed in the WorkUnit struct because it is
  // desirable for them to be applied as soon as possible, with the very next
  // WorkUnit popped from the `encode_queue_` on the encode thread, and not to
  // wait until some later WorkUnit is processed.
  bool needs_key_frame_ OSP_GUARDED_BY(mutex_) = true;
  int target_bitrate_ OSP_GUARDED_BY(mutex_) = 5000000;  // Default: 5 Mbps.

  // The queue of frame encodes. The size of this queue is implicitly bounded by
  // EncodeAndSend(), where it checks for the total in-flight media duration and
  // maybe drops a frame.
  std::queue<WorkUnit> encode_queue_ OSP_GUARDED_BY(mutex_);

  // Active FFmpeg encoder session.
  std::unique_ptr<EncoderContext> encoder_;

  WeakPtrFactory<StreamingFfmpegEncoder> weak_factory_{this};
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_SENDER_STREAMING_FFMPEG_ENCODER_H_
