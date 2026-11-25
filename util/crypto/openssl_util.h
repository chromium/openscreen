// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_CRYPTO_OPENSSL_UTIL_H_
#define UTIL_CRYPTO_OPENSSL_UTIL_H_

#include <openssl/ssl.h>
#include <stddef.h>

#include <cstring>

#include "platform/base/error.h"
#include "platform/base/location.h"

namespace openscreen {
// Initialize OpenSSL if it isn't already initialized. This must be called
// before any other OpenSSL functions though it is safe and cheap to call this
// multiple times.
// This function is thread-safe, and OpenSSL will only ever be initialized once.
// OpenSSL will be properly shut down on program exit.
// Multiple sequential calls to EnsureOpenSSLInit are ignored by OpenSSL itself.
void EnsureOpenSSLInit();

// Drains the OpenSSL ERR_get_error stack. On a debug build the error codes
// are send to VLOG(1), on a release build they are disregarded. In most
// cases you should pass CURRENT_LOCATION as the `location`.
void ClearOpenSSLERRStack(const Location& location);

// May be called directly if you already have invoked SSL_get_error and just
// want to convert the error code to an Error.
Error SSLErrorCodeToError(int error_code);

// Gets the most recent SSL error from the OpenSSL error stack, and converts it
// to an Error. The `ssl` and `return_code` parameters are passed to
// SSL_get_error to determine the SSL error code, which is then converted to an
// Error using SSLErrorCodeToError().
Error GetSSLError(const SSL* ssl, int return_code);

}  // namespace openscreen

#endif  // UTIL_CRYPTO_OPENSSL_UTIL_H_
