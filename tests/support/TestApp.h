#ifndef MADO_TEST_TEST_APP_H
#define MADO_TEST_TEST_APP_H

#include <Madoguchi.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "FakeClient.h"

using TestExchange = mado::Exchange<FakeClient, mado::DefaultLimits>;

enum TestRoute : uint8_t {
  kRouteIndex,
  kRouteEcho,
  kRouteUpload,
  kRouteKeepOpen,
  kRouteWildcard,
  kRouteCors
};

struct TestApp {
  static bool handle(TestExchange& exchange, uint8_t route);

  static std::vector<uint8_t> calls;
  static std::string last_path;
  static std::string last_query;
  static std::string last_header;
  static std::string last_body;
};

const mado::Route* test_routes();
const char* const* test_header_names();

#endif
