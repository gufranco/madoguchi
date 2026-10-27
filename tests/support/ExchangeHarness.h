#ifndef MADO_TEST_EXCHANGE_HARNESS_H
#define MADO_TEST_EXCHANGE_HARNESS_H

#include <stdint.h>

#include <string>

#include "FakeClient.h"
#include "FakeClock.h"
#include "mado/DefaultLimits.h"
#include "mado/Exchange.h"
#include "mado/HeaderPool.h"
#include "mado/HeaderStatus.h"
#include "mado/HeaderSummary.h"
#include "mado/Method.h"
#include "mado/OutputBuffer.h"
#include "mado/RequestLine.h"

using HarnessLimits = mado::DefaultLimits;
using HarnessExchange = mado::Exchange<FakeClient, HarnessLimits>;

class ExchangeHarness {
 public:
  explicit ExchangeHarness(const std::string& input = "", uint32_t content_length = 0U,
                           mado::Method method = mado::Method::kGet)
      : client_(input),
        line_{method, "/x", "q=1", true, mado::HttpVersion::kHttp11},
        headers_{mado::HeaderStatus::kOk, content_length, content_length > 0U, false, true, true} {
    fake_clock::set(0U);
    out_.attach(client_);
  }

  HarnessExchange make() {
    return HarnessExchange(client_, out_, pool_, names_, line_, headers_, fake_clock::now());
  }

  void capture(const char* const* names, uint8_t slot, const std::string& value) {
    names_ = names;
    pool_.begin(slot);
    for (const char symbol : value) {
      pool_.append(symbol);
    }
    pool_.finish();
  }

  std::string sent() {
    out_.flush();
    return client_.output();
  }

  FakeClient& client() {
    return client_;
  }

  void detach_output() {
    out_.detach();
  }

  void set_line(const mado::RequestLine& line) {
    line_ = line;
  }

 private:
  FakeClient client_;
  mado::OutputBuffer<FakeClient, HarnessLimits> out_;
  mado::HeaderPool<HarnessLimits> pool_;
  const char* const* names_ = nullptr;
  mado::RequestLine line_;
  mado::HeaderSummary headers_;
};

#endif
