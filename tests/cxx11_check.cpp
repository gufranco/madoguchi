#include <Madoguchi.h>
#include <stddef.h>
#include <stdint.h>

#include "mado/DefaultLimits.h"
#include "mado/Exchange.h"
#include "mado/Method.h"
#include "mado/Route.h"
#include "mado/http_status.h"

namespace {

struct MinimalClient {
  static int available() {
    return 0;
  }

  static int read() {
    return -1;
  }

  static int read(uint8_t* const buffer, const size_t size) {
    if ((buffer == nullptr) || (size == 0U)) {
      return -1;
    }
    buffer[0] = 0U;
    return 0;
  }

  static size_t write(const uint8_t value) {
    static_cast<void>(value);
    return 1U;
  }

  static size_t write(const uint8_t* const buffer, const size_t size) {
    return (buffer == nullptr) ? 0U : size;
  }

  uint8_t connected() const {
    return open_ ? 1U : 0U;
  }

  void stop() {
    open_ = false;
  }

 private:
  bool open_ = true;
};

struct MinimalApp {
  static bool handle(mado::Exchange<MinimalClient, mado::DefaultLimits>& exchange,
                     const uint8_t route) {
    return exchange.send_empty(route == 0U ? mado::http::kNoContent : mado::http::kNotFound);
  }
};

const mado::Route kRoutes[] = {
    {"/", mado::Method::kGet, 0U},
    {nullptr, mado::Method::kAny, 0U},
};

}

int main() {
  mado::Server<MinimalClient, MinimalApp> server(&kRoutes[0], nullptr);
  MinimalClient client;
  server.process(client);
  return static_cast<int>(server.barrier_faults());
}
