// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_SERVICE_DISPATCHER_H_
#define DISCOVERY_DNSSD_IMPL_SERVICE_DISPATCHER_H_

#include <string>

#include "discovery/common/config.h"
#include "discovery/common/reporting_client.h"
#include "discovery/dnssd/public/dns_sd_instance.h"
#include "discovery/dnssd/public/dns_sd_publisher.h"
#include "discovery/dnssd/public/dns_sd_querier.h"
#include "discovery/dnssd/public/dns_sd_service.h"
#include "platform/api/task_runner.h"
#include "util/raw_ref.h"

namespace openscreen::discovery {

class ServiceDispatcher final : public DnsSdService,
                                public DnsSdPublisher,
                                public DnsSdQuerier {
 public:
  ServiceDispatcher(TaskRunner& task_runner,
                    ReportingClient& reporting_client,
                    const Config& config);
  ~ServiceDispatcher() override;

  // DnsSdService overrides.
  DnsSdQuerier* GetQuerier() override { return querier_; }
  DnsSdPublisher* GetPublisher() override { return publisher_; }

 private:
  // DnsSdPublisher overrides.
  Error Register(const DnsSdInstance& instance,
                 DnsSdPublisher::Client* client) override;
  Error UpdateRegistration(const DnsSdInstance& instance) override;
  ErrorOr<int> DeregisterAll(const std::string& service) override;

  // DnsSdQuerier overrides.
  void StartQuery(const std::string& service, Callback* cb) override;
  void StopQuery(const std::string& service, Callback* cb) override;
  void ReinitializeQueries(const std::string& service) override;

  const raw_ref<TaskRunner> task_runner_;
  DnsSdPublisher* const publisher_;
  DnsSdQuerier* const querier_;
};

}  // namespace openscreen::discovery

#endif  // DISCOVERY_DNSSD_IMPL_SERVICE_DISPATCHER_H_
