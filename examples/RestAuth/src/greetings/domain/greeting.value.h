// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_AUTH_GREETINGS_GREETING_VALUE_H
#define REST_AUTH_GREETINGS_GREETING_VALUE_H

#include <mado/check.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace greetings {

constexpr uint8_t kMessageSize = 41U;
constexpr char kSalutation[] = "Hello, ";

struct Greeting {
  char message[kMessageSize];
};

inline bool greet(const char* const subject, Greeting& greeting) {
  greeting = Greeting{};
  const bool named = mado::check(subject != nullptr) && mado::check(subject[0] != '\0');
  const size_t used = sizeof(kSalutation) - 1U;
  const size_t length = named ? strnlen(subject, kMessageSize) : kMessageSize;
  if (!mado::check((used + length) < kMessageSize)) {
    return false;
  }
  static_cast<void>(memcpy(&greeting.message[0], &kSalutation[0], used));
  static_cast<void>(memcpy(&greeting.message[used], subject, length));
  return true;
}

}

#endif
