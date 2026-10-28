// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <Ethernet.h>
#include <Madoguchi.h>
#include <SPI.h>

#include "FormatApp.h"
#include "FormatJob.h"
#include "generated_api.h"

#include <SdFat.h>

constexpr uint8_t kSdChipSelect = 4U;
constexpr uint32_t kSdClockHz = 25000000UL;

struct SdFatDriver {
  using Device = SdCard;

  static SdCardFactory& factory() {
    static SdCardFactory instance;
    return instance;
  }

  static Device* open() {
    return factory().newCard(SdSpiConfig(kSdChipSelect, SHARED_SPI, kSdClockHz));
  }

  static bool format(Device& device, const CardFileSystem kind, uint8_t* const sector) {
    if (kind == CardFileSystem::kExFat) {
      ExFatFormatter formatter;
      return formatter.format(&device, sector, nullptr);
    }
    FatFormatter formatter;
    return formatter.format(&device, sector, nullptr);
  }

  static CardFileSystem mounted(Device& device) {
    static FsVolume volume;
    const bool opened = volume.begin(&device);
    const uint8_t type = opened ? volume.fatType() : 0U;
    switch (type) {
      case FAT_TYPE_FAT16: return CardFileSystem::kFat16;
      case FAT_TYPE_FAT32: return CardFileSystem::kFat32;
      case FAT_TYPE_EXFAT: return CardFileSystem::kExFat;
      default: return CardFileSystem::kNone;
    }
  }
};

using WebApp = FormatApp<EthernetClient, SdFatDriver>;

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp, rest_format::Limits> web(&rest_format::kRoutes[0],
                                                              &rest_format::kHeaderNames[0]);

void setup() {
  WebApp::begin();
  Ethernet.begin(mac_address, IPAddress(192, 168, 1, 177));
  ethernet_server.begin();
}

void loop() {
  EthernetClient client = ethernet_server.available();
  const bool has_client = static_cast<bool>(client);
  if (has_client) {
    web.process(client);
  }
  WebApp::step();
}
