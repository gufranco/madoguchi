// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <EEPROM.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Madoguchi.h>
#include <SPI.h>
#include <mado/MdnsResponder.h>
#include <mado/NetworkAction.h>
#include <mado/NetworkWatch.h>
#include <mado/UptimeClock.h>
#include <mado/begin_network.h>
#include <mado/load_mac.h>

#include "arduino_secrets.h"
#include "src/files/application/file_library.service.h"
#include "src/http/api.generated.h"
#include "src/http/api.router.h"
#include "src/identity/adapters/card_failure.store.h"
#include "src/identity/adapters/eeprom_session.counter.h"
#include "src/identity/adapters/flash_secrets.authority.h"
#include "src/identity/application/access.service.h"

#include <SdFat.h>

using Library = files::FileLibrary<SdFs, FsFile, FILE_READ, FILE_WRITE>;
using Authority = identity::FlashSecretsAuthority<secrets::Provisioned>;
using Failures = identity::CardFailureLog<SdFs, FILE_READ, FILE_WRITE>;
using Access = identity::AccessService<Authority, Failures>;
using WebApp = api::Router<EthernetClient, Access, Library, SdFs, FsFile, FILE_READ>;

constexpr uint8_t kSdChipSelect = 4U;

SdFs sd_card;

constexpr uint16_t kMacAddress = 8U;
constexpr uint32_t kDhcpTimeoutMs = 10000UL;
constexpr uint8_t kEntropyPins = 6U;
const char kHostName[] PROGMEM = "madofiles";

uint8_t mac_address[mado::kMacSize] = {};
EthernetServer ethernet_server(80);
EthernetUDP mdns_socket;
mado::MdnsResponder<EthernetUDP, IPAddress> mdns(&kHostName[0]);
mado::NetworkWatch network_watch;
mado::Server<EthernetClient, WebApp, rest_files::Limits> web(&rest_files::kRoutes[0],
                                                             &rest_files::kHeaderNames[0]);

uint32_t entropy() {
  uint32_t mixed = micros();
  for (uint8_t pin = 0U; pin < kEntropyPins; ++pin) {
    const uint32_t noise = static_cast<uint32_t>(analogRead(A0 + pin));
    mixed = (mixed << 5U) ^ (mixed >> 27U) ^ noise;
  }
  return mixed;
}

void start_network() {
  const bool leased =
      mado::begin_network(Ethernet, &mac_address[0], IPAddress(192, 168, 1, 178), kDhcpTimeoutMs);
  static_cast<void>(leased);
  ethernet_server.begin();
  const uint8_t joined = mdns_socket.beginMulticast(IPAddress(224, 0, 0, 251), mado::kMdnsPort);
  static_cast<void>(joined);
}

void watch_network() {
  const EthernetHardwareStatus chip = Ethernet.hardwareStatus();
  const EthernetLinkStatus link = Ethernet.linkStatus();
  const mado::NetworkSample sample = {chip != EthernetNoHardware, link != Unknown, link == LinkON};
  const mado::NetworkAction action = network_watch.update(sample);
  if (action == mado::NetworkAction::kRestart) {
    start_network();
  }
  const int renewed = Ethernet.maintain();
  static_cast<void>(renewed);
}

Authority authority;
mado::UptimeClock uptime;
identity::EepromSessionCounter<decltype(EEPROM)> sessions(EEPROM);

void setup() {
  static Library library;
  const bool mounted = library.begin(kSdChipSelect, sd_card, entropy());
  static_cast<void>(mounted);
  static Failures failures(library.mounted());
  static Access access(authority, failures);
  access.begin(sessions.advance());
  WebApp::attach_access(access, uptime);
  WebApp::attach(library);
  const bool known = mado::load_mac(EEPROM, kMacAddress, mac_address, entropy());
  static_cast<void>(known);
  start_network();
}

void loop() {
  watch_network();
  const bool answered = mdns.poll(mdns_socket, Ethernet.localIP());
  static_cast<void>(answered);
  EthernetClient client = ethernet_server.available();
  const bool has_client = static_cast<bool>(client);
  if (has_client) {
    web.process(client);
  }
}
