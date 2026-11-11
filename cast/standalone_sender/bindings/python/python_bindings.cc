// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>

#include "cast/standalone_sender/bindings/python/cast_runtime.h"
#include "cast/standalone_sender/bindings/python/sender_bridge.h"
#include "cast/standalone_sender/testing/video_packet_dropper.h"
#include "platform/api/time.h"

namespace py = pybind11;

using openscreen::cast::CastRuntime;
using openscreen::cast::CastSenderBridge;
using openscreen::cast::PacketDropConfig;

PYBIND11_MODULE(cast_sender, m) {
  m.doc() = "Python bindings for Open Screen Standalone Cast Sender.";

  py::class_<CastRuntime, std::shared_ptr<CastRuntime>>(m, "CastRuntime")
      .def(py::init([]() {
             static std::weak_ptr<CastRuntime> weak_instance;
             if (weak_instance.lock()) {
               throw std::runtime_error("CastRuntime already exists.");
             }
             auto instance = std::make_shared<CastRuntime>();
             weak_instance = instance;
             return instance;
           }),
           "Initialize the Open Screen environment event loop.");

  py::class_<PacketDropConfig>(
      m, "PacketDropConfig",
      "Configuration for bursty Wi-Fi video RTP packet drop simulation.")
      .def(py::init([](int64_t start_delay_ms, int64_t total_duration_ms,
                       double drop_ratio, int64_t burst_duration_ms,
                       uint32_t seed) {
             return PacketDropConfig{
                 .start_delay = std::chrono::milliseconds(start_delay_ms),
                 .total_duration = std::chrono::milliseconds(total_duration_ms),
                 .drop_ratio = drop_ratio,
                 .burst_duration = std::chrono::milliseconds(burst_duration_ms),
                 .seed = seed,
             };
           }),
           py::arg("start_delay_ms"), py::arg("total_duration_ms"),
           py::arg("drop_ratio") = PacketDropConfig::kDefaultDropRatio,
           py::arg("burst_duration_ms") =
               PacketDropConfig::kDefaultBurstDuration.count(),
           py::arg("seed") = PacketDropConfig::kDefaultSeed,
           "Initialize bursty video packet drop parameters.")
      .def_property_readonly(
          "start_delay_ms",
          [](const PacketDropConfig& self) { return self.start_delay.count(); },
          "Delay in milliseconds after the first video packet before packet "
          "drops begin.")
      .def_property_readonly(
          "total_duration_ms",
          [](const PacketDropConfig& self) {
            return self.total_duration.count();
          },
          "Total duration in milliseconds of the active packet drop window.")
      .def_readonly("drop_ratio", &PacketDropConfig::drop_ratio,
                    "Target fraction of active window spent in outage bursts "
                    "in (0.0, 1.0].")
      .def_property_readonly(
          "burst_duration_ms",
          [](const PacketDropConfig& self) {
            return self.burst_duration.count();
          },
          "Duration in milliseconds of each video RTP outage burst.")
      .def_readonly("seed", &PacketDropConfig::seed,
                    "Deterministic PRNG seed for outage burst intervals.");

  py::class_<CastSenderBridge::StreamConfig> stream_config(
      m, "StreamConfig", "Configuration options for streaming.");
  stream_config.def(py::init<>(), "Initialize the config.");
  stream_config.def_readwrite("file_path",
                              &CastSenderBridge::StreamConfig::file_path,
                              "Absolute path to the video file to stream.");
  stream_config.def_readwrite(
      "remote_transport_id",
      &CastSenderBridge::StreamConfig::remote_transport_id,
      "The remote transport ID to use.");
  stream_config.def_readwrite("session_id",
                              &CastSenderBridge::StreamConfig::session_id,
                              "The session ID to use.");
  stream_config.def_readwrite("codec_name",
                              &CastSenderBridge::StreamConfig::codec_name,
                              "The codec to use.");
  stream_config.def_readwrite("max_bitrate",
                              &CastSenderBridge::StreamConfig::max_bitrate,
                              "The maximum bitrate to use.");
  stream_config.def_readwrite(
      "should_include_audio",
      &CastSenderBridge::StreamConfig::should_include_audio,
      "Whether to include audio.");
  stream_config.def_readwrite(
      "packet_drop_config", &CastSenderBridge::StreamConfig::packet_drop_config,
      "Optional PacketDropConfig for bursty video packet drop simulation.");

  py::class_<CastSenderBridge>(m, "CastSender")
      .def(py::init([](CastRuntime& env, std::string_view ip_address, int port,
                       std::string_view cert_path) {
             const openscreen::ErrorOr<openscreen::IPAddress> ip =
                 openscreen::IPAddress::Parse(ip_address);
             if (!ip.is_value()) {
               throw std::invalid_argument("Invalid IP address provided: " +
                                           std::string(ip_address));
             }
             return std::make_unique<CastSenderBridge>(
                 env.task_runner(), ip.value(), port, cert_path);
           }),
           py::arg("env"), py::arg("ip_address"), py::arg("port"),
           py::arg("cert_path") = "", py::keep_alive<1, 2>(),
           "Initialize CastSender with target receiver IP, port, and "
           "optional cert path.")
      .def(
          "stream_video",
          [](CastSenderBridge& self,
             const CastSenderBridge::StreamConfig& config, bool is_debug) {
            bool success = self.StreamVideo(config, is_debug);
            if (!success) {
              throw std::runtime_error(
                  "StreamVideo failed to initialize or execute. See logs for "
                  "details.");
            }
            return true;
          },
          py::arg("config"), py::arg("is_debug") = false,
          py::call_guard<py::gil_scoped_release>(),
          "Establish connection and stream video file. Safely releases the "
          "Python GIL during execution.")
      .def(
          "get_streaming_elapsed_time_us",
          [](const CastSenderBridge& self) -> std::optional<int64_t> {
            const std::optional<openscreen::Clock::time_point> start_time =
                self.GetStreamingStartTime();
            if (!start_time.has_value()) {
              return std::nullopt;
            }
            return (openscreen::Clock::now() - *start_time).count();
          },
          py::call_guard<py::gil_scoped_release>(),
          "Returns the elapsed duration (in microseconds) since the first "
          "frame was scheduled to be captured (negative during the initial "
          "pre-capture delay), or None if streaming has not started yet.")
      .def("teardown", &CastSenderBridge::Teardown,
           py::call_guard<py::gil_scoped_release>(),
           "Teardown the streaming task runner and gracefully stop active "
           "streaming.");
}
