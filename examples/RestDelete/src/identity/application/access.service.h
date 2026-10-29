// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_DELETE_IDENTITY_ACCESS_SERVICE_H
#define REST_DELETE_IDENTITY_ACCESS_SERVICE_H

#include <mado/GuardEntry.h>
#include <mado/LoginAttempt.h>
#include <mado/LoginGuard.h>
#include <mado/LoginGuardTable.h>
#include <mado/TokenClaims.h>
#include <mado/TokenStatus.h>
#include <mado/check.h>
#include <stdint.h>
#include <string.h>

#include "../domain/credentials.value.h"
#include "../domain/session.value.h"
#include "token_authority.port.h"

namespace identity {

enum class Admission : uint8_t { kOpen, kUnprovisioned, kLocked };

enum class SignInStatus : uint8_t { kSignedIn, kRefused, kUnsigned };

template <typename AuthorityT, typename FailuresT>
class AccessService {
 public:
  static_assert(TokenAuthorityPort<AuthorityT>::kSatisfied, "the authority fits its port");
  static_assert(FailureLogPort<FailuresT>::kSatisfied, "the failure log fits its port");

  AccessService(const AuthorityT& authority, FailuresT& failures)
      : authority_(&authority), failures_(&failures) {
  }

  AccessService(const AccessService&) = delete;
  AccessService& operator=(const AccessService&) = delete;
  AccessService(AccessService&&) = delete;
  AccessService& operator=(AccessService&&) = delete;
  ~AccessService() = default;

  void begin(const uint32_t session) {
    const bool signable = mado::check(session > 0U) && mado::check(session <= kLastClaimValue);
    session_ = signable ? session : 0U;
    mado::GuardEntry stored[kGuardSlots] = {};
    const bool loaded = failures_->load(stored);
    guards_ = mado::LoginGuardTable<kGuardSlots>{};
    guards_.restore(stored);
    static_cast<void>(loaded);
  }

  Admission admit(const uint32_t address, const uint32_t now) const {
    if (!mado::check(authority_->provisioned())) {
      return Admission::kUnprovisioned;
    }
    return mado::check(!guards_.locked(mado::LoginAttempt{address, now})) ? Admission::kOpen
                                                                          : Admission::kLocked;
  }

  uint32_t retry_after(const uint32_t address, const uint32_t now) const {
    const uint32_t wait = guards_.retry_after(mado::LoginAttempt{address, now});
    const bool waiting =
        mado::check(wait > 0U) && mado::check(wait <= mado::LoginGuard::kLockSeconds);
    return waiting ? wait : 1U;
  }

  SignInStatus sign_in(const Credentials& credentials, const uint32_t address, const uint32_t now,
                       char (&token)[kTokenTextSize]) {
    token[0] = '\0';
    const bool complete =
        mado::check(credentials_status(credentials) == CredentialsStatus::kComplete);
    if (!(complete && mado::check(authority_->matches(credentials)))) {
      guards_.record_failure(mado::LoginAttempt{address, now});
      remember();
      return SignInStatus::kRefused;
    }
    guards_.record_success(address);
    remember();
    return issue(credentials, now, token) ? SignInStatus::kSignedIn : SignInStatus::kUnsigned;
  }

  mado::TokenStatus authorize(const char* const header, const uint32_t now,
                              mado::TokenClaims& claims) const {
    claims = mado::TokenClaims{};
    if (!mado::check(now <= kLastClaimValue)) {
      return mado::TokenStatus::kExpired;
    }
    const mado::TokenStatus status = authority_->verify(header, session_, now, claims);
    const bool bounded = mado::check(claims.ends_at <= kLastClaimValue);
    return bounded ? status : mado::TokenStatus::kMalformed;
  }

 private:
  void remember() {
    mado::GuardEntry entries[kGuardSlots] = {};
    guards_.snapshot(entries);
    const bool saved = failures_->save(entries);
    static_cast<void>(saved);
  }

  bool issue(const Credentials& credentials, const uint32_t now,
             char (&token)[kTokenTextSize]) const {
    mado::TokenClaims claims = {};
    const bool named = mado::check(strlen(&credentials.username[0]) < sizeof(claims.subject));
    static_cast<void>(
        strncpy(&claims.subject[0], &credentials.username[0], sizeof(claims.subject) - 1U));
    claims.session = session_;
    const bool timed = token_ends_at(now, claims.ends_at);
    return named && timed && mado::check(session_ > 0U) && authority_->sign(claims, token);
  }

  const AuthorityT* authority_;
  FailuresT* failures_;
  mado::LoginGuardTable<kGuardSlots> guards_ = {};
  uint32_t session_ = 0U;
};

}

#endif
