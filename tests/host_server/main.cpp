#include <Arduino.h>
#include <Madoguchi.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "../../examples/RestApi/src/board.composition.h"
#include "../../examples/RestApi/src/http/api.generated.h"
#include "../../examples/RestApi/src/http/api.router.h"
#include "../support/FakeSd.h"
#include "../support/TestSecrets.h"
#include "PosixClient.h"

namespace {

using HostBoard = Board<FakeSd, test_secrets::TestSecrets, FILE_READ, FILE_WRITE>;
using HostApp = api::Router<PosixClient, HostBoard>;

struct HostLog : mado::DefaultPolicy {
  static void on_response(const mado::RequestLine& request, const mado::ResponseSummary& response) {
    HostApp::log_response(request, response);
  }
};
using HostServer = mado::Server<PosixClient, HostApp, rest_api::Limits, HostLog>;

const auto kStarted = std::chrono::steady_clock::now();

int listen_on(const uint16_t port) {
  const int listener = socket(AF_INET, SOCK_STREAM, 0);
  const int reuse = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  const bool bound = bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
  const bool listening = bound && (listen(listener, 8) == 0);
  return listening ? listener : -1;
}

void load_card(FakeSd& card, const char* const folder) {
  const std::filesystem::path root(folder);
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    std::ifstream input(entry.path(), std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());
    card.files()["/" + std::filesystem::relative(entry.path(), root).generic_string()] = content;
  }
}

}

uint32_t millis() {
  const auto elapsed = std::chrono::steady_clock::now() - kStarted;
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

int main(int argc, char** argv) {
  const uint16_t port = static_cast<uint16_t>((argc > 1) ? std::atoi(argv[1]) : 8080);
  const long limit = (argc > 2) ? std::atol(argv[2]) : 100000L;
  signal(SIGPIPE, SIG_IGN);
  const int listener = listen_on(port);
  if (listener < 0) {
    std::perror("listen");
    return 1;
  }
  static FakeSd card;
  if (argc > 3) {
    load_card(card, argv[3]);
  }
  static HostBoard board;
  static_cast<void>(begin_board(board, 7U, card, 4U));
  HostApp::attach(board);
  HostServer server(&rest_api::kRoutes[0], &rest_api::kHeaderNames[0]);
  std::printf("listening on http://127.0.0.1:%u\n", port);
  std::fflush(stdout);
  for (long served = 0; served < limit; ++served) {
    PosixClient client(accept(listener, nullptr, nullptr));
    server.process(client);
    client.stop();
  }
  close(listener);
  return 0;
}
