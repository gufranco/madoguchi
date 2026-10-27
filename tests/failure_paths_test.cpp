#include <Arduino.h>
#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>

#include "ExchangeHarness.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "TestApp.h"
#include "mado/DefaultLimits.h"
#include "mado/DefaultPolicy.h"
#include "mado/FlashText.h"
#include "mado/HeaderPool.h"
#include "mado/HeaderStatus.h"
#include "mado/HeaderSummary.h"
#include "mado/HttpVersion.h"
#include "mado/Method.h"
#include "mado/OutputBuffer.h"
#include "mado/Problem.h"
#include "mado/Reply.h"
#include "mado/RequestLine.h"
#include "mado/ResponseSummary.h"
#include "mado/http_status.h"
#include "mado/send_cors_headers.h"
#include "mado/send_problem.h"

namespace {

using TestExchange = HarnessExchange;
using TestReply = mado::Reply<TestExchange>;

constexpr size_t kBudgetStep = 8U;
constexpr size_t kLargestBudget = 320U;
constexpr size_t kBuffered = HarnessLimits::kOutputBufferSize;

bool flushed_short(const size_t budget, const size_t full) {
  return (budget + kBuffered) < full;
}

const char* const* origin_header() {
  static const char* const kNames[] = {"Origin", nullptr};
  return &kNames[0];
}

const char* const* board_origins() {
  static const char* const kOrigins[] = {"http://board.local", nullptr};
  return &kOrigins[0];
}

struct FullReply {
  static bool send(TestExchange& exchange);
};

bool FullReply::send(TestExchange& exchange) {
  TestReply reply(exchange);
  return reply.status(mado::http::kCreated)
      .json()
      .no_store()
      .location(F("/api/items/"), 12U)
      .header(F("X-Count"), 2U)
      .header(F("X-Name"), "lamp")
      .header(F("X-Kind"), mado::FlashText(F("light")))
      .type(F("text/plain"))
      .length(0U)
      .end();
}

struct LongProblem {
  static bool send(TestExchange& exchange);
};

bool LongProblem::send(TestExchange& exchange) {
  return mado::send_problem(
      exchange, mado::Problem{
                    mado::http::kUnprocessableContent,
                    F("a detail long enough to cross more than one output buffer flush"),
                });
}

struct CorsHeaders {
  static bool send(TestExchange& exchange);
};

bool CorsHeaders::send(TestExchange& exchange) {
  const bool status = exchange.send_status(mado::http::kNoContent);
  return status && mado::send_cors_headers(exchange, board_origins());
}

template <typename SenderT>
size_t full_length() {
  ExchangeHarness harness;
  harness.capture(origin_header(), 0U, "http://board.local");
  TestExchange exchange = harness.make();
  const bool sent = SenderT::send(exchange);
  return sent ? harness.sent().size() : 0U;
}

class OutputBudgetTest : public ::testing::TestWithParam<size_t> {
 public:
  static TestExchange limited(ExchangeHarness& harness, const size_t budget) {
    harness.capture(origin_header(), 0U, "http://board.local");
    harness.client().fail_writes_after(budget);
    return harness.make();
  }
};

TEST_P(OutputBudgetTest, ReportsAReplyCutShortByTheClient) {
  // Arrange
  const size_t full = full_length<FullReply>();
  ExchangeHarness harness;
  TestExchange exchange = limited(harness, GetParam());

  // Act
  const bool ended = FullReply::send(exchange);

  // Assert
  EXPECT_GT(full, 0U);
  EXPECT_TRUE((!ended) || (!flushed_short(GetParam(), full)));
  EXPECT_TRUE(ended || (GetParam() < full));
}

TEST_P(OutputBudgetTest, ReportsAProblemCutShortByTheClient) {
  // Arrange
  const size_t full = full_length<LongProblem>();
  ExchangeHarness harness;
  TestExchange exchange = limited(harness, GetParam());

  // Act
  const bool sent = LongProblem::send(exchange);

  // Assert
  EXPECT_GT(full, 0U);
  EXPECT_TRUE((!sent) || (!flushed_short(GetParam(), full)));
  EXPECT_TRUE(sent || (GetParam() < full));
}

TEST_P(OutputBudgetTest, ReportsCorsHeadersCutShortByTheClient) {
  // Arrange
  const size_t full = full_length<CorsHeaders>();
  ExchangeHarness harness;
  TestExchange exchange = limited(harness, GetParam());

  // Act
  const bool sent = CorsHeaders::send(exchange);

  // Assert
  EXPECT_GT(full, 0U);
  EXPECT_TRUE((!sent) || (!flushed_short(GetParam(), full)));
  EXPECT_TRUE(sent || (GetParam() < full));
}

INSTANTIATE_TEST_SUITE_P(Budgets, OutputBudgetTest,
                         ::testing::Range(static_cast<size_t>(0U), kLargestBudget, kBudgetStep));

TEST(ReplyFailures, SkipsEveryStepOnceTheExchangeHasAnswered) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  const bool first = exchange.send_empty(mado::http::kNoContent);
  const size_t before = harness.sent().size();

  // Act
  const bool ended = FullReply::send(exchange);

  // Assert
  EXPECT_TRUE(first);
  EXPECT_FALSE(ended);
  EXPECT_EQ(harness.sent().size(), before);
}

TEST(ReplyFailures, RefusesANullType) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  TestReply reply(exchange);

  // Act
  const bool ended = reply.status(mado::http::kOk).type(nullptr).end();

  // Assert
  EXPECT_FALSE(ended);
  EXPECT_EQ(harness.sent().find("Content-Type"), std::string::npos);
}

TEST(ReplyFailures, RefusesNullHeaderNamesAndValues) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  TestReply reply(exchange);
  const char* const missing = nullptr;

  // Act
  const bool ended = reply.status(mado::http::kOk)
                         .header(nullptr, 1U)
                         .header(F("X-Name"), missing)
                         .header(nullptr, mado::FlashText(F("x")))
                         .location(nullptr, 1U)
                         .end();

  // Assert
  EXPECT_FALSE(ended);
  EXPECT_EQ(harness.sent().find("X-Name"), std::string::npos);
}

TEST(ReplyFailures, RefusesAnEndWithoutAStatus) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  TestReply reply(exchange);

  // Act
  const bool ended = reply.json().length(0U).end();

  // Assert
  EXPECT_FALSE(ended);
  EXPECT_EQ(harness.sent(), "");
}

TEST(CorsFailures, RefusesAMissingOriginList) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  const bool status = exchange.send_status(mado::http::kNoContent);

  // Act
  const bool sent = mado::send_cors_headers(exchange, nullptr);

  // Assert
  EXPECT_TRUE(status);
  EXPECT_FALSE(sent);
}

TEST(CorsFailures, LooksAtNoMoreThanEightOrigins) {
  // Arrange
  static const char* const kOrigins[] = {"a", "b", "c", "d", "e", "f", "g", "h", "i", nullptr};
  ExchangeHarness harness;
  harness.capture(origin_header(), 0U, "i");
  TestExchange exchange = harness.make();
  const bool status = exchange.send_status(mado::http::kNoContent);

  // Act
  const bool sent = mado::send_cors_headers(exchange, &kOrigins[0]);

  // Assert
  EXPECT_TRUE(status);
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent().find("Allow-Origin"), std::string::npos);
}

TEST(ProblemFailures, RefusesAStatusAboveTheValidRange) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_problem(exchange, mado::Problem{600U, nullptr});

  // Assert
  EXPECT_FALSE(sent);
  EXPECT_EQ(harness.sent(), "");
}

TEST(LocationFailures, RefusesANullPrefix) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  const bool status = exchange.send_status(mado::http::kCreated);

  // Act
  const bool sent = exchange.send_location(nullptr, 1U);

  // Assert
  EXPECT_TRUE(status);
  EXPECT_FALSE(sent);
}

TEST(LocationFailures, ReportsALocationTheClientRefused) {
  // Arrange
  ExchangeHarness harness;
  harness.detach_output();
  TestExchange exchange = harness.make();
  const bool status = exchange.send_status(mado::http::kCreated);

  // Act
  const bool sent = exchange.send_location(F("/api/items/"), 1U);

  // Assert
  EXPECT_FALSE(status);
  EXPECT_FALSE(sent);
}

TEST(DrainFailures, KeepsABodyTheClientWasNotAskedToSend) {
  // Arrange
  ExchangeHarness harness("abc", 3U, mado::Method::kPost);
  const mado::HeaderSummary headers = {mado::HeaderStatus::kOk, 3U, true, true, true, true};
  const mado::HeaderPool<HarnessLimits> pool;
  mado::OutputBuffer<FakeClient, HarnessLimits> out;
  out.attach(harness.client());
  const mado::RequestLine line = {mado::Method::kPost, "/x", "", true, mado::HttpVersion::kHttp11};
  TestExchange exchange(harness.client(), out, pool, nullptr, line, headers, 0U);

  // Act
  const bool drained = exchange.discard_body();

  // Assert
  EXPECT_FALSE(drained);
  EXPECT_EQ(harness.client().unread(), 3U);
}

TEST(DrainFailures, KeepsABodyLargerThanTheDrainLimit) {
  // Arrange
  const uint32_t declared = mado::DefaultLimits::kMaxDrainBytes + 1U;
  ExchangeHarness harness(std::string(declared, 'x'), declared, mado::Method::kPost);
  TestExchange exchange = harness.make();

  // Act
  const bool drained = exchange.discard_body();

  // Assert
  EXPECT_FALSE(drained);
  EXPECT_EQ(harness.client().unread(), declared);
}

struct HostCase {
  std::string host;
  const char* status_line = nullptr;
};

std::ostream& operator<<(std::ostream& stream, const HostCase& sample) {
  return stream << sample.host.size();
}

template <typename PolicyT>
std::string serve_with(const std::string& host) {
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp, mado::DefaultLimits, PolicyT> server(test_routes(),
                                                                         test_header_names());
  FakeClient client("GET / HTTP/1.1\r\nHost: " + host + "\r\n\r\n");
  server.process(client);
  return client.output().substr(0U, client.output().find("\r\n"));
}

class AnyHostTest : public ::testing::TestWithParam<HostCase> {};

TEST_P(AnyHostTest, AcceptsAnyWellFormedHostUpToTheLengthLimit) {
  // Arrange
  const HostCase& sample = GetParam();

  // Act
  const std::string line = serve_with<mado::DefaultPolicy>(sample.host);

  // Assert
  EXPECT_EQ(line, sample.status_line);
}

INSTANTIATE_TEST_SUITE_P(
    Lengths, AnyHostTest,
    ::testing::Values(HostCase{std::string(255U, 'a'), "HTTP/1.1 200 OK"},
                      HostCase{std::string(256U, 'a'), "HTTP/1.1 400 Bad Request"},
                      HostCase{std::string("a\0b", 3U), "HTTP/1.1 400 Bad Request"}));

struct NineHosts {
  static constexpr bool kTrustedHostsOnly = true;

  static const char* const* trusted_hosts() {
    static const char* const kHosts[] = {"a", "b", "c", "d", "e", "f", "g", "h", "i", nullptr};
    return &kHosts[0];
  }

  static void on_response(const mado::RequestLine& request, const mado::ResponseSummary& response) {
    static_cast<void>(request);
    static_cast<void>(response);
  }
};

struct NoHosts {
  static constexpr bool kTrustedHostsOnly = true;

  static const char* const* trusted_hosts() {
    static const char* const kHosts[] = {nullptr};
    return &kHosts[0];
  }

  static void on_response(const mado::RequestLine& request, const mado::ResponseSummary& response) {
    static_cast<void>(request);
    static_cast<void>(response);
  }
};

TEST(TrustedHostLimits, MatchesTheEighthHost) {
  // Arrange
  const std::string host = "h";

  // Act
  const std::string line = serve_with<NineHosts>(host);

  // Assert
  EXPECT_EQ(line, "HTTP/1.1 200 OK");
}

TEST(TrustedHostLimits, IgnoresHostsPastTheEighth) {
  // Arrange
  const std::string host = "i";

  // Act
  const std::string line = serve_with<NineHosts>(host);

  // Assert
  EXPECT_EQ(line, "HTTP/1.1 400 Bad Request");
}

TEST(TrustedHostLimits, RefusesEveryHostForAnEmptyList) {
  // Arrange
  FakeData data(20261104U);
  const std::string host = data.word(8);

  // Act
  const std::string line = serve_with<NoHosts>(host);

  // Assert
  EXPECT_EQ(line, "HTTP/1.1 400 Bad Request");
}

TEST(TrustedHostLimits, RefusesAHostWithAnEmbeddedNul) {
  // Arrange
  const std::string host("a\0", 2U);

  // Act
  const std::string line = serve_with<NineHosts>(host);

  // Assert
  EXPECT_EQ(line, "HTTP/1.1 400 Bad Request");
}

}
