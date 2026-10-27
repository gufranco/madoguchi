// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <Ethernet.h>
#include <Madoguchi.h>
#include <SD.h>
#include <SPI.h>
#include <mado/FlashText.h>
#include <mado/content_type_for.h>
#include <mado/http_status.h>
#include <mado/resolve_static_path.h>
#include <mado/send_file.h>

using WebExchange = mado::Exchange<EthernetClient, mado::DefaultLimits>;

enum RouteId : uint8_t { kLedStatus, kLedSet, kUpload, kStatic };

constexpr uint8_t kLedPin = 7U;
constexpr uint8_t kSdChipSelect = 4U;
constexpr uint8_t kPathSize = 40U;
constexpr uint8_t kChunkSize = 32U;
constexpr uint8_t kUploadPrefixLength = 7U;
constexpr uint32_t kMaxChunks = 0x08000000UL;

bool led_on = false;

void apply_led(const bool on) {
  led_on = on;
  digitalWrite(kLedPin, on ? HIGH : LOW);
}

bool send_led_state(WebExchange& exchange) {
  const bool status = exchange.send_status(mado::http::kOk);
  const bool type = exchange.send_content_type(F("application/json"));
  const bool cache = exchange.send_header(F("Cache-Control"), mado::FlashText(F("no-store")));
  const bool ended = exchange.end_headers();
  const size_t written = exchange.print(led_on ? F("{\"led\":1}") : F("{\"led\":0}"));
  return status && type && cache && ended && (written > 0U);
}

bool set_led(WebExchange& exchange) {
  uint8_t command = 0U;
  const int16_t got = exchange.read_body(&command, 1U);
  const bool explicit_on = (got == 1) && (command == static_cast<uint8_t>('1'));
  const bool explicit_off = (got == 1) && (command == static_cast<uint8_t>('0'));
  const bool toggled = !led_on;
  apply_led(explicit_on || ((!explicit_off) && toggled));
  return send_led_state(exchange);
}

bool copy_body(WebExchange& exchange, File& file) {
  uint8_t chunk[kChunkSize] = {};
  for (uint32_t i = 0U; i < kMaxChunks; ++i) {
    const int16_t got = exchange.read_body(&chunk[0], sizeof(chunk));
    if (got <= 0) {
      return got == 0;
    }
    const size_t written = file.write(&chunk[0], static_cast<size_t>(got));
    if (written != static_cast<size_t>(got)) {
      return false;
    }
  }
  return false;
}

bool store_upload(WebExchange& exchange) {
  char path[kPathSize] = {};
  const char* const name = &exchange.path()[kUploadPrefixLength];
  const bool resolved = mado::resolve_static_path(name, "", F("INDEX.HTM"), &path[0], sizeof(path));
  if (!resolved) {
    return exchange.send_empty(mado::http::kBadRequest);
  }
  static_cast<void>(SD.remove(&path[0]));
  File file = SD.open(&path[0], FILE_WRITE);
  if (!file) {
    return exchange.send_empty(mado::http::kInternalServerError);
  }
  const bool copied = copy_body(exchange, file);
  file.close();
  return exchange.send_empty(copied ? mado::http::kCreated : mado::http::kBadRequest);
}

bool serve_static(WebExchange& exchange) {
  char path[kPathSize] = {};
  const bool resolved =
      mado::resolve_static_path(exchange.path(), "", F("INDEX.HTM"), &path[0], sizeof(path));
  if (!resolved) {
    return exchange.send_empty(mado::http::kBadRequest);
  }
  File file = SD.open(&path[0], FILE_READ);
  const bool readable = static_cast<bool>(file) && (!file.isDirectory());
  if (!readable) {
    file.close();
    return exchange.send_empty(mado::http::kNotFound);
  }
  const bool sent = mado::send_file(exchange, file, file.size(), mado::content_type_for(&path[0]),
                                    false, F("max-age=300"));
  file.close();
  static_cast<void>(sent);
  return true;
}

struct WebApp {
  static bool handle(WebExchange& exchange, const uint8_t route) {
    switch (route) {
      case kLedStatus: return send_led_state(exchange);
      case kLedSet: return set_led(exchange);
      case kUpload: return store_upload(exchange);
      case kStatic: return serve_static(exchange);
      default: return exchange.send_empty(mado::http::kNotFound);
    }
  }
};

const char kLedPath[] PROGMEM = "/led";
const char kUploadPrefixPath[] PROGMEM = "/upload/*";
const char kRootPrefixPath[] PROGMEM = "/*";
const mado::Route kRoutes[] PROGMEM = {
    {&kLedPath[0], mado::Method::kGet, kLedStatus},
    {&kLedPath[0], mado::Method::kPost, kLedSet},
    {&kUploadPrefixPath[0], mado::Method::kPut, kUpload},
    {&kRootPrefixPath[0], mado::Method::kGet, kStatic},
    {nullptr, mado::Method::kAny, 0U},
};

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp> web(&kRoutes[0], nullptr);
bool card_ready = false;

void setup() {
  pinMode(kLedPin, OUTPUT);
  apply_led(false);
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
