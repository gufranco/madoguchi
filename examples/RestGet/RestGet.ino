// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <Ethernet.h>
#include <Madoguchi.h>
#include <SPI.h>

#include "GetApp.h"
#include "generated_api.h"

#include <SdFat.h>

using WebApp = GetApp<EthernetClient, SdFs, FILE_READ>;

constexpr uint8_t kSdChipSelect = 4U;

SdFs sd_card;

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp, rest_get::Limits> web(&rest_get::kRoutes[0],
                                                           &rest_get::kHeaderNames[0]);

void setup() {
  WebApp::begin(sd_card, kSdChipSelect);
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
