// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <ArduinoJson.h>
#include <Ethernet.h>
#include <Madoguchi.h>
#include <MadoguchiJson.h>
#include <SPI.h>
#include <mado/http_status.h>

using WebExchange = mado::Exchange<EthernetClient, mado::DefaultLimits>;

enum RouteId : uint8_t { kStatus };

constexpr uint8_t kAnalogPins = 6U;

bool send_status_document(WebExchange& exchange) {
  JsonDocument document;
  document[F("uptime_ms")] = millis();
  JsonArray analog = document[F("analog")].to<JsonArray>();
  for (uint8_t pin = 0U; pin < kAnalogPins; ++pin) {
    static_cast<void>(analog.add(analogRead(static_cast<uint8_t>(A0 + pin))));
  }
  return mado::send_json(exchange, mado::http::kOk, document);
}

struct WebApp {
  static bool handle(WebExchange& exchange, const uint8_t route) {
    const bool known = route == kStatus;
    return known ? send_status_document(exchange) : exchange.send_empty(mado::http::kNotFound);
  }
};

const char kApiStatusPath[] PROGMEM = "/api/status";
const mado::Route kRoutes[] PROGMEM = {
    {&kApiStatusPath[0], mado::Method::kGet, kStatus},
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
