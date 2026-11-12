// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/service_dispatcher.h"

#include "discovery/public/dns_sd_service_factory.h"
#include "platform/base/error.h"
#include "util/osp_logging.h"

namespace openscreen::discovery {

ServiceDispatcher::ServiceDispatcher(TaskRunner& task_runner,
                                     ReportingClient&,
                                     const Config& config)
    : task_runner_(task_runner),
      publisher_(config.enable_publication ? this : nullptr),
      querier_(config.enable_querying ? this : nullptr) {}

ServiceDispatcher::~ServiceDispatcher() {
  OSP_CHECK(task_runner_->IsRunningOnTaskRunner());
}

Error ServiceDispatcher::Register(const DnsSdInstance&,
                                  DnsSdPublisher::Client*) {
  OSP_CHECK(task_runner_->IsRunningOnTaskRunner());
  return Error::Code::kNotImplemented;
}

Error ServiceDispatcher::UpdateRegistration(const DnsSdInstance&) {
  OSP_CHECK(task_runner_->IsRunningOnTaskRunner());
  return Error::Code::kNotImplemented;
}

ErrorOr<int> ServiceDispatcher::DeregisterAll(const std::string&) {
  OSP_CHECK(task_runner_->IsRunningOnTaskRunner());
  return Error::Code::kNotImplemented;
}

void ServiceDispatcher::StartQuery(const std::string&, Callback* cb) {
  OSP_CHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_CHECK(cb);
}

void ServiceDispatcher::StopQuery(const std::string&, Callback*) {
  OSP_CHECK(task_runner_->IsRunningOnTaskRunner());
}

void ServiceDispatcher::ReinitializeQueries(const std::string&) {
  OSP_CHECK(task_runner_->IsRunningOnTaskRunner());
}

DnsSdServicePtr CreateDnsSdService(TaskRunner& task_runner,
                                   ReportingClient& reporting_client,
                                   const Config& config) {
  return TaskRunnerDeleter::MakeUnique<ServiceDispatcher>(
      task_runner, task_runner, reporting_client, config);
}

}  // namespace openscreen::discovery
