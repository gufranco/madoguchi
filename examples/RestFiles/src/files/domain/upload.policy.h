// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FILES_FILES_UPLOAD_POLICY_H
#define REST_FILES_FILES_UPLOAD_POLICY_H

#include <mado/check.h>
#include <mado/constant_time_equal.h>
#include <mado/kSha256Size.h>
#include <stdint.h>

namespace files {

constexpr uint32_t kMaxUpload = 8388608UL;

struct DigestClaim {
  bool given;
  uint8_t bytes[mado::kSha256Size];
};

inline bool upload_fits(const uint32_t content_length) {
  return mado::check(content_length <= kMaxUpload);
}

inline bool digest_accepted(const DigestClaim& claim, const uint8_t (&actual)[mado::kSha256Size]) {
  const bool same = mado::constant_time_equal(&claim.bytes[0], &actual[0], sizeof(actual));
  return mado::check(!claim.given) || mado::check(same);
}

}

#endif
