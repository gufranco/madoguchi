#ifndef MADO_TEST_TEST_ACCESS_H
#define MADO_TEST_TEST_ACCESS_H

#include <mado/UptimeClock.h>
#include <stdint.h>

#include <string>

#include "TestSecrets.h"

constexpr uint32_t kTestSession = 7U;

inline std::string bearer() {
  return std::string("Authorization: Bearer ") + &test_secrets::kToken[0] + "\r\n";
}

template <typename AuthorityT, typename FailuresT, typename AccessT>
struct TestAccess {
  AuthorityT authority;
  FailuresT failures{nullptr};
  AccessT access{authority, failures};
  mado::UptimeClock clock;
};

template <typename GatekeeperT>
GatekeeperT& fresh_gatekeeper() {
  static GatekeeperT instance;
  instance.clock = mado::UptimeClock{};
  instance.access.begin(kTestSession);
  return instance;
}

#endif
