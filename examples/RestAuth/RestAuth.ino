// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <EEPROM.h>
#include <Ethernet.h>
#include <Madoguchi.h>
#include <SD.h>
#include <SPI.h>

#include "AuthApp.h"
#include "arduino_secrets.h"
#include "generated_api.h"

using WebApp = AuthApp<EthernetClient, secrets::Provisioned, SDClass, FILE_READ, FILE_WRITE>;

constexpr uint8_t kSdChipSelect = 4U;

constexpr int kSessionAddress = 0;
constexpr uint32_t kLastSession = 0x7FFFFFFFU;

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp, rest_auth::Limits> web(&rest_auth::kRoutes[0],
                                                            &rest_auth::kHeaderNames[0]);

uint32_t next_session() {
  uint32_t stored = 0U;
  static_cast<void>(EEPROM.get(kSessionAddress, stored));
  const bool fresh = (stored == 0U) || (stored >= kLastSession);
  const uint32_t session = fresh ? 1U : (stored + 1U);
  static_cast<void>(EEPROM.put(kSessionAddress, session));
  return session;
}

void setup() {
  WebApp::begin(next_session(), SD, kSdChipSelect);
  Ethernet.begin(mac_address, IPAddress(192, 168, 1, 177));
  ethernet_server.begin();
}

void loop() {
  WebApp::tick(millis());
  EthernetClient client = ethernet_server.available();
  const bool has_client = static_cast<bool>(client);
  if (has_client) {
    web.process(client);
  }
}
