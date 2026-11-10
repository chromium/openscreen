// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/udp_socket_posix.h"

#include <sys/socket.h>

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/fake_udp_socket.h"

namespace openscreen {
namespace {

using testing::_;

TEST(UdpSocketPosixTest, SetsBufferSizes) {
  const uint8_t kIpV4AddrAny[4] = {};
  FakeClock clock(Clock::now());
  FakeTaskRunner task_runner(clock);
  testing::StrictMock<FakeUdpSocket::MockClient> client;
  ErrorOr<std::unique_ptr<UdpSocket>> create_result = UdpSocket::Create(
      task_runner, &client, IPEndpoint{IPAddress(kIpV4AddrAny), 0});
  ASSERT_TRUE(create_result) << create_result.error();
  const auto socket = std::move(create_result.value());
  EXPECT_CALL(client, OnBound(_)).Times(1);
  socket->Bind();

  constexpr size_t kTestReceiveBufferSize = 10000;
  constexpr size_t kTestSendBufferSize = 20000;
  socket->SetReceiveBufferSize(kTestReceiveBufferSize);
  socket->SetSendBufferSize(kTestSendBufferSize);

  int rcvbuf = 0;
  socklen_t optlen = sizeof(rcvbuf);
  int res =
      getsockopt(static_cast<UdpSocketPosix*>(socket.get())->GetHandle().fd,
                 SOL_SOCKET, SO_RCVBUF, &rcvbuf, &optlen);
  ASSERT_EQ(res, 0);
  // Linux kernel doubles the requested buffer size for bookkeeping, or sets
  // it to at least the requested value.
  EXPECT_GE(static_cast<size_t>(rcvbuf), kTestReceiveBufferSize);

  int sndbuf = 0;
  optlen = sizeof(sndbuf);
  res = getsockopt(static_cast<UdpSocketPosix*>(socket.get())->GetHandle().fd,
                   SOL_SOCKET, SO_SNDBUF, &sndbuf, &optlen);
  ASSERT_EQ(res, 0);
  EXPECT_GE(static_cast<size_t>(sndbuf), kTestSendBufferSize);
}

// Tests that binding to an ephemeral port resolves the OS-assigned port, and
// that the socket's IP version is unaffected by resolving the local endpoint.
TEST(UdpSocketPosixTest, BindResolvesEphemeralPort) {
  const uint8_t kIpV4Loopback[4] = {127, 0, 0, 1};
  FakeClock clock(Clock::now());
  FakeTaskRunner task_runner(clock);
  testing::StrictMock<FakeUdpSocket::MockClient> client;
  ErrorOr<std::unique_ptr<UdpSocket>> create_result = UdpSocket::Create(
      task_runner, &client, IPEndpoint{IPAddress(kIpV4Loopback), 0});
  ASSERT_TRUE(create_result) << create_result.error();
  const auto socket = std::move(create_result.value());
  ASSERT_TRUE(socket->IsIPv4());

  IPEndpoint endpoint_when_bound;
  EXPECT_CALL(client, OnBound(socket.get())).WillOnce([&](UdpSocket* s) {
    endpoint_when_bound = s->GetLocalEndpoint();
  });
  socket->Bind();

  EXPECT_NE(0, endpoint_when_bound.port);
  EXPECT_EQ(IPAddress(kIpV4Loopback), endpoint_when_bound.address);
  EXPECT_EQ(endpoint_when_bound, socket->GetLocalEndpoint());
  EXPECT_TRUE(socket->IsIPv4());
  EXPECT_FALSE(socket->IsIPv6());
}

// Creates a UDP socket for `endpoint` that reports to `client`.
std::unique_ptr<UdpSocket> CreateSocket(TaskRunner& task_runner,
                                        UdpSocket::Client& client,
                                        const IPEndpoint& endpoint) {
  ErrorOr<std::unique_ptr<UdpSocket>> result =
      UdpSocket::Create(task_runner, &client, endpoint);
  EXPECT_TRUE(result) << result.error();
  return result ? std::move(result.value()) : nullptr;
}

int GetSocketOption(const UdpSocket& socket, int option) {
  int value = -1;
  socklen_t optlen = sizeof(value);
  const int res =
      getsockopt(static_cast<const UdpSocketPosix&>(socket).GetHandle().fd,
                 SOL_SOCKET, option, &value, &optlen);
  EXPECT_EQ(res, 0);
  return value;
}

TEST(UdpSocketPosixTest, DoesNotSetReuseAddressByDefault) {
  FakeClock clock(Clock::now());
  FakeTaskRunner task_runner(clock);
  testing::StrictMock<FakeUdpSocket::MockClient> client;
  const auto socket = CreateSocket(
      task_runner, client, IPEndpoint{IPAddress::kV4LoopbackAddress(), 0});
  ASSERT_TRUE(socket);
  EXPECT_CALL(client, OnBound(_)).Times(1);
  socket->Bind();

  EXPECT_EQ(GetSocketOption(*socket, SO_REUSEADDR), 0);
#if BUILDFLAG(IS_APPLE)
  EXPECT_EQ(GetSocketOption(*socket, SO_REUSEPORT), 0);
#endif  // BUILDFLAG(IS_APPLE)
}

TEST(UdpSocketPosixTest, SetsReuseAddressWhenEnabled) {
  FakeClock clock(Clock::now());
  FakeTaskRunner task_runner(clock);
  testing::StrictMock<FakeUdpSocket::MockClient> client;
  const auto socket = CreateSocket(
      task_runner, client, IPEndpoint{IPAddress::kV4LoopbackAddress(), 0});
  ASSERT_TRUE(socket);
  socket->SetReuseAddress(true);
  EXPECT_CALL(client, OnBound(_)).Times(1);
  socket->Bind();

  EXPECT_NE(GetSocketOption(*socket, SO_REUSEADDR), 0);
#if BUILDFLAG(IS_APPLE)
  EXPECT_NE(GetSocketOption(*socket, SO_REUSEPORT), 0);
#endif  // BUILDFLAG(IS_APPLE)
}

// When address reuse is enabled on both sockets, they can bind the same port,
// as required by mDNS.
TEST(UdpSocketPosixTest, ReusableSocketsCanSharePort) {
  FakeClock clock(Clock::now());
  FakeTaskRunner task_runner(clock);

  testing::StrictMock<FakeUdpSocket::MockClient> first_client;
  const auto first_socket =
      CreateSocket(task_runner, first_client,
                   IPEndpoint{IPAddress::kV4LoopbackAddress(), 0});
  ASSERT_TRUE(first_socket);
  first_socket->SetReuseAddress(true);
  EXPECT_CALL(first_client, OnBound(_)).Times(1);
  first_socket->Bind();
  const IPEndpoint bound_endpoint = first_socket->GetLocalEndpoint();
  ASSERT_NE(bound_endpoint.port, 0);

  testing::StrictMock<FakeUdpSocket::MockClient> second_client;
  const auto second_socket =
      CreateSocket(task_runner, second_client, bound_endpoint);
  ASSERT_TRUE(second_socket);
  second_socket->SetReuseAddress(true);
  EXPECT_CALL(second_client, OnBound(_)).Times(1);
  second_socket->Bind();
}

// When address reuse is disabled, the port assigned by the OS must not be
// shareable with another socket, even one that requests address reuse.
TEST(UdpSocketPosixTest, NonReusableSocketPortCannotBeShared) {
  FakeClock clock(Clock::now());
  FakeTaskRunner task_runner(clock);

  testing::StrictMock<FakeUdpSocket::MockClient> first_client;
  const auto first_socket =
      CreateSocket(task_runner, first_client,
                   IPEndpoint{IPAddress::kV4LoopbackAddress(), 0});
  ASSERT_TRUE(first_socket);
  EXPECT_CALL(first_client, OnBound(_)).Times(1);
  first_socket->Bind();
  const IPEndpoint bound_endpoint = first_socket->GetLocalEndpoint();
  ASSERT_NE(bound_endpoint.port, 0);

  testing::StrictMock<FakeUdpSocket::MockClient> second_client;
  const auto second_socket =
      CreateSocket(task_runner, second_client, bound_endpoint);
  ASSERT_TRUE(second_socket);
  second_socket->SetReuseAddress(true);
  EXPECT_CALL(second_client, OnError(_, _)).Times(1);
  second_socket->Bind();
}

}  // namespace
}  // namespace openscreen
