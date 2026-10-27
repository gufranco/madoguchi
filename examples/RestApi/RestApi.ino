// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <EEPROM.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Madoguchi.h>
#include <SD.h>
#include <SPI.h>
#include <mado/MdnsResponder.h>
#include <mado/NetworkAction.h>
#include <mado/NetworkWatch.h>
#include <mado/begin_network.h>
#include <mado/load_mac.h>

#include "RestApp.h"
#include "arduino_secrets.h"
#include "generated_api.h"

using WebApp = RestApp<EthernetClient, secrets::Provisioned, SDClass, FILE_READ, FILE_WRITE>;

struct LoggingPolicy {
  static constexpr bool kTrustedHostsOnly = false;

  static void on_response(const mado::RequestLine& request, const mado::ResponseSummary& response) {
    WebApp::log_response(request, response);
  }
};

constexpr int kSessionAddress = 0;
constexpr uint32_t kLastSession = 0x7FFFFFFFU;
constexpr uint16_t kMacAddress = 8U;
constexpr uint32_t kDhcpTimeoutMs = 10000UL;
constexpr uint8_t kEntropyPins = 6U;
constexpr uint8_t kSdChipSelect = 4U;

uint8_t mac_address[mado::kMacSize] = {};
EthernetServer ethernet_server(80);
EthernetUDP mdns_socket;
mado::MdnsResponder<EthernetUDP, IPAddress> mdns(&kRestHostName[0]);
mado::NetworkWatch network_watch;
mado::Server<EthernetClient, WebApp, rest_api::Limits, LoggingPolicy> web(
    &rest_api::kRoutes[0], &rest_api::kHeaderNames[0]);

uint32_t next_session() {
  uint32_t stored = 0U;
  static_cast<void>(EEPROM.get(kSessionAddress, stored));
  const bool fresh = (stored == 0U) || (stored >= kLastSession);
  const uint32_t session = fresh ? 1U : (stored + 1U);
  static_cast<void>(EEPROM.put(kSessionAddress, session));
  return session;
}

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
      mado::begin_network(Ethernet, &mac_address[0], IPAddress(192, 168, 1, 177), kDhcpTimeoutMs);
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

void setup() {
  WebApp::begin(next_session(), SD, kSdChipSelect);
  const bool known = mado::load_mac(EEPROM, kMacAddress, mac_address, entropy());
  static_cast<void>(known);
  start_network();
}

void loop() {
  WebApp::tick(millis());
  watch_network();
  const bool answered = mdns.poll(mdns_socket, Ethernet.localIP());
  static_cast<void>(answered);
  EthernetClient client = ethernet_server.available();
  const bool has_client = static_cast<bool>(client);
  if (has_client) {
    web.process(client);
  }
}
