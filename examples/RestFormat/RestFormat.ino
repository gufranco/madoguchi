// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <Ethernet.h>
#include <Madoguchi.h>
#include <SPI.h>

#include "src/card/application/format.service.h"
#include "src/http/api.generated.h"
#include "src/http/api.router.h"

#include "src/card/adapters/sdfat_card.driver.h"

using Service = card::FormatService<card::SdFatCardDriver>;
using WebApp = api::Router<EthernetClient, Service>;

Service service;

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp, rest_format::Limits> web(&rest_format::kRoutes[0],
                                                              &rest_format::kHeaderNames[0]);

void setup() {
  const bool found = service.begin();
  static_cast<void>(found);
  WebApp::attach(service);
  Ethernet.begin(mac_address, IPAddress(192, 168, 1, 177));
  ethernet_server.begin();
}

void loop() {
  EthernetClient client = ethernet_server.available();
  const bool has_client = static_cast<bool>(client);
  if (has_client) {
    web.process(client);
  }
  service.step();
}
