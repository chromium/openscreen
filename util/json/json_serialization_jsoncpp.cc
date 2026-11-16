// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "json/reader.h"
#include "json/writer.h"
#include "platform/base/error.h"
#include "util/json/json_serialization_internal.h"
#include "util/osp_logging.h"

namespace openscreen::json::internal {

ErrorOr<Value> ParseWithJsoncpp(std::string_view document) {
  Json::CharReaderBuilder builder;
  Json::CharReaderBuilder::strictMode(&builder.settings_);
  if (document.empty()) {
    return ErrorOr<Value>(Error::Code::kJsonParseError, "empty document");
  }

  Json::Value root_node;
  std::string error_msg;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  const bool succeeded = reader->parse(&*document.begin(), &*document.end(),
                                       &root_node, &error_msg);
  if (!succeeded) {
    return ErrorOr<Value>(Error::Code::kJsonParseError, error_msg);
  }

  return Value(root_node);
}

ErrorOr<std::string> StringifyWithJsoncpp(const Value& value) {
  Json::StreamWriterBuilder factory;
#ifndef _DEBUG
  // Default is to "pretty print" the output JSON in a human readable
  // format. On non-debug builds, we can remove pretty printing by simply
  // getting rid of all indentation.
  factory["indentation"] = "";
#endif

  std::unique_ptr<Json::StreamWriter> const writer(factory.newStreamWriter());
  std::ostringstream stream;
  const ::Json::Value jsoncpp_value = static_cast<::Json::Value>(value);
  writer->write(jsoncpp_value, &stream);

  if (!stream) {
    // Note: jsoncpp doesn't give us more information about what actually
    // went wrong, just says to "check the stream". However, failures on
    // the stream should be rare, as we do not throw any errors in the jsoncpp
    // library.
    return ErrorOr<std::string>(Error::Code::kJsonWriteError, "Invalid stream");
  }

  return stream.str();
}

}  // namespace openscreen::json::internal
