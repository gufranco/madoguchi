#ifndef MADO_TEST_TEST_ACCESS_H
#define MADO_TEST_TEST_ACCESS_H

#include <mado/UptimeClock.h>
#include <stdint.h>

#include <string>

#include "TestSecrets.h"

constexpr uint32_t kTestSession = 7U;

inline bool& anonymous_requests() {
  static bool anonymous = false;
  return anonymous;
}

inline std::string bearer() {
  return anonymous_requests()
             ? std::string()
             : std::string("Authorization: Bearer ") + test_secrets::kToken + "\r\n";
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
  anonymous_requests() = false;
  return instance;
}

#endif
