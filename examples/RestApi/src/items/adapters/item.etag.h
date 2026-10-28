// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_ITEMS_ITEM_ETAG_H
#define REST_API_ITEMS_ITEM_ETAG_H

#include <mado/check.h>
#include <stdint.h>

#include "../domain/item.entity.h"

namespace items {

constexpr uint8_t kRevisionDigits = 8U;
constexpr uint8_t kItemTagSize = kRevisionDigits + 5U;
static_assert(kCapacity < 10U, "an item id is one decimal digit in the tag");

inline bool item_tag(const Item& item, char (&out)[kItemTagSize]) {
  const bool numbered = mado::check(is_item_id(item.id));
  const bool revised = mado::check(item.revision != 0U);
  out[0] = '"';
  out[1] = static_cast<char>('0' + (numbered ? item.id : 0U));
  out[2] = '-';
  for (uint8_t i = 0U; i < kRevisionDigits; ++i) {
    const uint8_t shift = static_cast<uint8_t>(28U - (4U * i));
    const uint8_t nibble = static_cast<uint8_t>((item.revision >> shift) & 0x0FU);
    out[3U + i] = static_cast<char>((nibble < 10U) ? ('0' + nibble) : ('a' + (nibble - 10U)));
  }
  out[3U + kRevisionDigits] = '"';
  out[4U + kRevisionDigits] = '\0';
  return numbered && revised;
}

}

#endif
