// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SEND_TEMPLATE_H
#define MADO_SEND_TEMPLATE_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "check.h"
#include "flash_address.h"
#include "kMaxStringScan.h"

namespace mado {

constexpr uint8_t kMaxTagLength = 16U;
constexpr uint16_t kMaxTemplateLength = 0x7FFFU;

namespace detail {

inline bool is_tag_char(const char symbol) {
  const bool alpha = ((symbol >= 'a') && (symbol <= 'z')) || ((symbol >= 'A') && (symbol <= 'Z'));
  const bool digit = (symbol >= '0') && (symbol <= '9');
  return alpha || digit || (symbol == '_');
}

struct FlashPage {
  const char* text;
  size_t length;
};

inline uint8_t read_tag(const FlashPage& page, const size_t start, char* const tag) {
  const bool valid = check(page.text != nullptr) && check(tag != nullptr);
  if (!valid) {
    return 0U;
  }
  uint8_t tag_length = 0U;
  for (uint8_t i = 0U; i <= kMaxTagLength; ++i) {
    const size_t position = start + i;
    if (position >= page.length) {
      return 0U;
    }
    const char symbol = static_cast<char>(pgm_read_byte(page.text + position));
    if (symbol == '$') {
      tag[tag_length] = '\0';
      return tag_length;
    }
    if ((!is_tag_char(symbol)) || (tag_length >= kMaxTagLength)) {
      return 0U;
    }
    tag[tag_length] = symbol;
    ++tag_length;
  }
  return 0U;
}

template <typename ExchangeT, typename TagsT>
size_t emit_tag(ExchangeT& exchange, const char* const tag) {
  const bool named = check(tag != nullptr) && check(tag[0] != '\0');
  if (!named) {
    return 0U;
  }
  const bool rendered = TagsT::render(exchange, tag);
  if (rendered) {
    return 0U;
  }
  size_t written = exchange.write(static_cast<uint8_t>('$'));
  written += exchange.print(tag);
  written += exchange.write(static_cast<uint8_t>('$'));
  return written;
}

}

template <typename ExchangeT, typename TagsT>
bool send_template(ExchangeT& exchange, const __FlashStringHelper* const page) {
  const bool valid = check(page != nullptr);
  if (!valid) {
    return false;
  }
  const char* const text = flash_address(page);
  const size_t length = strlen_P(text);
  const bool fits = check(length <= kMaxTemplateLength);
  if (!fits) {
    return false;
  }
  char tag[kMaxTagLength + 1U] = {};
  const detail::FlashPage page_text = {text, length};
  size_t index = 0U;
  for (uint16_t step = 0U; (step <= kMaxTemplateLength) && (index < length); ++step) {
    const bool in_page = check(index < length);
    if (!in_page) {
      break;
    }
    const char symbol = static_cast<char>(pgm_read_byte(text + index));
    uint8_t tag_length = 0U;
    if (symbol == '$') {
      tag_length = detail::read_tag(page_text, index + 1U, &tag[0]);
    }
    if (tag_length > 0U) {
      static_cast<void>(detail::emit_tag<ExchangeT, TagsT>(exchange, &tag[0]));
      index += static_cast<size_t>(tag_length) + 2U;
      continue;
    }
    const size_t written = exchange.write(static_cast<uint8_t>(symbol));
    if (written == 0U) {
      return false;
    }
    ++index;
  }
  return index >= length;
}

}

#endif
