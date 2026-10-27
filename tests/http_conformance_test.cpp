#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "TestApp.h"
#include "mado/DefaultLimits.h"
#include "mado/RequestLine.h"
#include "mado/ResponseSummary.h"

namespace {

struct ConformanceCase {
  const char* request;
  const char* status_line;
};

std::ostream& operator<<(std::ostream& stream, const ConformanceCase& sample) {
  return stream << sample.request;
}

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

std::string serve(const std::string& request) {
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp> server(test_routes(), test_header_names());
  FakeClient client(request);
  server.process(client);
  return client.output();
}

class ConformanceTest : public ::testing::TestWithParam<ConformanceCase> {};

TEST_P(ConformanceTest, AnswersEachMessageFramingCaseAsRfc9112Requires) {
  // Arrange
  const ConformanceCase sample = GetParam();

  // Act
  const std::string response = serve(sample.request);

  // Assert
  EXPECT_EQ(status_line(response), sample.status_line);
}

INSTANTIATE_TEST_SUITE_P(
    Rfc9112, ConformanceTest,
    ::testing::Values(
        ConformanceCase{"GET / HTTP/1.1\r\nHost: a\r\n\r\n", "HTTP/1.1 200 OK"},
        ConformanceCase{"GET / HTTP/1.1\r\nhost: a\r\n\r\n", "HTTP/1.1 200 OK"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost:\r\n\r\n", "HTTP/1.1 200 OK"},
        ConformanceCase{"GET / HTTP/1.0\r\n\r\n", "HTTP/1.1 200 OK"},
        ConformanceCase{"GET / HTTP/1.9\r\nHost: a\r\n\r\n", "HTTP/1.1 200 OK"},
        ConformanceCase{"GET / HTTP/1.1\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost: a b\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost : a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"PUT /upload/a HTTP/1.1\r\nHost: a\r\nContent-Length: 2\r\n"
                        "Content-Length: 2\r\n\r\nab",
                        "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"PUT /upload/a HTTP/1.1\r\nHost: a\r\nContent-Length: 900\r\n"
                        "Content-Length: 2\r\n\r\nab",
                        "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"PUT /upload/a HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n"
                        "2\r\nab\r\n0\r\n\r\n",
                        "HTTP/1.1 501 Not Implemented"},
        ConformanceCase{"PUT /upload/a HTTP/1.1\r\nHost: a\r\nContent-Length: 2\r\n"
                        "Transfer-Encoding: chunked\r\n\r\nab",
                        "HTTP/1.1 501 Not Implemented"},
        ConformanceCase{"GET / HTTP/2.0\r\nHost: a\r\n\r\n",
                        "HTTP/1.1 505 HTTP Version Not Supported"},
        ConformanceCase{"GET / HTTP/0.9\r\nHost: a\r\n\r\n",
                        "HTTP/1.1 505 HTTP Version Not Supported"},
        ConformanceCase{"GET /\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.12\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.1 x\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTX/1.1\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/x.1\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / XTTP/1.1\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HXTP/1.1\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTXP/1.1\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP-1.1\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/!.1\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1-1\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.!\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.a\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP\r\nHost: a\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET /  HTTP/1.1\r\nHost: a\r\n\r\n", "HTTP/1.1 200 OK"},
        ConformanceCase{"BREW / HTTP/1.1\r\nHost: a\r\n\r\n", "HTTP/1.1 501 Not Implemented"}));

struct BoardHosts {
  static constexpr bool kTrustedHostsOnly = true;

  static const char* const* trusted_hosts() {
    static const char* const kHosts[] = {"board.local", "192.168.1.177", nullptr};
    return &kHosts[0];
  }

  static void on_response(const mado::RequestLine& request, const mado::ResponseSummary& response) {
    static_cast<void>(request);
    static_cast<void>(response);
  }
};

std::string serve_trusted(const std::string& request) {
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp, mado::DefaultLimits, BoardHosts> server(test_routes(),
                                                                            test_header_names());
  FakeClient client(request);
  server.process(client);
  return status_line(client.output());
}

class TrustedHostTest : public ::testing::TestWithParam<ConformanceCase> {};

TEST_P(TrustedHostTest, ServesOnlyTheListedHostsWithAnOptionalPort) {
  // Arrange
  const ConformanceCase sample = GetParam();

  // Act
  const std::string line = serve_trusted(sample.request);

  // Assert
  EXPECT_EQ(line, sample.status_line);
}

INSTANTIATE_TEST_SUITE_P(
    Hosts, TrustedHostTest,
    ::testing::Values(
        ConformanceCase{"GET / HTTP/1.1\r\nHost: board.local\r\n\r\n", "HTTP/1.1 200 OK"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost: BOARD.Local\r\n\r\n", "HTTP/1.1 200 OK"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost:  board.local:8080 \r\n\r\n", "HTTP/1.1 200 OK"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost: board.local:\r\n\r\n", "HTTP/1.1 200 OK"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost: 192.168.1.177:80\r\n\r\n", "HTTP/1.1 200 OK"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost: board.local:8x\r\n\r\n",
                        "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost: board.loca\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost: board.local.evil\r\n\r\n",
                        "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost: board.local x\r\n\r\n",
                        "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.1\r\nHost:\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.0\r\n\r\n", "HTTP/1.1 400 Bad Request"},
        ConformanceCase{"GET / HTTP/1.0\r\nHost: board.local\r\n\r\n", "HTTP/1.1 200 OK"}));

TEST(TrustedHosts, RejectsAnUnlistedHostDrawnAtRandom) {
  // Arrange
  FakeData data(20261027U);
  const std::string host = data.word(12) + ".example";

  // Act
  const std::string line = serve_trusted("GET / HTTP/1.1\r\nHost: " + host + "\r\n\r\n");

  // Assert
  EXPECT_EQ(line, "HTTP/1.1 400 Bad Request");
}

TEST(TrustedHosts, RejectsAHostLongerThanTheMatcherTracks) {
  // Arrange
  FakeData data(20261028U);
  const std::string host = "board.local" + data.word(300);

  // Act
  const std::string line = serve_trusted("GET / HTTP/1.1\r\nHost: " + host + "\r\n\r\n");

  // Assert
  EXPECT_EQ(line, "HTTP/1.1 400 Bad Request");
}

struct RecordedResponse {
  std::string path;
  uint16_t status = 0U;
};

bool operator==(const RecordedResponse& left, const RecordedResponse& right) {
  return (left.path == right.path) && (left.status == right.status);
}

std::ostream& operator<<(std::ostream& stream, const RecordedResponse& response) {
  return stream << response.path << " " << response.status;
}

using Responses = std::vector<RecordedResponse>;

std::vector<RecordedResponse>& recorded() {
  static std::vector<RecordedResponse> responses;
  return responses;
}

struct RecordingPolicy {
  static constexpr bool kTrustedHostsOnly = false;

  static void on_response(const mado::RequestLine& request, const mado::ResponseSummary& response) {
    recorded().push_back({request.path == nullptr ? "<none>" : request.path, response.status});
  }
};

std::vector<RecordedResponse> serve_recorded(const std::string& request) {
  fake_clock::set(0U);
  recorded().clear();
  mado::Server<FakeClient, TestApp, mado::DefaultLimits, RecordingPolicy> server(
      test_routes(), test_header_names());
  FakeClient client(request);
  server.process(client);
  return recorded();
}

TEST(ResponseHook, ReportsTheStatusAHandlerSent) {
  // Arrange
  const std::string request = "GET / HTTP/1.1\r\nHost: a\r\n\r\n";

  // Act
  const std::vector<RecordedResponse> responses = serve_recorded(request);

  // Assert
  EXPECT_EQ(responses, (Responses{{"/", 200U}}));
}

TEST(ResponseHook, ReportsTheNotFoundTheServerSent) {
  // Arrange
  FakeData data(20261029U);
  const std::string path = "/" + data.word(10);

  // Act
  const std::vector<RecordedResponse> responses =
      serve_recorded("GET " + path + " HTTP/1.1\r\nHost: a\r\n\r\n");

  // Assert
  EXPECT_EQ(responses, (Responses{{path, 404U}}));
}

TEST(ResponseHook, ReportsARejectedRequestWithAnEmptyPath) {
  // Arrange
  const std::string request = "GET /\r\n\r\n";

  // Act
  const std::vector<RecordedResponse> responses = serve_recorded(request);

  // Assert
  EXPECT_EQ(responses, (Responses{{"", 400U}}));
}

TEST(ResponseHook, StaysSilentWhenTheClientHangsUp) {
  // Arrange
  fake_clock::set(0U);
  recorded().clear();
  mado::Server<FakeClient, TestApp, mado::DefaultLimits, RecordingPolicy> server(
      test_routes(), test_header_names());
  FakeClient client("GET / HTTP/1.1\r\n");
  client.close_when_drained();

  // Act
  server.process(client);
  const std::vector<RecordedResponse>& responses = recorded();

  // Assert
  EXPECT_TRUE(responses.empty());
}

TEST(RequestDeadline, TimesOutASenderThatDripsBytesUnderTheReadTimeout) {
  // Arrange
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp> server(test_routes(), test_header_names());
  FakeClient client("GET / HTTP/1.1\r\nHost: a\r\nX-Slow: abcdefghijklmnop\r\n\r\n");
  client.limit_available(1U);
  client.delay_each_read(900U);

  // Act
  server.process(client);

  // Assert
  EXPECT_EQ(status_line(client.output()), "HTTP/1.1 408 Request Timeout");
  EXPECT_GE(fake_clock::now(), 5000U);
  EXPECT_LT(fake_clock::now(), 7000U);
}

TEST(BodyDrain, ReadsAnUnreadBodyBeforeClosing) {
  // Arrange
  FakeData data(20261030U);
  const std::string body = data.word(40);
  const std::string request =
      "DELETE /echo HTTP/1.1\r\nHost: a\r\nContent-Length: " + std::to_string(body.size()) +
      "\r\n\r\n" + body;
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp> server(test_routes(), test_header_names());
  FakeClient client(request);

  // Act
  server.process(client);

  // Assert
  EXPECT_EQ(status_line(client.output()), "HTTP/1.1 405 Method Not Allowed");
  EXPECT_EQ(client.unread(), 0U);
  EXPECT_TRUE(client.stopped());
}

TEST(BodyDrain, LeavesABodyLargerThanTheDrainLimit) {
  // Arrange
  const std::string body(mado::DefaultLimits::kMaxDrainBytes + 1U, 'x');
  const std::string request =
      "DELETE /echo HTTP/1.1\r\nHost: a\r\nContent-Length: " + std::to_string(body.size()) +
      "\r\n\r\n" + body;
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp> server(test_routes(), test_header_names());
  FakeClient client(request);

  // Act
  server.process(client);

  // Assert
  EXPECT_EQ(client.unread(), body.size());
  EXPECT_TRUE(client.stopped());
}

TEST(BodyDrain, WaitsForNoBodyTheClientWasNotAskedToSend) {
  // Arrange
  const std::string request =
      "DELETE /echo HTTP/1.1\r\nHost: a\r\nExpect: 100-continue\r\nContent-Length: 5\r\n\r\n";
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp> server(test_routes(), test_header_names());
  FakeClient client(request);

  // Act
  server.process(client);

  // Assert
  EXPECT_EQ(status_line(client.output()), "HTTP/1.1 405 Method Not Allowed");
  EXPECT_EQ(client.output().find("100 Continue"), std::string::npos);
  EXPECT_LT(fake_clock::now(), 10U);
}

TEST(BodyDrain, KeepsTheBodyOfAConnectionTheHandlerKeepsOpen) {
  // Arrange
  const std::string request = "GET /stream HTTP/1.1\r\nHost: a\r\nContent-Length: 3\r\n\r\nabc";
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp> server(test_routes(), test_header_names());
  FakeClient client(request);

  // Act
  server.process(client);

  // Assert
  EXPECT_EQ(client.unread(), 3U);
  EXPECT_FALSE(client.stopped());
}

TEST(BodyDrain, SkipsTheDrainWhenTheClientRefusesTheResponse) {
  // Arrange
  const std::string request = "DELETE /echo HTTP/1.1\r\nHost: a\r\nContent-Length: 4\r\n\r\nbody";
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp> server(test_routes(), test_header_names());
  FakeClient client(request);
  client.fail_writes_after(0U);

  // Act
  server.process(client);

  // Assert
  EXPECT_EQ(client.unread(), 4U);
  EXPECT_TRUE(client.stopped());
}

TEST(ResponseHook, StaysSilentWhenARejectionCannotBeWritten) {
  // Arrange
  fake_clock::set(0U);
  recorded().clear();
  mado::Server<FakeClient, TestApp, mado::DefaultLimits, RecordingPolicy> server(
      test_routes(), test_header_names());
  FakeClient client("GET /\r\n\r\n");
  client.fail_writes_after(0U);

  // Act
  server.process(client);

  // Assert
  EXPECT_TRUE(recorded().empty());
  EXPECT_TRUE(client.stopped());
}

}
