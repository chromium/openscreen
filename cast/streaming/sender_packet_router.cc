// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/sender_packet_router.h"

#include <algorithm>
#include <utility>

#include "cast/streaming/impl/packet_util.h"
#include "cast/streaming/public/constants.h"
#include "platform/base/byte_span.h"
#include "util/chrono_helpers.h"
#include "util/osp_logging.h"
#include "util/saturate_cast.h"
#include "util/string_util.h"
#include "util/trace_logging.h"

namespace openscreen::cast {

using clock_operators::operator<<;

SenderPacketRouter::SenderPacketRouter(Environment& environment,
                                       int max_burst_bitrate)
    : SenderPacketRouter(
          environment,
          ComputeMaxPacketsPerBurst(max_burst_bitrate,
                                    environment.GetMaxPacketSize(),
                                    kDefaultBurstInterval),
          kDefaultBurstInterval) {}

SenderPacketRouter::SenderPacketRouter(Environment& environment,
                                       int max_packets_per_burst,
                                       milliseconds burst_interval)
    : BandwidthEstimator(max_packets_per_burst * environment.GetMaxPacketSize(),
                         burst_interval,
                         environment.now()),
      environment_(environment),
      packet_buffer_size_(environment.GetMaxPacketSize()),
      packet_buffer_(new uint8_t[packet_buffer_size_]),
      max_packets_per_burst_(max_packets_per_burst),
      burst_interval_(burst_interval),
      max_burst_bitrate_(ComputeMaxBurstBitrate(packet_buffer_size_,
                                                max_packets_per_burst_,
                                                burst_interval_)),
      alarm_(environment_->now_function(), environment_->task_runner()) {
  OSP_CHECK_GT(packet_buffer_size_, kRequiredNetworkPacketSize);
}

SenderPacketRouter::~SenderPacketRouter() {
  OSP_CHECK(senders_.empty());
}

void SenderPacketRouter::OnSenderCreated(Ssrc receiver_ssrc, Sender* sender) {
  OSP_CHECK(FindEntry(receiver_ssrc) == senders_.end());
  senders_.push_back(SenderEntry{receiver_ssrc, sender, kNever, kNever});

  if (senders_.size() == 1) {
    environment_->ConsumeIncomingPackets(this);
  } else {
    // Sort the list of Senders so that they are iterated in priority order.
    std::sort(senders_.begin(), senders_.end());
  }
}

void SenderPacketRouter::OnSenderDestroyed(Ssrc receiver_ssrc) {
  const auto it = FindEntry(receiver_ssrc);
  OSP_CHECK(it != senders_.end());
  senders_.erase(it);

  // If there are no longer any Senders, suspend receiving RTCP packets.
  if (senders_.empty()) {
    environment_->DropIncomingPackets();
  }
}

void SenderPacketRouter::RequestRtcpSend(Ssrc receiver_ssrc) {
  const auto it = FindEntry(receiver_ssrc);
  OSP_CHECK(it != senders_.end());
  it->next_rtcp_send_time = Alarm::kImmediately;
  ScheduleNextBurst();
}

void SenderPacketRouter::RequestRtpSend(Ssrc receiver_ssrc) {
  const auto it = FindEntry(receiver_ssrc);
  OSP_CHECK(it != senders_.end());
  it->next_rtp_send_time = Alarm::kImmediately;
  ScheduleNextBurst();
}

void SenderPacketRouter::OnReceivedPacket(const IPEndpoint& source,
                                          Clock::time_point arrival_time,
                                          std::vector<uint8_t> packet) {
  // If the packet did not come from the expected endpoint, ignore it.
  OSP_CHECK_NE(source.port, uint16_t{0});
  if (source != environment_->remote_endpoint()) {
    return;
  }

  // Determine which Sender to dispatch the packet to. Senders may only receive
  // RTCP packets from Receivers. Log a warning containing a pretty-printed dump
  // if the packet is not an RTCP packet.
  const std::pair<ApparentPacketType, Ssrc> seems_like =
      InspectPacketForRouting(packet, rtp_parser_version_);
  if (seems_like.first != ApparentPacketType::RTCP) {
    constexpr int kMaxPartiaHexDumpSize = 96;
    const std::size_t encode_size =
        std::min(packet.size(), static_cast<size_t>(kMaxPartiaHexDumpSize));
    OSP_LOG_WARN << "UNKNOWN packet of " << packet.size()
                 << " bytes. Partial hex dump: "
                 << HexEncode(packet.data(), encode_size);
    return;
  }
  const auto it = FindEntry(seems_like.second);
  if (it != senders_.end()) {
    it->sender->OnReceivedRtcpPacket(arrival_time, std::move(packet));
  }
}

SenderPacketRouter::SenderEntries::iterator SenderPacketRouter::FindEntry(
    Ssrc receiver_ssrc) {
  return std::find_if(senders_.begin(), senders_.end(),
                      [receiver_ssrc](const SenderEntry& entry) {
                        return entry.receiver_ssrc == receiver_ssrc;
                      });
}

void SenderPacketRouter::ScheduleNextBurst() {
  // Determine the next burst time by scanning for the earliest of the
  // next-scheduled send times for each Sender.
  const Clock::time_point earliest_allowed_burst_time =
      last_burst_time_ + burst_interval_;
  Clock::time_point next_burst_time = kNever;
  for (const SenderEntry& entry : senders_) {
    const auto next_send_time =
        std::min(entry.next_rtcp_send_time, entry.next_rtp_send_time);
    if (next_send_time >= next_burst_time) {
      continue;
    }
    if (next_send_time <= earliest_allowed_burst_time) {
      next_burst_time = earliest_allowed_burst_time;
      // No need to continue, since `next_burst_time` cannot become any earlier.
      break;
    }
    next_burst_time = next_send_time;
  }

  // Schedule the alarm for the next burst time unless none of the Senders has
  // anything to send.
  if (next_burst_time == kNever) {
    alarm_.Cancel();
  } else {
    alarm_.Schedule([this] { SendBurstOfPackets(); }, next_burst_time);
  }
}

void SenderPacketRouter::SendBurstOfPackets() {
  TRACE_DEFAULT_SCOPED(TraceCategory::kSender);
  // Treat RTCP packets as "critical priority," and so there is no upper limit
  // on the number to send. Practically, this will always be limited by the
  // number of Senders; so, this won't be a huge number of packets.
  const Clock::time_point burst_time = environment_->now();
  const SendCounts rtcp_sent = SendJustTheRtcpPackets(burst_time);
  // Now send all the RTP packets, up to the maximum number allowed in a burst.
  // Higher priority Senders' RTP packets are sent first.
  const SendCounts rtp_sent = SendJustTheRtpPackets(
      burst_time, max_packets_per_burst_ - rtcp_sent.num_packets);
  last_burst_time_ = burst_time;

  BandwidthEstimator::OnBurstComplete(rtcp_sent.num_bytes + rtp_sent.num_bytes,
                                      burst_time);

  ScheduleNextBurst();
}

SenderPacketRouter::SendCounts SenderPacketRouter::SendJustTheRtcpPackets(
    Clock::time_point send_time) {
  SendCounts sent;
  for (SenderEntry& entry : senders_) {
    if (entry.next_rtcp_send_time > send_time) {
      continue;
    }

    // Note: Only one RTCP packet is sent from the same Sender in the same
    // burst. This is because RTCP packets are supposed to always contain the
    // most up-to-date Sender state. Having multiple RTCP packets in the same
    // burst would mean that all but the last one are old/irrelevant snapshots
    // of Sender state, and this would just thrash/confuse the Receiver.
    const ByteBuffer packet = entry.sender->GetRtcpPacketForImmediateSend(
        send_time, ByteBuffer(packet_buffer_.get(), packet_buffer_size_));
    if (!packet.empty()) {
      environment_->SendPacket(
          ByteView(packet.data(), packet.size()),
          PacketMetadata{.stream_type = entry.sender->GetStreamType(),
                         .rtp_timestamp = entry.sender->GetLastRtpTimestamp()});
      entry.next_rtcp_send_time = send_time + kRtcpReportInterval;
      ++sent.num_packets;
      sent.num_bytes += static_cast<int>(packet.size());
    }
  }

  return sent;
}

SenderPacketRouter::SendCounts SenderPacketRouter::SendJustTheRtpPackets(
    Clock::time_point send_time,
    int num_packets_to_send) {
  TRACE_DEFAULT_SCOPED(TraceCategory::kSender);
  SendCounts sent;
  for (SenderEntry& entry : senders_) {
    if (sent.num_packets >= num_packets_to_send) {
      break;
    }
    if (entry.next_rtp_send_time > send_time) {
      continue;
    }

    for (; sent.num_packets < num_packets_to_send; ++sent.num_packets) {
      const ByteBuffer packet = entry.sender->GetRtpPacketForImmediateSend(
          send_time, ByteBuffer(packet_buffer_.get(), packet_buffer_size_));
      if (packet.empty()) {
        break;
      }
      environment_->SendPacket(
          ByteView(packet.data(), packet.size()),
          PacketMetadata{.stream_type = entry.sender->GetStreamType(),
                         .rtp_timestamp = entry.sender->GetLastRtpTimestamp(),
                         .is_retransmission =
                             entry.sender->WasLastRtpPacketRetransmission()});
      sent.num_bytes += static_cast<int>(packet.size());
    }
    entry.next_rtp_send_time = entry.sender->GetRtpResumeTime();
  }

  return sent;
}

namespace {
constexpr int kBitsPerByte = 8;
constexpr auto kOneSecondInMilliseconds = to_milliseconds(seconds(1));
}  // namespace

// static
int SenderPacketRouter::ComputeMaxPacketsPerBurst(int max_burst_bitrate,
                                                  int packet_size,
                                                  milliseconds burst_interval) {
  OSP_CHECK_GT(max_burst_bitrate, 0);
  OSP_CHECK_GT(packet_size, 0);
  OSP_CHECK_GT(burst_interval, milliseconds(0));
  OSP_CHECK_LE(burst_interval, kOneSecondInMilliseconds);

  const int max_packets_per_second =
      max_burst_bitrate / kBitsPerByte / packet_size;
  const int bursts_per_second = kOneSecondInMilliseconds / burst_interval;
  return std::max(max_packets_per_second / bursts_per_second, 1);
}

// static
int SenderPacketRouter::ComputeMaxBurstBitrate(int packet_size,
                                               int max_packets_per_burst,
                                               milliseconds burst_interval) {
  OSP_CHECK_GT(packet_size, 0);
  OSP_CHECK_GT(max_packets_per_burst, 0);
  OSP_CHECK_GT(burst_interval, milliseconds(0));
  OSP_CHECK_LE(burst_interval, kOneSecondInMilliseconds);

  const int64_t max_bits_per_burst =
      int64_t{packet_size} * kBitsPerByte * max_packets_per_burst;
  const int bursts_per_second = kOneSecondInMilliseconds / burst_interval;
  return saturate_cast<int>(max_bits_per_burst * bursts_per_second);
}

SenderPacketRouter::Sender::~Sender() = default;

// static
constexpr int SenderPacketRouter::kDefaultMaxBurstBitrate;
// static
constexpr milliseconds SenderPacketRouter::kDefaultBurstInterval;
// static
constexpr Clock::time_point SenderPacketRouter::kNever;

}  // namespace openscreen::cast
