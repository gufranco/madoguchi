// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SEND_FILE_H
#define MADO_SEND_FILE_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ByteRange.h"
#include "DecimalText.h"
#include "FileReply.h"
#include "FlashText.h"
#include "Method.h"
#include "check.h"
#include "etag_matches.h"
#include "http_status.h"
#include "kMaxStringScan.h"
#include "parse_byte_range.h"
#include "valid_card_name.h"

namespace mado {

constexpr uint32_t kMaxFileChunks = 0x08000000UL;

namespace detail {

template <typename ExchangeT, typename FileT>
bool stream_file(ExchangeT& exchange, FileT& file, const uint32_t size) {
  constexpr uint16_t kChunk = ExchangeT::Limits::kFileChunkSize;
  static_assert(kChunk > 0U, "a file chunk needs room");
  uint8_t chunk[kChunk] = {};
  uint32_t remaining = size;
  for (uint32_t step = 0U; step < kMaxFileChunks; ++step) {
    if (remaining == 0UL) {
      break;
    }
    const uint16_t wanted = static_cast<uint16_t>((remaining < kChunk) ? remaining : kChunk);
    const int32_t got = file.read(&chunk[0], wanted);
    const bool read_ok = check(got > 0) && check(got <= static_cast<int32_t>(wanted));
    if (!read_ok) {
      return false;
    }
    const size_t written = exchange.write(&chunk[0], static_cast<size_t>(got));
    if (written != static_cast<size_t>(got)) {
      return false;
    }
    const bool consistent = check(static_cast<uint32_t>(got) <= remaining);
    if (!consistent) {
      return false;
    }
    remaining -= static_cast<uint32_t>(got);
  }
  return remaining == 0UL;
}

}

namespace detail {

constexpr uint8_t kHeaderTextSize = 96U;

inline uint8_t append_text(char* const out, const uint8_t offset, const char* const flash_text) {
  const bool valid = check(out != nullptr) && check(flash_text != nullptr);
  if (!valid) {
    return offset;
  }
  const size_t length = strlen_P(flash_text);
  const bool fits = check((offset + length) < kHeaderTextSize);
  uint8_t used = offset;
  for (size_t i = 0U; fits && (i < length); ++i) {
    out[used] = static_cast<char>(pgm_read_byte(flash_text + i));
    ++used;
  }
  out[used] = '\0';
  return used;
}

inline uint8_t append_number(char* const out, const uint8_t offset, const DecimalText& digits) {
  const bool fits = check(out != nullptr) && check((offset + digits.length()) < kHeaderTextSize);
  uint8_t used = offset;
  for (uint8_t i = 0U; fits && (i < digits.length()); ++i) {
    out[used] = digits.begin()[i];
    ++used;
  }
  out[used] = '\0';
  return used;
}

inline bool content_range(const ByteRange& range, const uint32_t size, const bool satisfiable,
                          char (&out)[kHeaderTextSize]) {
  const bool ordered = check(range.first <= range.last) || check(!satisfiable);
  uint8_t used = append_text(&out[0], 0U, PSTR("bytes "));
  if (satisfiable) {
    used = append_number(&out[0], used, DecimalText(range.first));
    used = append_text(&out[0], used, PSTR("-"));
    used = append_number(&out[0], used, DecimalText(range.last));
  } else {
    used = append_text(&out[0], used, PSTR("*"));
  }
  used = append_text(&out[0], used, PSTR("/"));
  used = append_number(&out[0], used, DecimalText(size));
  return ordered && check(used < kHeaderTextSize);
}

template <typename ExchangeT>
bool send_validators(ExchangeT& exchange, const char* const etag) {
  const bool valid = check(etag != nullptr) && check(exchange.status_sent());
  const bool tagged = valid && exchange.send_header(F("ETag"), etag);
  return tagged && exchange.send_header(F("Cache-Control"), FlashText(F("no-cache")));
}

template <typename ExchangeT>
bool send_file_head(ExchangeT& exchange, const uint16_t code, const FileReply& reply,
                    const uint32_t length) {
  const bool valid = check(reply.type != nullptr) && check(!exchange.status_sent());
  const bool status = valid && exchange.send_status(code);
  const bool typed = status && exchange.send_content_type(reply.type);
  const bool sized = typed && exchange.send_content_length(length);
  const bool ranges = sized && exchange.send_header(F("Accept-Ranges"), FlashText(F("bytes")));
  const bool encoded = ranges && ((!reply.gzip) || exchange.send_header(F("Content-Encoding"),
                                                                        FlashText(F("gzip"))));
  const bool tagged = encoded && ((reply.etag == nullptr) || send_validators(exchange, reply.etag));
  const bool cached =
      tagged && ((reply.cache_control == nullptr) ||
                 exchange.send_header(F("Cache-Control"), FlashText(reply.cache_control)));
  return cached;
}

inline uint8_t append_name(char* const out, const uint8_t offset, const char* const name) {
  const bool valid = check(out != nullptr) && valid_card_name(name);
  uint8_t used = offset;
  for (uint8_t i = 0U; valid && (i < kCardNameSize) && (used < (kHeaderTextSize - 1U)); ++i) {
    if (name[i] == '\0') {
      break;
    }
    out[used] = name[i];
    ++used;
  }
  out[used] = '\0';
  return used;
}

template <typename ExchangeT>
bool send_disposition(ExchangeT& exchange, const char* const download_name) {
  const bool ready = check(exchange.status_sent());
  if ((!ready) || (download_name == nullptr)) {
    return ready;
  }
  char value[kHeaderTextSize] = {};
  uint8_t used = append_text(&value[0], 0U, PSTR("attachment"));
  if (valid_card_name(download_name)) {
    used = append_text(&value[0], used, PSTR("; filename=\""));
    used = append_name(&value[0], used, download_name);
    used = append_text(&value[0], used, PSTR("\""));
  }
  const bool fits = check(used < kHeaderTextSize);
  return fits && exchange.send_header(F("Content-Disposition"), &value[0]);
}

inline bool range_allowed(const char* const if_range, const char* const etag) {
  if (if_range == nullptr) {
    return true;
  }
  const bool tagged = check((etag == nullptr) || (etag[0] != '\0'));
  const bool strong = tagged && check(if_range[0] != '\0') && (etag != nullptr) && (etag[0] == '"');
  return strong && (strncmp(if_range, etag, kMaxStringScan) == 0);
}

template <typename ExchangeT>
RangeStatus requested_range(ExchangeT& exchange, const FileReply& reply, ByteRange& range) {
  const bool unsent = check(!exchange.status_sent()) && check(reply.type != nullptr);
  const bool ranges = unsent && range_allowed(exchange.header(F("If-Range")), reply.etag);
  const char* const asked = ranges ? exchange.header(F("Range")) : nullptr;
  return parse_byte_range(asked, reply.size, range);
}

template <typename ExchangeT>
bool send_not_modified(ExchangeT& exchange, const char* const etag) {
  const bool valid = check(etag != nullptr) && check(!exchange.status_sent());
  const bool status = valid && exchange.send_status(http::kNotModified);
  const bool tagged = status && send_validators(exchange, etag);
  const bool length = tagged && exchange.send_content_length(0U);
  return length && exchange.end_headers();
}

template <typename ExchangeT>
bool send_unsatisfiable(ExchangeT& exchange, const uint32_t size) {
  char range_text[kHeaderTextSize] = {};
  const ByteRange none = {0U, 0U};
  const bool formatted = content_range(none, size, false, range_text);
  const bool status = exchange.send_status(http::kRangeNotSatisfiable);
  const bool ranged =
      status && formatted && exchange.send_header(F("Content-Range"), &range_text[0]);
  const bool length = ranged && exchange.send_content_length(0U);
  return length && exchange.end_headers();
}

}

template <typename ExchangeT, typename FileT>
bool send_file(ExchangeT& exchange, FileT& file, const FileReply& reply) {
  const bool valid = check(reply.type != nullptr) && check(!exchange.status_sent());
  if (!valid) {
    return false;
  }
  const bool fresh =
      (reply.etag != nullptr) && etag_matches(exchange.header(F("If-None-Match")), reply.etag);
  if (fresh) {
    return detail::send_not_modified(exchange, reply.etag);
  }
  ByteRange range = {0U, 0U};
  const RangeStatus status = detail::requested_range(exchange, reply, range);
  if (status == RangeStatus::kUnsatisfiable) {
    return detail::send_unsatisfiable(exchange, reply.size);
  }
  const bool partial = status == RangeStatus::kPartial;
  const uint32_t length = partial ? ((range.last - range.first) + 1U) : reply.size;
  const bool head =
      detail::send_file_head(exchange, partial ? http::kPartialContent : http::kOk, reply, length);
  char range_text[detail::kHeaderTextSize] = {};
  const bool ranged = (!partial) || (detail::content_range(range, reply.size, true, range_text) &&
                                     exchange.send_header(F("Content-Range"), &range_text[0]));
  const bool named = head && ranged && detail::send_disposition(exchange, reply.download_name);
  const bool ended = named && exchange.end_headers();
  if ((!ended) || (exchange.method() == Method::kHead)) {
    return ended;
  }
  const bool positioned = (!partial) || file.seek(range.first);
  return positioned && detail::stream_file(exchange, file, length);
}

template <typename ExchangeT, typename FileT>
bool send_file(ExchangeT& exchange, FileT& file, const uint32_t size,
               const __FlashStringHelper* const type, const bool gzip,
               const __FlashStringHelper* const cache_control = nullptr) {
  const bool valid = check(type != nullptr) && check(!exchange.status_sent());
  if (!valid) {
    return false;
  }
  const bool status = exchange.send_status(http::kOk);
  const bool typed = exchange.send_content_type(type);
  const bool sized = exchange.send_content_length(size);
  bool encoded = true;
  if (gzip) {
    encoded = exchange.send_header(F("Content-Encoding"), FlashText(F("gzip")));
  }
  const bool cached = (cache_control == nullptr) ||
                      exchange.send_header(F("Cache-Control"), FlashText(cache_control));
  const bool ended = exchange.end_headers();
  const bool headers_ok = status && typed && sized && encoded && cached && ended;
  if ((!headers_ok) || (exchange.method() == Method::kHead)) {
    return headers_ok;
  }
  return detail::stream_file(exchange, file, size);
}

}

#endif
