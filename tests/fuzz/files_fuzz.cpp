#include <Madoguchi.h>
#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <string>

#include "../../examples/RestFiles/src/files/application/file_library.service.h"
#include "../../examples/RestFiles/src/http/api.generated.h"
#include "../../examples/RestFiles/src/http/api.router.h"
#include "../../examples/RestFiles/src/identity/adapters/card_failure.store.h"
#include "../../examples/RestFiles/src/identity/adapters/flash_secrets.authority.h"
#include "../../examples/RestFiles/src/identity/application/access.service.h"
#include "../support/FakeSd.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FuzzTarget.h"
#include "TestAccess.h"
#include "TestSecrets.h"

namespace {

using Library = files::FileLibrary<FakeSd, FakeSdFile, FILE_READ, FILE_WRITE>;
using Authority = identity::FlashSecretsAuthority<test_secrets::TestSecrets>;
using Failures = identity::CardFailureLog<FakeSd, FILE_READ, FILE_WRITE>;
using Access = identity::AccessService<Authority, Failures>;
using Gatekeeper = TestAccess<Authority, Failures, Access>;
using App = api::Router<FakeClient, Access, Library, FakeSd, FakeSdFile, FILE_READ>;
using AppServer = mado::Server<FakeClient, App, rest_files::Limits>;

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);
  fake_clock::set(0U);
  FakeSd card;
  card.files()["/HELLO.TXT"] = "hello world";
  static Library library;
  static_cast<void>(library.begin(4U, card, 7U));
  Gatekeeper& keeper = fresh_gatekeeper<Gatekeeper>();
  App::attach_access(keeper.access, keeper.clock);
  App::attach(library);
  AppServer server(&rest_files::kRoutes[0], &rest_files::kHeaderNames[0]);
  FakeClient client(input);
  client.close_when_drained();
  server.process(client);
  if (server.barrier_faults() != 0U) {
    std::abort();
  }
  return 0;
}
