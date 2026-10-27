// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <Ethernet.h>
#include <Madoguchi.h>
#include <SD.h>
#include <SPI.h>
#include <mado/accepts_gzip.h>
#include <mado/content_type_for.h>
#include <mado/http_status.h>
#include <mado/resolve_static_path.h>
#include <mado/send_file.h>

using WebExchange = mado::Exchange<EthernetClient, mado::DefaultLimits>;

enum RouteId : uint8_t { kStatic };

constexpr uint8_t kSdChipSelect = 4U;
constexpr uint8_t kPathSize = 40U;

File open_file(const char* const path) {
  File file = SD.open(path, FILE_READ);
  const bool directory = static_cast<bool>(file) && file.isDirectory();
  if (directory) {
    file.close();
    return File();
  }
  return file;
}

bool serve_static(WebExchange& exchange) {
  char path[kPathSize] = {};
  char gzip_path[kPathSize] = {};
  const bool resolved =
      mado::resolve_static_path(exchange.path(), "", F("INDEX.HTM"), &path[0], sizeof(path));
  if (!resolved) {
    return exchange.send_empty(mado::http::kBadRequest);
  }
  const bool wants_gzip = mado::accepts_gzip(exchange.header(F("Accept-Encoding")));
  bool has_gzip = false;
  if (wants_gzip) {
    has_gzip = mado::resolve_static_path(exchange.path(), "/GZ", F("INDEX.HTM"), &gzip_path[0],
                                         sizeof(gzip_path));
  }
  File file = has_gzip ? open_file(&gzip_path[0]) : File();
  const bool gzip = static_cast<bool>(file);
  if (!gzip) {
    file = open_file(&path[0]);
  }
  if (!file) {
    return exchange.send_empty(mado::http::kNotFound);
  }
  const bool sent = mado::send_file(exchange, file, file.size(), mado::content_type_for(&path[0]),
                                    gzip, F("max-age=300"));
  file.close();
  static_cast<void>(sent);
  return true;
}

struct WebApp {
  static bool handle(WebExchange& exchange, const uint8_t route) {
    const bool known = route == kStatic;
    return known ? serve_static(exchange) : exchange.send_empty(mado::http::kNotFound);
  }
};

const char kRootPrefixPath[] PROGMEM = "/*";
const mado::Route kRoutes[] PROGMEM = {
    {&kRootPrefixPath[0], mado::Method::kGet, kStatic},
    {nullptr, mado::Method::kAny, 0U},
};

const char kAcceptEncodingHeader[] PROGMEM = "Accept-Encoding";
const char* const kHeaderNames[] PROGMEM = {&kAcceptEncodingHeader[0], nullptr};

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp> web(&kRoutes[0], &kHeaderNames[0]);
bool card_ready = false;

void setup() {
  card_ready = SD.begin(kSdChipSelect);
  Ethernet.begin(mac_address, IPAddress(192, 168, 1, 177));
  ethernet_server.begin();
}

void loop() {
  EthernetClient client = ethernet_server.available();
  const bool has_client = static_cast<bool>(client);
  if (has_client && card_ready) {
    web.process(client);
  }
  if (has_client && (!card_ready)) {
    client.stop();
  }
}
