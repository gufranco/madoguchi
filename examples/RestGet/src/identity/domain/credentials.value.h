// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_GET_IDENTITY_CREDENTIALS_VALUE_H
#define REST_GET_IDENTITY_CREDENTIALS_VALUE_H

#include <mado/check.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace identity {

constexpr uint8_t kUserNameSize = 17U;
constexpr uint8_t kPasswordSize = 65U;

struct Credentials {
  char username[kUserNameSize];
  char password[kPasswordSize];
};

enum class CredentialsStatus : uint8_t { kComplete, kEmpty, kUnterminated };

inline CredentialsStatus credentials_status(const Credentials& credentials) {
  const size_t user = strnlen(&credentials.username[0], kUserNameSize);
  const size_t secret = strnlen(&credentials.password[0], kPasswordSize);
  if ((!mado::check(user < kUserNameSize)) || (!mado::check(secret < kPasswordSize))) {
    return CredentialsStatus::kUnterminated;
  }
  const bool filled = mado::check(user > 0U) && mado::check(secret > 0U);
  return filled ? CredentialsStatus::kComplete : CredentialsStatus::kEmpty;
}

}

#endif
