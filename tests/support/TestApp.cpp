#include "TestApp.h"

std::vector<uint8_t> TestApp::calls;
std::string TestApp::last_path;
std::string TestApp::last_query;
std::string TestApp::last_header;
std::string TestApp::last_body;

namespace {

bool send_text(TestExchange& exchange, uint16_t code, const char* body) {
  exchange.send_status(code);
  exchange.send_content_type(F("text/plain"));
  exchange.end_headers();
  exchange.print(body);
  return true;
}

bool handle_echo(TestExchange& exchange) {
  TestApp::last_path = exchange.path();
  TestApp::last_query = exchange.query();
  const char* trace = exchange.header("X-Trace");
  TestApp::last_header = trace == nullptr ? "<none>" : trace;
  char name[32];
  const bool found = exchange.query_param("name", name, sizeof(name));
  return send_text(exchange, 200U, found ? name : "<missing>");
}

bool handle_upload(TestExchange& exchange) {
  TestApp::last_body.clear();
  uint8_t chunk[16];
  for (int i = 0; i < 100000; ++i) {
    const int16_t got = exchange.read_body(chunk, sizeof(chunk));
    if (got <= 0) {
      break;
    }
    TestApp::last_body.append(reinterpret_cast<const char*>(chunk), static_cast<size_t>(got));
  }
  const bool complete = exchange.remaining_body() == 0U;
  return send_text(exchange, complete ? 201U : 400U, complete ? "stored" : "short");
}

bool handle_cors(TestExchange& exchange) {
  exchange.send_status(204U);
  exchange.send_cors_headers();
  exchange.end_headers();
  return true;
}

bool handle_keep_open(TestExchange& exchange) {
  exchange.send_status(200U);
  exchange.end_headers();
  return false;
}

}

bool TestApp::handle(TestExchange& exchange, uint8_t route) {
  calls.push_back(route);
  switch (route) {
    case kRouteIndex: return send_text(exchange, 200U, "<h1>Hello</h1>");
    case kRouteEcho: return handle_echo(exchange);
    case kRouteUpload: return handle_upload(exchange);
    case kRouteKeepOpen: return handle_keep_open(exchange);
    case kRouteWildcard: return send_text(exchange, 200U, exchange.path());
    case kRouteCors: return handle_cors(exchange);
    default: return send_text(exchange, 500U, "unknown");
  }
}

const mado::Route* test_routes() {
  static const mado::Route kRoutes[] = {
      {"/", mado::Method::kGet, kRouteIndex},
      {"/echo", mado::Method::kGet, kRouteEcho},
      {"/upload/*", mado::Method::kPut, kRouteUpload},
      {"/stream", mado::Method::kGet, kRouteKeepOpen},
      {"/files/*", mado::Method::kGet, kRouteWildcard},
      {"/api/*", mado::Method::kOptions, kRouteCors},
      {nullptr, mado::Method::kAny, 0U},
  };
  return kRoutes;
}

const char* const* test_header_names() {
  static const char* const kNames[] = {"X-Trace", "Host", nullptr};
  return kNames;
}
