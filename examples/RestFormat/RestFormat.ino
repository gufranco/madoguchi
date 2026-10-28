// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <EEPROM.h>
#include <Ethernet.h>
#include <Madoguchi.h>
#include <SPI.h>
#include <mado/UptimeClock.h>

#include "arduino_secrets.h"
#include "src/card/application/format.service.h"
#include "src/http/api.generated.h"
#include "src/http/api.router.h"
#include "src/identity/adapters/card_failure.store.h"
#include "src/identity/adapters/eeprom_session.counter.h"
#include "src/identity/adapters/flash_secrets.authority.h"
#include "src/identity/application/access.service.h"

#include "src/card/adapters/sdfat_card.driver.h"

using Service = card::FormatService<card::SdFatCardDriver>;
using Authority = identity::FlashSecretsAuthority<secrets::Provisioned>;
using Failures = identity::CardFailureLog<SdFs, FILE_READ, FILE_WRITE>;
using Access = identity::AccessService<Authority, Failures>;
using WebApp = api::Router<EthernetClient, Access, Service>;

Service service;

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp, rest_format::Limits> web(&rest_format::kRoutes[0],
                                                              &rest_format::kHeaderNames[0]);

Authority authority;
mado::UptimeClock uptime;
identity::EepromSessionCounter<decltype(EEPROM)> sessions(EEPROM);

void setup() {
  const bool found = service.begin();
  static_cast<void>(found);
  static Failures failures(nullptr);
  static Access access(authority, failures);
  access.begin(sessions.advance());
  WebApp::attach_access(access, uptime);
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
