// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <EEPROM.h>
#include <Ethernet.h>
#include <Madoguchi.h>
#include <SPI.h>
#include <mado/UptimeClock.h>

#include "arduino_secrets.h"
#include "src/http/api.generated.h"
#include "src/http/api.router.h"
#include "src/identity/adapters/card_failure.store.h"
#include "src/identity/adapters/eeprom_session.counter.h"
#include "src/identity/adapters/flash_secrets.authority.h"
#include "src/identity/application/access.service.h"

#include <SdFat.h>

using Authority = identity::FlashSecretsAuthority<secrets::Provisioned>;
using Failures = identity::CardFailureLog<SdFs, FILE_READ, FILE_WRITE>;
using Access = identity::AccessService<Authority, Failures>;
using WebApp = api::Router<EthernetClient, Access, SdFs, FILE_READ>;

constexpr uint8_t kSdChipSelect = 4U;

SdFs sd_card;
Authority authority;
mado::UptimeClock uptime;
identity::EepromSessionCounter<decltype(EEPROM)> sessions(EEPROM);

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp, rest_auth::Limits> web(&rest_auth::kRoutes[0],
                                                            &rest_auth::kHeaderNames[0]);

void setup() {
  const bool card_ready = sd_card.begin(kSdChipSelect);
  SdFs* const card = card_ready ? &sd_card : nullptr;
  static Failures failures(card);
  static Access access(authority, failures);
  access.begin(sessions.advance());
  WebApp::attach(access, uptime, card);
  Ethernet.begin(mac_address, IPAddress(192, 168, 1, 177));
  ethernet_server.begin();
}

void loop() {
  uptime.advance(millis());
  EthernetClient client = ethernet_server.available();
  const bool has_client = static_cast<bool>(client);
  if (has_client) {
    web.process(client);
  }
}
