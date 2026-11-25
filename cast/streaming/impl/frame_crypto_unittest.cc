// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/impl/frame_crypto.h"

#include <array>
#include <cstring>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/byte_span.h"
#include "util/crypto/random_bytes.h"

namespace openscreen::cast {
namespace {

using testing::ElementsAreArray;
using testing::Not;

TEST(FrameCryptoTest, EncryptsAndDecryptsFrames) {
  // Prepare two frames with different FrameIds, but having the same payload
  // bytes.
  EncodedFrame frame0;
  frame0.frame_id = FrameId::first();
  const char kPayload[] = "The quick brown fox jumps over the lazy dog.";
  std::vector<uint8_t> buffer(
      reinterpret_cast<const uint8_t*>(kPayload),
      reinterpret_cast<const uint8_t*>(kPayload) + sizeof(kPayload));
  frame0.data = buffer;
  EncodedFrame frame1;
  frame1.frame_id = frame0.frame_id + 1;
  frame1.data = frame0.data;

  const std::array<uint8_t, 16> key = GenerateRandomBytes16();
  const std::array<uint8_t, 16> iv = GenerateRandomBytes16();
  EXPECT_NE(0, memcmp(key.data(), iv.data(), sizeof(key)));
  const FrameCrypto crypto(key, iv);

  // Encrypt both frames, and confirm the encrypted data is something other than
  // the plaintext, and that both frames have different encrypted data.
  const EncryptedFrame encrypted_frame0 = crypto.Encrypt(frame0);
  EXPECT_EQ(frame0.frame_id, encrypted_frame0.frame_id);
  ASSERT_EQ(frame0.data.size(), encrypted_frame0.data.size());
  EXPECT_THAT(frame0.data, Not(ElementsAreArray(encrypted_frame0.data)));

  const EncryptedFrame encrypted_frame1 = crypto.Encrypt(frame1);
  EXPECT_EQ(frame1.frame_id, encrypted_frame1.frame_id);
  ASSERT_EQ(frame1.data.size(), encrypted_frame1.data.size());
  EXPECT_THAT(frame1.data, Not(ElementsAreArray(encrypted_frame1.data)));
  EXPECT_THAT(encrypted_frame0.data,
              Not(ElementsAreArray(encrypted_frame1.data)));

  // Now, decrypt the encrypted frames, and confirm the original payload
  // plaintext is retrieved.
  std::vector<uint8_t> decrypted_frame0_buffer(encrypted_frame0.data.size());
  crypto.Decrypt(encrypted_frame0.frame_id, {&encrypted_frame0.data, 1},
                 decrypted_frame0_buffer);
  EncodedFrame decrypted_frame0;
  encrypted_frame0.CopyMetadataTo(&decrypted_frame0);
  decrypted_frame0.data = decrypted_frame0_buffer;
  EXPECT_EQ(frame0.frame_id, decrypted_frame0.frame_id);
  EXPECT_THAT(frame0.data, ElementsAreArray(decrypted_frame0.data));

  std::vector<uint8_t> decrypted_frame1_buffer(encrypted_frame1.data.size());
  crypto.Decrypt(encrypted_frame1.frame_id, {&encrypted_frame1.data, 1},
                 decrypted_frame1_buffer);
  EncodedFrame decrypted_frame1;
  encrypted_frame1.CopyMetadataTo(&decrypted_frame1);
  decrypted_frame1.data = decrypted_frame1_buffer;
  EXPECT_EQ(frame1.frame_id, decrypted_frame1.frame_id);
  EXPECT_THAT(frame1.data, ElementsAreArray(decrypted_frame1.data));
}

TEST(FrameCryptoTest, EncryptsWithBufferReuseAndDecryptsEncryptedFrame) {
  const char kPayload[] = "Buffer reuse encryption test payload.";
  std::vector<uint8_t> buffer(
      reinterpret_cast<const uint8_t*>(kPayload),
      reinterpret_cast<const uint8_t*>(kPayload) + sizeof(kPayload));

  EncodedFrame frame;
  frame.frame_id = FrameId::first();
  frame.data = buffer;

  const FrameCrypto crypto(GenerateRandomBytes16(), GenerateRandomBytes16());

  EncryptedFrame reusable_frame;
  crypto.Encrypt(frame, reusable_frame);
  EXPECT_EQ(frame.frame_id, reusable_frame.frame_id);
  EXPECT_THAT(frame.data, Not(ElementsAreArray(reusable_frame.data)));

  const uint8_t* first_buffer_ptr = reusable_frame.data.data();

  // Encrypt a second frame of the same size and verify buffer capacity reuse.
  frame.frame_id = FrameId::first() + 1;
  crypto.Encrypt(frame, reusable_frame);
  EXPECT_EQ(frame.frame_id, reusable_frame.frame_id);
  EXPECT_EQ(first_buffer_ptr, reusable_frame.data.data());

  // Verify the Decrypt(const EncryptedFrame&, ByteBuffer) overload.
  std::vector<uint8_t> decrypted_buffer(reusable_frame.data.size());
  crypto.Decrypt(reusable_frame, decrypted_buffer);
  EXPECT_THAT(frame.data, ElementsAreArray(decrypted_buffer));
}

}  // namespace
}  // namespace openscreen::cast
