// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_WRITE_LOG_LINE_H
#define MADO_WRITE_LOG_LINE_H

#include <Arduino.h>
#include <stdint.h>

#include "LogEntry.h"
#include "check.h"
#include "http_status.h"
#include "json_write_string.h"
#include "method_name.h"

namespace mado {

namespace detail {

template <typename SinkT>
bool write_log_stamp(SinkT& sink, const uint8_t level, const LogStamp& stamp) {
  const bool valid = check(stamp.hostname != nullptr) && check(level >= kLogInfo) &&
                     check(stamp.boot < 0x80000000UL);
  bool written = valid && (sink.print(F("{\"level\":")) > 0U);
  written = written && (sink.print(static_cast<uint32_t>(level)) > 0U);
  written = written && (sink.print(F(",\"boot\":")) > 0U);
  written = written && (sink.print(stamp.boot) > 0U);
  written = written && (sink.print(F(",\"uptime\":")) > 0U);
  written = written && (sink.print(stamp.uptime_ms) > 0U);
  written = written && (sink.print(F(",\"hostname\":\"")) > 0U);
  written = written && (sink.print(stamp.hostname) > 0U);
  return written && (sink.print(F("\"")) > 0U);
}

}

template <typename SinkT>
bool write_request_log(SinkT& sink, const LogStamp& stamp, const RequestLog& request) {
  const bool valid = check(request.url != nullptr) && check(request.status >= http::kFirstValid);
  const bool failed = request.status >= http::kInternalServerError;
  bool written = valid && detail::write_log_stamp(sink, failed ? kLogError : kLogInfo, stamp);
  written = written && (sink.print(F(",\"req\":{\"id\":")) > 0U);
  written = written && (sink.print(request.id) > 0U);
  written = written && (sink.print(F(",\"method\":\"")) > 0U);
  written = written && (sink.print(method_name(request.method)) > 0U);
  written = written && (sink.print(F("\",\"url\":")) > 0U);
  written = written && json_write_string(sink, request.url);
  written = written && (sink.print(F("},\"res\":{\"statusCode\":")) > 0U);
  written = written && (sink.print(static_cast<uint32_t>(request.status)) > 0U);
  written = written && (sink.print(F("},\"responseTime\":")) > 0U);
  written = written && (sink.print(request.response_ms) > 0U);
  written = written && (sink.print(failed ? F(",\"msg\":\"request errored\"}\n")
                                          : F(",\"msg\":\"request completed\"}\n")) > 0U);
  return written;
}

template <typename SinkT>
bool write_event_log(SinkT& sink, const LogStamp& stamp, const uint8_t level,
                     const __FlashStringHelper* const message) {
  const bool valid = check(message != nullptr) && check(level <= kLogError);
  bool written = valid && detail::write_log_stamp(sink, level, stamp);
  written = written && (sink.print(F(",\"msg\":\"")) > 0U);
  written = written && (sink.print(message) > 0U);
  return written && (sink.print(F("\"}\n")) > 0U);
}

}

#endif
