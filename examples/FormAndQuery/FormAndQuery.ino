// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#include <Ethernet.h>
#include <Madoguchi.h>
#include <SPI.h>
#include <mado/FormStatus.h>
#include <mado/http_status.h>
#include <mado/next_form_field.h>

using WebExchange = mado::Exchange<EthernetClient, mado::DefaultLimits>;

enum RouteId : uint8_t { kFormPage, kGreet, kSubmit };

constexpr uint8_t kFieldSize = 24U;
constexpr uint8_t kMaxFields = 8U;

bool send_form_page(WebExchange& exchange) {
  const bool status = exchange.send_status(mado::http::kOk);
  const bool type = exchange.send_content_type(F("text/html; charset=utf-8"));
  const bool ended = exchange.end_headers();
  const size_t written =
      exchange.print(F("<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
                       "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                       "<title>Form</title><form method=\"post\" action=\"/submit\">"
                       "<label>Name <input name=\"name\"></label> "
                       "<label>Color <input name=\"color\"></label> "
                       "<button>Send</button></form>"
                       "<p><a href=\"/greet?name=Ada\">Greet Ada</a></p></html>"));
  return status && type && ended && (written > 0U);
}

bool send_greeting(WebExchange& exchange) {
  char name[kFieldSize] = {};
  const bool found = exchange.query_param(F("name"), &name[0], sizeof(name));
  const bool status = exchange.send_status(mado::http::kOk);
  const bool type = exchange.send_content_type(F("text/plain; charset=utf-8"));
  const bool ended = exchange.end_headers();
  const size_t greeting = exchange.print(F("Hello, "));
  const size_t written = found ? exchange.print(&name[0]) : exchange.print(F("stranger"));
  return status && type && ended && (greeting > 0U) && (written > 0U);
}

bool send_submission(WebExchange& exchange) {
  const bool status = exchange.send_status(mado::http::kOk);
  const bool type = exchange.send_content_type(F("text/plain; charset=utf-8"));
  const bool ended = exchange.end_headers();
  for (uint8_t i = 0U; i < kMaxFields; ++i) {
    char name[kFieldSize] = {};
    char value[kFieldSize] = {};
    const mado::FormStatus field =
        mado::next_form_field(exchange, &name[0], sizeof(name), &value[0], sizeof(value));
    if (field != mado::FormStatus::kField) {
      break;
    }
    static_cast<void>(exchange.print(&name[0]));
    static_cast<void>(exchange.print(F(" = ")));
    static_cast<void>(exchange.print(&value[0]));
    static_cast<void>(exchange.print(F("\n")));
  }
  return status && type && ended;
}

struct WebApp {
  static bool handle(WebExchange& exchange, const uint8_t route) {
    switch (route) {
      case kFormPage: return send_form_page(exchange);
      case kGreet: return send_greeting(exchange);
      case kSubmit: return send_submission(exchange);
      default: return exchange.send_empty(mado::http::kNotFound);
    }
  }
};

const char kRootPath[] PROGMEM = "/";
const char kGreetPath[] PROGMEM = "/greet";
const char kSubmitPath[] PROGMEM = "/submit";
const mado::Route kRoutes[] PROGMEM = {
    {&kRootPath[0], mado::Method::kGet, kFormPage},
    {&kGreetPath[0], mado::Method::kGet, kGreet},
    {&kSubmitPath[0], mado::Method::kPost, kSubmit},
    {nullptr, mado::Method::kAny, 0U},
};

uint8_t mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer ethernet_server(80);
mado::Server<EthernetClient, WebApp> web(&kRoutes[0], nullptr);

void setup() {
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
