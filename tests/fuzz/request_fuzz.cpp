#include <Madoguchi.h>
#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <string>

#include "FakeClient.h"
#include "FakeClock.h"
#include "FuzzTarget.h"
#include "TestApp.h"
#include "mado/DefaultLimits.h"
#include "mado/DefaultPolicy.h"
#include "mado/RequestLine.h"
#include "mado/ResponseSummary.h"

namespace {

struct FuzzHosts {
  static constexpr bool kTrustedHostsOnly = true;

  static const char* const* trusted_hosts() {
    static const char* const kHosts[] = {"board.local", "[::1]", "192.168.1.177", nullptr};
    return &kHosts[0];
  }

  static void on_response(const mado::RequestLine& request, const mado::ResponseSummary& response) {
    const bool answered = (response.status >= 100U) && (response.status <= 599U);
    if ((!answered) || (request.valid && (request.path == nullptr))) {
      std::abort();
    }
  }
};

template <typename PolicyT>
void serve(const std::string& input) {
  fake_clock::set(0U);
  TestApp::calls.clear();
  mado::Server<FakeClient, TestApp, mado::DefaultLimits, PolicyT> server(test_routes(),
                                                                         test_header_names());
  FakeClient client(input);
  client.close_when_drained();
  server.process(client);
  if (server.barrier_faults() != 0U) {
    std::abort();
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);
  serve<mado::DefaultPolicy>(input);
  serve<FuzzHosts>(input);
  return 0;
}
