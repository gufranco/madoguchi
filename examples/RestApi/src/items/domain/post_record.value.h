// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_ITEMS_POST_RECORD_VALUE_H
#define REST_API_ITEMS_POST_RECORD_VALUE_H

#include <mado/check.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "item.entity.h"

namespace items {

constexpr uint8_t kKeySize = 65U;
constexpr uint32_t kFingerprintOffset = 2166136261UL;
constexpr uint32_t kFingerprintPrime = 16777619UL;

struct PostRecord {
  char key[kKeySize];
  uint32_t fingerprint;
  uint8_t item_id;
};

inline uint32_t draft_fingerprint(const ItemDraft& draft) {
  const size_t length = strnlen(&draft.name[0], kNameSize);
  const bool terminated = mado::check(length < kNameSize);
  uint32_t hash = kFingerprintOffset;
  for (size_t i = 0U; terminated && (i < length); ++i) {
    hash = (hash ^ static_cast<uint8_t>(draft.name[i])) * kFingerprintPrime;
  }
  const uint32_t value = static_cast<uint32_t>(draft.value);
  for (uint8_t shift = 0U; shift < 32U; shift = static_cast<uint8_t>(shift + 8U)) {
    hash = (hash ^ static_cast<uint8_t>(value >> shift)) * kFingerprintPrime;
  }
  hash = (hash ^ (draft.on ? 1U : 0U)) * kFingerprintPrime;
  const bool hashed = mado::check(hash != 0U);
  return (terminated && hashed) ? hash : 0U;
}

inline bool same_key(const char (&left)[kKeySize], const char* const right) {
  const bool given = mado::check(right != nullptr);
  const bool stored = mado::check(left[kKeySize - 1U] == '\0');
  return given && stored && (left[0] != '\0') && (strncmp(&left[0], right, kKeySize) == 0);
}

}

#endif
