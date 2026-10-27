// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <EthernetENC.h>
#include <Madoguchi.h>
#include <SPI.h>

using WebExchange = mado::Exchange<EthernetClient, mado::DefaultLimits>;

enum RouteId : uint8_t { kIndex };

struct WebApp {
  static bool handle(WebExchange& exchange, const uint8_t route) {
    const bool known = route == kIndex;
    if (!known) {
      return exchange.send_empty(404U);
    }
    const bool status = exchange.send_status(200U);
    const bool type = exchange.send_content_type(F("text/html"));
    const bool ended = exchange.end_headers();
    const size_t written = exchange.print(F("<html><body><h1>Hello World!</h1></body></html>\n"));
    return status && type && ended && (written > 0U);
  }
};

const char kRootPath[] PROGMEM = "/";
const mado::Route kRoutes[] PROGMEM = {
    {&kRootPath[0], mado::Method::kGet, kIndex},
    {nullptr, mado::Method::kAny, 0U},
};

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp> web(kRoutes, nullptr);

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
