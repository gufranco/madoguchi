// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_DELETE_IDENTITY_TOKEN_AUTHORITY_PORT_H
#define REST_DELETE_IDENTITY_TOKEN_AUTHORITY_PORT_H

#include <mado/GuardEntry.h>
#include <mado/TokenClaims.h>
#include <mado/TokenStatus.h>
#include <mado/reference_to.h>
#include <mado/same_type.h>
#include <stdint.h>

#include "../domain/credentials.value.h"

namespace identity {

constexpr uint8_t kGuardSlots = 4U;

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
  using Load = decltype(mado::reference_to<LogT>().load(
      mado::reference_to<mado::GuardEntry[kGuardSlots]>()));
  using Save = decltype(mado::reference_to<LogT>().save(
      mado::reference_to<const mado::GuardEntry[kGuardSlots]>()));

  static_assert(mado::same_type<Load, bool>::value,
                "load(entries) fills the failure count kept for each address and returns whether "
                "a stored table was read");
  static_assert(mado::same_type<Save, bool>::value,
                "save(entries) stores the failure count of each address");

  static constexpr bool kSatisfied = true;
};

}

#endif
