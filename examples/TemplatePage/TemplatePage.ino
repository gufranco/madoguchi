// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <Ethernet.h>
#include <Madoguchi.h>
#include <SPI.h>
#include <mado/http_status.h>
#include <mado/send_template.h>
#include <string.h>

using WebExchange = mado::Exchange<EthernetClient, mado::DefaultLimits>;

enum RouteId : uint8_t { kPage };

constexpr uint8_t kSensorPin = A0;
constexpr uint32_t kMillisPerSecond = 1000UL;

struct PageTags {
  static bool render(WebExchange& exchange, const char* const tag) {
    if (strcmp_P(tag, PSTR("SENSOR")) == 0) {
      static_cast<void>(exchange.print(static_cast<uint32_t>(analogRead(kSensorPin))));
      return true;
    }
    if (strcmp_P(tag, PSTR("UPTIME")) == 0) {
      static_cast<void>(exchange.print(millis() / kMillisPerSecond));
      return true;
    }
    return false;
  }
};

bool send_page(WebExchange& exchange) {
  const bool status = exchange.send_status(mado::http::kOk);
  const bool type = exchange.send_content_type(F("text/html; charset=utf-8"));
  const bool ended = exchange.end_headers();
  const bool rendered = mado::send_template<WebExchange, PageTags>(
      exchange, F("<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
                  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                  "<title>Sensor</title><h1>Sensor</h1>"
                  "<p>Reading: $SENSOR$</p><p>Up for $UPTIME$ s</p>"
                  "<p>Unknown tags stay as they are: $OTHER$</p></html>"));
  return status && type && ended && rendered;
}

struct WebApp {
  static bool handle(WebExchange& exchange, const uint8_t route) {
    const bool known = route == kPage;
    return known ? send_page(exchange) : exchange.send_empty(mado::http::kNotFound);
  }
};

const char kRootPath[] PROGMEM = "/";
const mado::Route kRoutes[] PROGMEM = {
    {&kRootPath[0], mado::Method::kGet, kPage},
    {nullptr, mado::Method::kAny, 0U},
};

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp> web(&kRoutes[0], nullptr);

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
