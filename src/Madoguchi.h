// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADOGUCHI_H
#define MADOGUCHI_H

#include <Arduino.h>
#include <stdint.h>

#include "mado/DefaultLimits.h"
#include "mado/DefaultPolicy.h"
#include "mado/Exchange.h"
#include "mado/GuardedBuffer.h"
#include "mado/HeaderParser.h"
#include "mado/HeaderPool.h"
#include "mado/HeaderStatus.h"
#include "mado/HeaderSummary.h"
#include "mado/HttpVersion.h"
#include "mado/LineStatus.h"
#include "mado/Method.h"
#include "mado/OutputBuffer.h"
#include "mado/RequestLine.h"
#include "mado/ResponseSummary.h"
#include "mado/Route.h"
#include "mado/RouteLookup.h"
#include "mado/check.h"
#include "mado/error_status.h"
#include "mado/find_route.h"
#include "mado/http_status.h"
#include "mado/kMaxStringScan.h"
#include "mado/parse_request_line.h"
#include "mado/read_line.h"
#include "mado/send_method_not_allowed.h"

namespace mado {

constexpr uint16_t kMinLineBufferSize = 16U;
constexpr uint8_t kMinOutputBufferSize = 8U;

template <typename ClientT, typename AppT, typename LimitsT = DefaultLimits,
          typename PolicyT = DefaultPolicy>
class Server {
 public:
  static_assert(LimitsT::kLineBufferSize >= kMinLineBufferSize,
                "the line buffer must hold a request line");
  static_assert(LimitsT::kLineBufferSize <= kMaxStringScan,
                "the line buffer exceeds the scan bound");
  static_assert(LimitsT::kOutputBufferSize >= kMinOutputBufferSize,
                "the output buffer is too small");

  using ExchangeType = Exchange<ClientT, LimitsT>;

  Server(const Route* const routes, const char* const* const header_names)
      : routes_(routes), header_names_(header_names) {
  }

  void process(ClientT& client) {
    const bool connected = client.connected() != 0U;
    const bool configured = check(routes_ != nullptr);
    if ((!connected) || (!configured)) {
      return;
    }
    pool_.reset();
    out_.attach(client);
    const bool keep_open = serve(client);
    const bool flushed = out_.flush();
    out_.detach();
    const bool intact = barriers_intact();
    barrier_faults_ = static_cast<uint16_t>(barrier_faults_ + (intact ? 0U : 1U));
    const bool close = (!keep_open) || (!intact) || (!flushed);
    if (close) {
      client.stop();
    }
  }

  uint16_t barrier_faults() const {
    const bool intact = check(barriers_intact());
    const bool counted = check((barrier_faults_ > 0U) || intact);
    return (intact && counted) ? barrier_faults_ : static_cast<uint16_t>(barrier_faults_ + 1U);
  }

 private:
  bool serve(ClientT& client) {
    const uint32_t started = millis();
    const bool configured = check(routes_ != nullptr) && check(line_.intact());
    const LineStatus line_status =
        configured ? read_line<LimitsT>(client, started, line_.data(), LimitsT::kLineBufferSize)
                   : LineStatus::kClosed;
    const RequestLine none = {Method::kUnknown, "", "", false, HttpVersion::kUnsupported};
    if (line_status != LineStatus::kOk) {
      return reject(client, error_status(line_status), none, started);
    }
    const RequestLine request = parse_request_line(line_.data());
    const uint16_t line_problem = line_error(request);
    if (line_problem != http::kNoResponse) {
      return reject(client, line_problem, request, started);
    }
    HeaderParser<LimitsT, ClientT, PolicyT> parser(client, header_names_, pool_, started);
    const HeaderSummary headers = parser.parse();
    if (headers.status != HeaderStatus::kOk) {
      return reject(client, error_status(headers.status), request, started);
    }
    const bool host_named = headers.has_host || (request.version == HttpVersion::kHttp10);
    if ((!host_named) || (!headers.host_trusted)) {
      return reject(client, http::kBadRequest, request, started);
    }
    return dispatch(client, request, headers, started);
  }

  static uint16_t line_error(const RequestLine& request) {
    const bool parsed = check(request.valid || (request.path == nullptr));
    const bool versioned = check(request.valid || (request.version == HttpVersion::kUnsupported));
    if ((!request.valid) || (!parsed) || (!versioned)) {
      return http::kBadRequest;
    }
    if (request.version == HttpVersion::kUnsupported) {
      return http::kHttpVersionNotSupported;
    }
    return (request.method == Method::kUnknown) ? http::kNotImplemented : http::kNoResponse;
  }

  bool dispatch(ClientT& client, const RequestLine& request, const HeaderSummary& headers,
                const uint32_t started) {
    const bool valid = check(request.valid) && check(request.path != nullptr);
    const RouteLookup unrouted = {false, false, 0U};
    const RouteLookup lookup =
        valid ? find_route<LimitsT::kMaxRoutes>(routes_, request.method, request.path) : unrouted;
    ExchangeType exchange(client, out_, pool_, header_names_, request, headers, started);
    bool keep_open = false;
    if (lookup.found) {
      keep_open = !AppT::handle(exchange, lookup.id);
    } else {
      const bool sent = lookup.path_matched ? send_method_not_allowed<LimitsT::kMaxRoutes>(
                                                  exchange, routes_, request.path)
                                            : exchange.send_empty(http::kNotFound);
      static_cast<void>(sent);
    }
    finish(exchange, request, keep_open, started);
    return keep_open;
  }

  void finish(ExchangeType& exchange, const RequestLine& request, const bool keep_open,
              const uint32_t started) {
    const uint16_t status = exchange.status_code();
    const bool answered = check(status <= http::kLastValid) && (status != http::kNoResponse);
    if (answered) {
      PolicyT::on_response(request, ResponseSummary{status, started});
    }
    if (keep_open) {
      return;
    }
    const bool flushed = check(out_.intact()) && out_.flush();
    const bool drained = flushed && exchange.discard_body();
    static_cast<void>(drained);
  }

  bool reject(ClientT& client, const uint16_t code, const RequestLine& request,
              const uint32_t started) {
    const bool valid = check(code == http::kNoResponse) ||
                       (check(code >= http::kFirstValid) && check(code <= http::kLastValid));
    if ((code == http::kNoResponse) || (!valid)) {
      return false;
    }
    const HeaderSummary empty = {HeaderStatus::kOk, 0U, false, false, false, false};
    const RequestLine none = {Method::kUnknown, "", "", false, HttpVersion::kUnsupported};
    const RequestLine& reported = request.valid ? request : none;
    ExchangeType exchange(client, out_, pool_, header_names_, reported, empty, started);
    const bool sent = exchange.send_empty(code);
    if (sent) {
      PolicyT::on_response(reported, ResponseSummary{code, started});
    }
    return false;
  }

  bool barriers_intact() const {
    const bool line_ok = check(line_.intact());
    const bool pool_ok = check(pool_.intact());
    const bool out_ok = check(out_.intact());
    return line_ok && pool_ok && out_ok;
  }

  const Route* routes_;
  const char* const* header_names_;
  GuardedBuffer<char, LimitsT::kLineBufferSize> line_;
  HeaderPool<LimitsT> pool_;
  OutputBuffer<ClientT, LimitsT> out_;
  uint16_t barrier_faults_ = 0U;
};

}

#endif
