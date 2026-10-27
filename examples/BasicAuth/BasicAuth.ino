// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <Ethernet.h>
#include <Madoguchi.h>
#include <SPI.h>
#include <mado/check_basic_auth.h>
#include <mado/http_status.h>
#include <mado/send_auth_challenge.h>

using WebExchange = mado::Exchange<EthernetClient, mado::DefaultLimits>;

enum RouteId : uint8_t { kPublic, kAdmin };

bool send_text(WebExchange& exchange, const __FlashStringHelper* const text) {
  const bool status = exchange.send_status(mado::http::kOk);
  const bool type = exchange.send_content_type(F("text/plain; charset=utf-8"));
  const bool ended = exchange.end_headers();
  const size_t written = exchange.print(text);
  return status && type && ended && (written > 0U);
}

bool send_admin(WebExchange& exchange) {
  const bool allowed =
      mado::check_basic_auth(exchange.header(F("Authorization")), F("YWRtaW46Y2hhbmdlLW1l"));
  if (!allowed) {
    return mado::send_auth_challenge(exchange, F("Madoguchi"));
  }
  return send_text(exchange, F("Welcome, admin.\n"));
}

struct WebApp {
  static bool handle(WebExchange& exchange, const uint8_t route) {
    switch (route) {
      case kPublic: return send_text(exchange, F("Anyone can read this.\n"));
      case kAdmin: return send_admin(exchange);
      default: return exchange.send_empty(mado::http::kNotFound);
    }
  }
};

const char kRootPath[] PROGMEM = "/";
const char kAdminPath[] PROGMEM = "/admin";
const mado::Route kRoutes[] PROGMEM = {
    {&kRootPath[0], mado::Method::kGet, kPublic},
    {&kAdminPath[0], mado::Method::kGet, kAdmin},
    {nullptr, mado::Method::kAny, 0U},
};

const char kAuthorizationHeader[] PROGMEM = "Authorization";
const char* const kHeaderNames[] PROGMEM = {&kAuthorizationHeader[0], nullptr};

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp> web(&kRoutes[0], &kHeaderNames[0]);

void setup() {
  Ethernet.begin(mac_address, IPAddress(192, 168, 1, 177));
  ethernet_server.begin();
}

void loop() {
  EthernetClient client = ethernet_server.available();
  const bool has_client = static_cast<bool>(client);
  if (has_client) {
    web.process(client);
  }
}
