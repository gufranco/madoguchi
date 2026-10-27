#include <Madoguchi.h>
#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <string>

#include "../../examples/RestFiles/FilesApp.h"
#include "../../examples/RestFiles/generated_api.h"
#include "../support/FakeSd.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FuzzTarget.h"

namespace {

using App = FilesApp<FakeClient, FakeSd, FakeSdFile, FILE_READ, FILE_WRITE>;
using AppServer = mado::Server<FakeClient, App, rest_files::Limits>;

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);
  fake_clock::set(0U);
  FakeSd card;
  card.files()["/HELLO.TXT"] = "hello world";
  App::begin(card, 4U, 7U);
  AppServer server(&rest_files::kRoutes[0], &rest_files::kHeaderNames[0]);
  FakeClient client(input);
  client.close_when_drained();
  server.process(client);
  if (server.barrier_faults() != 0U) {
    std::abort();
  }
  return 0;
}
