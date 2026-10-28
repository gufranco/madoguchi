// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SEND_TEMPLATE_H
#define MADO_SEND_TEMPLATE_H

#include <stddef.h>
#include <stdint.h>

#include "check.h"

namespace mado {

constexpr uint8_t kMaxTagLength = 16U;
constexpr uint8_t kMaxDollarRun = 16U;
constexpr uint32_t kMaxTemplateBytes = 1048576UL;

namespace detail {

inline bool is_tag_char(const int16_t symbol) {
  const bool known = check((symbol >= -1) && (symbol <= 0xFF));
  const bool alpha = ((symbol >= 'a') && (symbol <= 'z')) || ((symbol >= 'A') && (symbol <= 'Z'));
  const bool digit = (symbol >= '0') && (symbol <= '9');
  return known && (alpha || digit || (symbol == '_'));
}

struct TagScan {
  uint8_t length;
  int16_t stop;
};

template <typename SourceT>
TagScan scan_tag(SourceT& source, char (&tag)[kMaxTagLength + 1U]) {
  TagScan scan = {0U, -1};
  for (uint8_t i = 0U; i <= kMaxTagLength; ++i) {
    const int16_t symbol = source.next();
    const bool in_tag = is_tag_char(symbol) && (scan.length < kMaxTagLength);
    if (!in_tag) {
      scan.stop = symbol;
      break;
    }
    tag[scan.length] = static_cast<char>(symbol);
    scan.length = static_cast<uint8_t>(scan.length + 1U);
  }
  tag[scan.length] = '\0';
  const bool bounded = check(scan.length <= kMaxTagLength);
  return bounded ? scan : TagScan{0U, -1};
}

template <typename ExchangeT>
bool write_unmatched(ExchangeT& exchange, const char* const tag, const int16_t stop) {
  const bool valid = check(tag != nullptr) && check(stop <= 0xFF);
  if (!valid) {
    return false;
  }
  const bool opened = exchange.write(static_cast<uint8_t>('$')) == 1U;
  const bool body = opened && ((tag[0] == '\0') || (exchange.print(tag) > 0U));
  const bool tail = (stop < 0) || (exchange.write(static_cast<uint8_t>(stop)) == 1U);
  return body && tail;
}

template <typename ExchangeT, typename TagsT, typename SourceT>
bool copy_tag(ExchangeT& exchange, SourceT& source) {
  char tag[kMaxTagLength + 1U] = {};
  for (uint8_t run = 0U; run < kMaxDollarRun; ++run) {
    const TagScan scan = scan_tag(source, tag);
    const bool sane =
        check(tag[scan.length] == '\0') && check((scan.stop >= -1) && (scan.stop <= 0xFF));
    if (!sane) {
      return false;
    }
    const bool doubled = (scan.stop == '$') && (scan.length == 0U);
    if (doubled) {
      const bool kept = exchange.write(static_cast<uint8_t>('$')) == 1U;
      if (!kept) {
        return false;
      }
      continue;
    }
    const bool closed = scan.stop == '$';
    if (closed && TagsT::render(exchange, &tag[0])) {
      return true;
    }
    return write_unmatched(exchange, &tag[0], scan.stop);
  }
  return exchange.write(static_cast<uint8_t>('$')) == 1U;
}

}

template <typename ExchangeT, typename TagsT, typename SourceT>
bool send_template(ExchangeT& exchange, SourceT& source) {
  bool written = true;
  for (uint32_t step = 0U; written && (step < kMaxTemplateBytes); ++step) {
    const int16_t symbol = source.next();
    if (symbol < 0) {
      return check(symbol == -1);
    }
    const bool in_range = check(symbol <= 0xFF);
    written = in_range && ((symbol == '$') ? detail::copy_tag<ExchangeT, TagsT>(exchange, source)
                                           : (exchange.write(static_cast<uint8_t>(symbol)) == 1U));
  }
  return false;
}

}

#endif
