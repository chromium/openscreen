// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_RUNTIME_OPTIONS_H_
#define PLATFORM_API_RUNTIME_OPTIONS_H_

namespace openscreen {

// Options that let the embedder change library behavior at runtime, without a
// rebuild. For example, an embedder can use them to roll out a new
// implementation behind a feature flag, or to pass experiment settings into
// Open Screen.
//
// The default value of each field is the library's recommended setting.
// Embedders that are happy with the defaults do not need to do anything. To
// change some of them, start from the defaults and set only the fields you
// want to control:
//
//   RuntimeOptions options;
//   options.use_rust_json = false;
//   SetRuntimeOptions(options);
//
// When a new field is added, embedders get its default until they choose to
// set it.
//
// Each field's comment says what it controls, when Open Screen reads it, any
// build arguments that it depends on, and whether it is temporary. For how the
// mechanism works as a whole, see docs/runtime_options.md.
struct RuntimeOptions {
  // Use the Rust serde_json_lenient backend for json::Parse() and
  // json::Stringify(). When false, use jsoncpp.
  //
  // Read on every call to json::Parse() or json::Stringify().
  //
  // Has no effect if Open Screen was built without the Rust backend
  // (`enable_rust = false` or `use_json_serde = false`). jsoncpp is used
  // instead.
  //
  // Temporary: remove once the jsoncpp backend is deleted. See
  // https://crbug.com/578353812.
  bool use_rust_json = true;

  // Use the Rust wire parser for Cast Streaming RTP and RTCP packets. When
  // false, use the C++ parser.
  //
  // Read once, when a Cast Streaming sender or receiver creates its packet
  // parsers and packet router. They keep that parser for their whole life, so
  // a change affects only senders and receivers created after it.
  //
  // Has no effect if Open Screen was built without the Rust parser
  // (`enable_rust = false` or `use_rust_rtp_parser = false`). The C++ parser is
  // used instead.
  //
  // Temporary: remove once the C++ parser is deleted. See
  // https://crbug.com/578353812.
  bool use_rust_rtp_parser = true;
};

// Returns the options in effect: the defaults, or the last value passed to
// SetRuntimeOptions(). Can be called on any thread.
RuntimeOptions GetRuntimeOptions();

// Replaces all of the options in effect. Can be called on any thread, and more
// than once.
//
// Some options are read every time they are used, and others are read once
// when an object is created. See the comment on each field. To make sure that
// every option applies to everything, call this once at startup, before
// creating any Open Screen objects.
void SetRuntimeOptions(const RuntimeOptions& options);

}  // namespace openscreen

#endif  // PLATFORM_API_RUNTIME_OPTIONS_H_
