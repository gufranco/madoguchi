#include <Madoguchi.h>
#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <string>

#include "../../examples/RestApi/src/board.composition.h"
#include "../../examples/RestApi/src/http/api.generated.h"
#include "../../examples/RestApi/src/http/api.router.h"
#include "../support/FakeSd.h"
#include "../support/TestSecrets.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FuzzTarget.h"

namespace {

using TestBoard = Board<FakeSd, test_secrets::TestSecrets, FILE_READ, FILE_WRITE>;
using App = api::Router<FakeClient, TestBoard>;
using AppServer = mado::Server<FakeClient, App, rest_api::Limits>;

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);
  fake_clock::set(0U);
  FakeSd card;
  static TestBoard board;
  static_cast<void>(begin_board(board, 7U, card, 4U));
  App::attach(board);
  AppServer server(&rest_api::kRoutes[0], &rest_api::kHeaderNames[0]);
  FakeClient client(input);
  client.close_when_drained();
  server.process(client);
  if (server.barrier_faults() != 0U) {
    std::abort();
  }
  return 0;
}
