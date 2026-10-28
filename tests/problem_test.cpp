#include <Arduino.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>

#include "ExchangeHarness.h"
#include "FakeClient.h"
#include "FakeData.h"
#include "mado/FlashText.h"
#include "mado/JsonStatus.h"
#include "mado/Method.h"
#include "mado/Problem.h"
#include "mado/Reply.h"
#include "mado/http_status.h"
#include "mado/json_body_problem.h"
#include "mado/json_problem.h"
#include "mado/send_cors_headers.h"
#include "mado/send_problem.h"

namespace {

using TestExchange = HarnessExchange;
using TestReply = mado::Reply<TestExchange>;

const char* const* type_header() {
  static const char* const kNames[] = {"Content-Type", nullptr};
  return &kNames[0];
}

const char* const* origin_header() {
  static const char* const kNames[] = {"Origin", nullptr};
  return &kNames[0];
}

const char* const* board_origins() {
  static const char* const kOrigins[] = {"http://board.local", "http://192.168.1.177", nullptr};
  return &kOrigins[0];
}

std::string detail_text(const mado::Problem& problem) {
  return problem.detail == nullptr ? "<none>" : reinterpret_cast<const char*>(problem.detail);
}

TEST(SendProblem, WritesAProblemDocumentWithTitleStatusAndDetail) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  const mado::Problem problem{
      mado::http::kUnprocessableContent,
      F("missing required property: on"),
  };

  // Act
  const bool sent = mado::send_problem(exchange, problem);

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 422 Unprocessable Content\r\nConnection: close\r\n"
            "Content-Type: application/problem+json\r\nX-Content-Type-Options: nosniff\r\n"
            "Cache-Control: no-store\r\n\r\n"
            R"({"title":"Unprocessable Content","status":422,)"
            R"("detail":"missing required property: on"})");
}

TEST(SendProblem, LeavesOutAMissingDetail) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_problem(exchange, mado::Problem{mado::http::kNotFound, nullptr});
  const std::string output = harness.sent();

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(output.substr(output.find("\r\n\r\n") + 4U), R"({"title":"Not Found","status":404})");
}

TEST(SendProblem, RefusesASuccessStatus) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_problem(exchange, mado::Problem{mado::http::kOk, nullptr});

  // Assert
  EXPECT_FALSE(sent);
  EXPECT_EQ(harness.sent(), "");
}

TEST(SendProblem, RefusesASecondStatus) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  const bool first = exchange.send_empty(mado::http::kNoContent);

  // Act
  const bool sent = mado::send_problem(exchange, mado::Problem{mado::http::kNotFound, nullptr});

  // Assert
  EXPECT_TRUE(first);
  EXPECT_FALSE(sent);
  EXPECT_EQ(harness.sent().find("404"), std::string::npos);
}

TEST(SendProblem, ReportsAFailedOutput) {
  // Arrange
  ExchangeHarness harness;
  harness.detach_output();
  TestExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_problem(exchange, mado::Problem{mado::http::kNotFound, F("gone")});

  // Assert
  EXPECT_FALSE(sent);
}

struct JsonProblemCase {
  mado::JsonStatus status;
  uint16_t code;
  const char* detail;
};

std::ostream& operator<<(std::ostream& stream, const JsonProblemCase& sample) {
  return stream << sample.detail;
}

class JsonProblemTest : public ::testing::TestWithParam<JsonProblemCase> {};

TEST_P(JsonProblemTest, NamesWhatWasWrongWithTheBody) {
  // Arrange
  const JsonProblemCase sample = GetParam();

  // Act
  const mado::Problem problem = mado::json_problem(sample.status);

  // Assert
  EXPECT_EQ(problem.status, sample.code);
  EXPECT_EQ(detail_text(problem), sample.detail);
}

INSTANTIATE_TEST_SUITE_P(
    Statuses, JsonProblemTest,
    ::testing::Values(
        JsonProblemCase{mado::JsonStatus::kSyntax, 400U, "the body is not valid JSON"},
        JsonProblemCase{mado::JsonStatus::kTruncated, 400U, "the body is not valid JSON"},
        JsonProblemCase{mado::JsonStatus::kTooLong, 422U, "a string is longer than its maxLength"},
        JsonProblemCase{mado::JsonStatus::kTooShort, 422U,
                        "a string is shorter than its minLength"},
        JsonProblemCase{mado::JsonStatus::kOutOfRange, 422U, "a number is outside its range"},
        JsonProblemCase{mado::JsonStatus::kWrongType, 422U, "a property has the wrong type"},
        JsonProblemCase{mado::JsonStatus::kUnsupported, 422U,
                        "a property is unknown, read-only or repeated"},
        JsonProblemCase{mado::JsonStatus::kOk, 500U, "<none>"},
        JsonProblemCase{mado::JsonStatus::kEnd, 500U, "<none>"}));

TEST(JsonBodyProblem, AcceptsAJsonBodyWithinTheLimit) {
  // Arrange
  ExchangeHarness harness("{}", 2U, mado::Method::kPost);
  harness.capture(type_header(), 0U, "application/json; charset=utf-8");
  const TestExchange exchange = harness.make();

  // Act
  const mado::Problem problem = mado::json_body_problem(exchange, 2U);

  // Assert
  EXPECT_EQ(problem.status, mado::http::kNoResponse);
  EXPECT_EQ(problem.detail, nullptr);
}

TEST(JsonBodyProblem, RefusesAnotherMediaType) {
  // Arrange
  ExchangeHarness harness("a=1", 3U, mado::Method::kPost);
  harness.capture(type_header(), 0U, "application/x-www-form-urlencoded");
  const TestExchange exchange = harness.make();

  // Act
  const mado::Problem problem = mado::json_body_problem(exchange, 64U);

  // Assert
  EXPECT_EQ(problem.status, mado::http::kUnsupportedMediaType);
  EXPECT_EQ(detail_text(problem), "Content-Type must be application/json");
}

TEST(JsonBodyProblem, RefusesAnEmptyBody) {
  // Arrange
  ExchangeHarness harness("", 0U, mado::Method::kPost);
  harness.capture(type_header(), 0U, "application/json");
  const TestExchange exchange = harness.make();

  // Act
  const mado::Problem problem = mado::json_body_problem(exchange, 64U);

  // Assert
  EXPECT_EQ(problem.status, mado::http::kBadRequest);
  EXPECT_EQ(detail_text(problem), "a JSON body is required");
}

TEST(JsonBodyProblem, RefusesABodyOverTheLimit) {
  // Arrange
  FakeData data(20261031U);
  const uint32_t limit = data.number(1U, 512U);
  ExchangeHarness harness("", limit + 1U, mado::Method::kPost);
  harness.capture(type_header(), 0U, "application/json");
  const TestExchange exchange = harness.make();

  // Act
  const mado::Problem problem = mado::json_body_problem(exchange, limit);

  // Assert
  EXPECT_EQ(problem.status, mado::http::kContentTooLarge);
  EXPECT_EQ(detail_text(problem), "the body is larger than this operation accepts");
}

TEST(JsonBodyProblem, ReportsAnAnsweredExchange) {
  // Arrange
  ExchangeHarness harness("{}", 2U, mado::Method::kPost);
  TestExchange exchange = harness.make();
  const bool answered = exchange.send_empty(mado::http::kNoContent);

  // Act
  const mado::Problem problem = mado::json_body_problem(exchange, 64U);

  // Assert
  EXPECT_TRUE(answered);
  EXPECT_EQ(problem.status, mado::http::kInternalServerError);
}

TEST(Reply, ChainsAStatusTypeHeadersAndLocation) {
  // Arrange
  FakeData data(20261101U);
  const uint32_t item_id = data.number(1U, 254U);
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  TestReply reply(exchange);

  // Act
  const bool ended = reply.status(mado::http::kCreated)
                         .json()
                         .no_store()
                         .location(F("/api/items/"), item_id)
                         .header(F("X-Count"), 2U)
                         .header(F("X-Name"), "lamp")
                         .type(F("text/plain"))
                         .length(0U)
                         .end();

  // Assert
  EXPECT_TRUE(ended);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 201 Created\r\nConnection: close\r\n"
            "Content-Type: application/json\r\nX-Content-Type-Options: nosniff\r\n"
            "Cache-Control: no-store\r\nLocation: /api/items/" +
                std::to_string(item_id) +
                "\r\nX-Count: 2\r\nX-Name: lamp\r\nContent-Type: text/plain\r\n"
                "Content-Length: 0\r\n\r\n");
}

TEST(Reply, StopsAtTheFirstFailure) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  TestReply reply(exchange);

  // Act
  const bool ended = reply.status(42U).json().no_store().end();

  // Assert
  EXPECT_FALSE(ended);
  EXPECT_EQ(harness.sent(), "");
}

TEST(Reply, RefusesAnExchangeThatAlreadyAnswered) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  const bool first = exchange.send_empty(mado::http::kNoContent);
  TestReply reply(exchange);

  // Act
  const bool ended = reply.status(mado::http::kOk).end();

  // Assert
  EXPECT_TRUE(first);
  EXPECT_FALSE(ended);
  EXPECT_EQ(harness.sent().find("200 OK"), std::string::npos);
}

TEST(Reply, RefusesHeadersBeforeAStatus) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  TestReply reply(exchange);

  // Act
  const bool ended = reply.header(F("X-Name"), mado::FlashText(F("lamp"))).end();

  // Assert
  EXPECT_FALSE(ended);
  EXPECT_EQ(harness.sent(), "");
}

TEST(SendCorsHeaders, EchoesAListedOrigin) {
  // Arrange
  ExchangeHarness harness;
  harness.capture(origin_header(), 0U, "http://board.local");
  TestExchange exchange = harness.make();
  const bool status = exchange.send_status(mado::http::kNoContent);

  // Act
  const bool sent = mado::send_cors_headers(exchange, board_origins());

  // Assert
  EXPECT_TRUE(status);
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 204 No Content\r\nConnection: close\r\nVary: Origin\r\n"
            "Access-Control-Allow-Origin: http://board.local\r\n"
            "Access-Control-Allow-Methods: GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
            "Access-Control-Max-Age: 600\r\n");
}

TEST(SendCorsHeaders, SendsOnlyVaryForAnUnlistedOrigin) {
  // Arrange
  FakeData data(20261102U);
  ExchangeHarness harness;
  harness.capture(origin_header(), 0U, "http://" + data.word(10) + ".example");
  TestExchange exchange = harness.make();
  const bool status = exchange.send_status(mado::http::kNoContent);

  // Act
  const bool sent = mado::send_cors_headers(exchange, board_origins());

  // Assert
  EXPECT_TRUE(status);
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(), "HTTP/1.1 204 No Content\r\nConnection: close\r\nVary: Origin\r\n");
}

TEST(SendCorsHeaders, SendsOnlyVaryWithoutAnOrigin) {
  // Arrange
  ExchangeHarness harness;
  harness.capture(origin_header(), 0U, "");
  TestExchange exchange = harness.make();
  const bool status = exchange.send_status(mado::http::kNoContent);

  // Act
  const bool sent = mado::send_cors_headers(exchange, board_origins());

  // Assert
  EXPECT_TRUE(status);
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent().find("Allow-Origin"), std::string::npos);
}

TEST(SendCorsHeaders, RefusesBeforeAStatus) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_cors_headers(exchange, board_origins());

  // Assert
  EXPECT_FALSE(sent);
  EXPECT_EQ(harness.sent(), "");
}

TEST(ExchangeStatus, RemembersTheStatusItSent) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();
  const uint16_t before = exchange.status_code();

  // Act
  const bool sent = exchange.send_empty(mado::http::kCreated);

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(before, mado::http::kNoResponse);
  EXPECT_EQ(exchange.status_code(), mado::http::kCreated);
}

TEST(ExchangeLocation, RefusesALocationBeforeAStatus) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();

  // Act
  const bool sent = exchange.send_location(F("/api/items/"), 1U);

  // Assert
  EXPECT_FALSE(sent);
  EXPECT_EQ(harness.sent(), "");
}

TEST(ExchangeReason, PrintsNothingForAnInvalidStatus) {
  // Arrange
  ExchangeHarness harness;
  TestExchange exchange = harness.make();

  // Act
  const size_t written = exchange.print_reason(42U);

  // Assert
  EXPECT_EQ(written, 0U);
}

TEST(ExchangeDrain, DiscardsWhatTheHandlerLeft) {
  // Arrange
  FakeData data(20261103U);
  const std::string body = data.word(30);
  ExchangeHarness harness(body, static_cast<uint32_t>(body.size()), mado::Method::kPost);
  TestExchange exchange = harness.make();

  // Act
  const bool drained = exchange.discard_body();

  // Assert
  EXPECT_TRUE(drained);
  EXPECT_EQ(exchange.remaining_body(), 0U);
  EXPECT_EQ(harness.client().unread(), 0U);
}

TEST(ExchangeDrain, StopsWhenTheClientSendsLessThanItDeclared) {
  // Arrange
  ExchangeHarness harness("abc", 10U, mado::Method::kPost);
  harness.client().close_when_drained();
  TestExchange exchange = harness.make();

  // Act
  const bool drained = exchange.discard_body();

  // Assert
  EXPECT_FALSE(drained);
  EXPECT_EQ(exchange.remaining_body(), 7U);
}

}
