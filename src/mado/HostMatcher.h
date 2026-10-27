// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_HOST_MATCHER_H
#define MADO_HOST_MATCHER_H

#include <stdint.h>

#include "check.h"
#include "to_lower_ascii.h"

namespace mado {

constexpr uint8_t kMaxTrustedHosts = 8U;
constexpr uint8_t kMaxHostLength = 0xFFU;

template <typename PolicyT, bool kListed = PolicyT::kTrustedHostsOnly>
class HostMatcher {
  static_assert(kMaxTrustedHosts <= 8U, "candidate masks hold one bit per host in 8 bits");
  static_assert(kMaxHostLength <= 0xFFU, "the match index is 8-bit");

 public:
  void begin() {
    const char* const* const hosts = PolicyT::trusted_hosts();
    const bool settled =
        check(static_cast<uint8_t>(ports_ & static_cast<uint8_t>(~candidates_)) == 0U);
    static_cast<void>(settled);
    index_ = 0U;
    ports_ = 0U;
    candidates_ = 0U;
    for (uint8_t i = 0U; i < kMaxTrustedHosts; ++i) {
      if (hosts[i] == nullptr) {
        break;
      }
      candidates_ = static_cast<uint8_t>(candidates_ | mask_at(i));
    }
    const bool armed = check((candidates_ != 0U) || (hosts[0] == nullptr));
    candidates_ = armed ? candidates_ : 0U;
  }

  void accept(const char symbol) {
    const char* const* const hosts = PolicyT::trusted_hosts();
    const bool bounded =
        check(index_ < kMaxHostLength) &&
        check(static_cast<uint8_t>(ports_ & static_cast<uint8_t>(~candidates_)) == 0U);
    if (!bounded) {
      candidates_ = 0U;
      return;
    }
    const char lowered = to_lower_ascii(symbol);
    uint8_t kept = 0U;
    for (uint8_t i = 0U; i < kMaxTrustedHosts; ++i) {
      const uint8_t mask = mask_at(i);
      if ((candidates_ & mask) == 0U) {
        continue;
      }
      const Candidate candidate = {hosts[i], mask};
      if (keeps(candidate, lowered)) {
        kept = static_cast<uint8_t>(kept | mask);
      }
    }
    candidates_ = kept;
    ports_ = static_cast<uint8_t>(ports_ & kept);
    ++index_;
  }

  bool trusted() const {
    const char* const* const hosts = PolicyT::trusted_hosts();
    const bool consistent =
        check(static_cast<uint8_t>(ports_ & static_cast<uint8_t>(~candidates_)) == 0U);
    bool matched = false;
    for (uint8_t i = 0U; consistent && (i < kMaxTrustedHosts); ++i) {
      const uint8_t mask = mask_at(i);
      const bool live = (candidates_ & mask) != 0U;
      const bool complete = live && (((ports_ & mask) != 0U) || (hosts[i][index_] == '\0'));
      matched = matched || complete;
    }
    return matched;
  }

 private:
  static uint8_t mask_at(const uint8_t index) {
    const bool in_range = check(index < kMaxTrustedHosts);
    return in_range ? static_cast<uint8_t>(1U << index) : 0U;
  }

  struct Candidate {
    const char* entry;
    uint8_t mask;
  };

  bool keeps(const Candidate& candidate, const char lowered) {
    const bool valid = check(candidate.entry != nullptr) && check(candidate.mask != 0U);
    if (!valid) {
      return false;
    }
    if ((ports_ & candidate.mask) != 0U) {
      return (lowered >= '0') && (lowered <= '9');
    }
    const char expected = to_lower_ascii(candidate.entry[index_]);
    if ((expected == '\0') && (lowered == ':')) {
      ports_ = static_cast<uint8_t>(ports_ | candidate.mask);
      return true;
    }
    return (expected != '\0') && (expected == lowered);
  }

  uint8_t index_ = 0U;
  uint8_t candidates_ = 0U;
  uint8_t ports_ = 0U;
};

template <typename PolicyT>
class HostMatcher<PolicyT, false> {
 public:
  void begin() {
    const bool bounded = check(length_ <= kMaxHostLength);
    static_cast<void>(bounded);
    length_ = 0U;
    invalid_ = false;
  }

  void accept(const char symbol) {
    const bool bounded = check(length_ <= kMaxHostLength);
    const bool visible = symbol != '\0';
    invalid_ = invalid_ || (!bounded) || (!visible) || (length_ == kMaxHostLength);
    length_ = invalid_ ? length_ : static_cast<uint16_t>(length_ + 1U);
  }

  bool trusted() const {
    const bool bounded = check(length_ <= kMaxHostLength);
    return bounded && (!invalid_);
  }

 private:
  uint16_t length_ = 0U;
  bool invalid_ = false;
};

}

#endif
