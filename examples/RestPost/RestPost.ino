// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <Ethernet.h>
#include <Madoguchi.h>
#include <SPI.h>

#include "src/http/api.generated.h"
#include "src/http/api.router.h"
#include "src/items/adapters/ram_item.repository.h"

#include <SdFat.h>

using WebApp = api::Router<EthernetClient, items::RamItemRepository, SdFs, FILE_READ>;

constexpr uint8_t kSdChipSelect = 4U;

SdFs sd_card;
items::RamItemRepository repository;

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp, rest_post::Limits> web(&rest_post::kRoutes[0],
                                                            &rest_post::kHeaderNames[0]);

void setup() {
  const bool card_ready = sd_card.begin(kSdChipSelect);
  WebApp::attach(repository, card_ready ? &sd_card : nullptr);
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
