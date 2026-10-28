// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_AUTH_IDENTITY_TOKEN_AUTHORITY_PORT_H
#define REST_AUTH_IDENTITY_TOKEN_AUTHORITY_PORT_H

#include <mado/TokenClaims.h>
#include <mado/TokenStatus.h>
#include <mado/reference_to.h>
#include <mado/same_type.h>
#include <stdint.h>

#include "../domain/credentials.value.h"

namespace identity {

constexpr uint16_t kTokenTextSize = 201U;

template <typename AuthorityT>
struct TokenAuthorityPort {
  using Provisioned = decltype(mado::reference_to<const AuthorityT>().provisioned());
  using Matches = decltype(mado::reference_to<const AuthorityT>().matches(
      mado::reference_to<const Credentials>()));
  using Sign = decltype(mado::reference_to<const AuthorityT>().sign(
      mado::reference_to<const mado::TokenClaims>(),
      mado::reference_to<char (&)[kTokenTextSize]>()));
  using Verify = decltype(mado::reference_to<const AuthorityT>().verify(
      static_cast<const char*>(nullptr), uint32_t{}, uint32_t{},
      mado::reference_to<mado::TokenClaims>()));

  static_assert(mado::same_type<Provisioned, bool>::value,
                "provisioned() tells whether a signing key is installed");
  static_assert(mado::same_type<Matches, bool>::value,
                "matches(const Credentials&) checks the user name and the password");
  static_assert(mado::same_type<Sign, bool>::value,
                "sign(claims, text) writes a signed token and returns whether it fit");
  static_assert(mado::same_type<Verify, mado::TokenStatus>::value,
                "verify(header, session, now, claims) checks a bearer token");

  static constexpr bool kSatisfied = true;
};

template <typename LogT>
struct FailureLogPort {
  using Load = decltype(mado::reference_to<LogT>().load());
  using Save = decltype(mado::reference_to<LogT>().save(uint8_t{}));

  static_assert(mado::same_type<Load, uint8_t>::value,
                "load() returns the stored failure count, or 0xFF when there is none");
  static_assert(mado::same_type<Save, bool>::value, "save(count) stores the failure count");

  static constexpr bool kSatisfied = true;
};

}

#endif
