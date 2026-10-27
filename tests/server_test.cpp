#include <Madoguchi.h>
#include <gtest/gtest.h>

#include <string>

#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "TestApp.h"

namespace {

using TestServer = mado::Server<FakeClient, TestApp>;

class ServerTest : public ::testing::Test {
 public:
  TestServer& server() {
    return server_;
  }

  FakeData& data() {
    return data_;
  }

 protected:
  void SetUp() override {
    fake_clock::set(0U);
    TestApp::calls.clear();
    TestApp::last_path.clear();
    TestApp::last_query.clear();
    TestApp::last_header.clear();
    TestApp::last_body.clear();
  }

 private:
  TestServer server_{test_routes(), test_header_names()};
  FakeData data_{20261026U};
};

std::string request(const std::string& line, const std::string& headers = "") {
  return line + "\r\nHost: mado.test\r\n" + headers + "\r\n";
}

TEST_F(ServerTest, ServesIndexWithStatusContentTypeAndBody) {
  // Arrange
  FakeClient client(request("GET / HTTP/1.1"));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(
      client.output(),
      "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\n\r\n<h1>Hello</h1>");
  EXPECT_TRUE(client.stopped());
}

TEST_F(ServerTest, AnswersNotFoundForUnknownPath) {
  // Arrange
  FakeClient client(request("GET /" + data().word(8) + " HTTP/1.1"));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output(),
            "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
  EXPECT_TRUE(TestApp::calls.empty());
}

TEST_F(ServerTest, AnswersMethodNotAllowedWhenOnlyThePathMatches) {
  // Arrange
  FakeClient client(request("DELETE /echo HTTP/1.1"));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output(),
            "HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\nAllow: GET, HEAD\r\n"
            "Content-Length: 0\r\n\r\n");
}

TEST_F(ServerTest, HeadReturnsHeadersWithoutBody) {
  // Arrange
  FakeClient client(request("HEAD / HTTP/1.1"));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output(),
            "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\n\r\n");
}

TEST_F(ServerTest, RoutesIgnoreTheQueryStringAndDecodeParameters) {
  // Arrange
  FakeClient client(request("GET /echo?_=1730000000&name=a%2Fb+c HTTP/1.1"));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(TestApp::last_path, "/echo");
  EXPECT_EQ(TestApp::last_query, "_=1730000000&name=a%2Fb+c");
  EXPECT_NE(client.output().find("\r\n\r\na/b c"), std::string::npos);
}

TEST_F(ServerTest, CapturesRequestedHeadersCaseInsensitively) {
  // Arrange
  const std::string trace = data().word(12);
  FakeClient client(request("GET /echo HTTP/1.1", "x-tRACE:   " + trace + "\r\n"));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(TestApp::last_header, trace);
}

TEST_F(ServerTest, LeavesHeadersThatWereNotRequestedUncaptured) {
  // Arrange
  FakeClient client(request("GET /echo HTTP/1.1", "X-Other: " + data().word(6) + "\r\n"));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(TestApp::last_header, "<none>");
}

TEST_F(ServerTest, RejectsARequestLineLongerThanTheBuffer) {
  // Arrange
  FakeClient client(request("GET /" + data().word(200) + " HTTP/1.1"));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output(),
            "HTTP/1.1 414 URI Too Long\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
}

TEST_F(ServerTest, TimesOutAStalledRequestLine) {
  // Arrange
  FakeClient client("GET /");

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output(),
            "HTTP/1.1 408 Request Timeout\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
  EXPECT_GE(fake_clock::now(), 1000U);
}

TEST_F(ServerTest, IgnoresADisconnectedClient) {
  // Arrange
  FakeClient client(request("GET / HTTP/1.1"));
  client.disconnect();

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output(), "");
  EXPECT_TRUE(TestApp::calls.empty());
}

TEST_F(ServerTest, RejectsAMalformedHeaderLine) {
  // Arrange
  FakeClient client(request("GET / HTTP/1.1", "Broken header line\r\n"));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output(),
            "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
}

TEST_F(ServerTest, RejectsMoreHeaderLinesThanTheLimit) {
  // Arrange
  std::string headers;
  for (int i = 0; i < 60; ++i) {
    headers += "X-" + data().word(4) + ": " + data().word(4) + "\r\n";
  }
  FakeClient client(request("GET / HTTP/1.1", headers));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output(),
            "HTTP/1.1 431 Request Header Fields Too Large\r\nConnection: close\r\nContent-Length: "
            "0\r\n\r\n");
}

TEST_F(ServerTest, AnswersNotImplementedForAnUnknownMethod) {
  // Arrange
  FakeClient client(request("BREW / HTTP/1.1"));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output(),
            "HTTP/1.1 501 Not Implemented\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
}

TEST_F(ServerTest, RejectsATargetThatIsNotAnAbsolutePath) {
  // Arrange
  FakeClient client(request("GET " + data().word(5) + " HTTP/1.1"));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output(),
            "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
}

TEST_F(ServerTest, SendsContinueBeforeReadingAnExpectingBody) {
  // Arrange
  const std::string body = data().word(5);
  FakeClient client(
      request("PUT /upload/a.txt HTTP/1.1", "Expect: 100-continue\r\nContent-Length: 5\r\n") +
      body);

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output().rfind("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 201 Created\r\n", 0), 0U);
  EXPECT_EQ(TestApp::last_body, body);
}

TEST_F(ServerTest, ReportsAShortBodyAfterTheTimeout) {
  // Arrange
  FakeClient client(request("PUT /upload/a.txt HTTP/1.1", "Content-Length: 100\r\n") +
                    data().word(10));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output().rfind("HTTP/1.1 400 Bad Request\r\n", 0), 0U);
  EXPECT_EQ(TestApp::last_body.size(), 10U);
}

TEST_F(ServerTest, ReadsTheBodyInBlocksNotBytes) {
  // Arrange
  const std::string body = data().word(4096);
  const std::string head = request("PUT /upload/big.bin HTTP/1.1", "Content-Length: 4096\r\n");
  FakeClient client(head + body);
  client.limit_available(64U);

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(TestApp::last_body, body);
  EXPECT_EQ(client.block_reads(), 256U);
  EXPECT_EQ(client.single_byte_reads(), head.size());
}

TEST_F(ServerTest, KeepsTheConnectionOpenWhenTheHandlerAsks) {
  // Arrange
  FakeClient client(request("GET /stream HTTP/1.1"));

  // Act
  server().process(client);

  // Assert
  EXPECT_FALSE(client.stopped());
  EXPECT_EQ(client.output(), "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n");
}

TEST_F(ServerTest, AnswersAPreflightWithCorsHeaders) {
  // Arrange
  FakeClient client(request("OPTIONS /api/led HTTP/1.1"));

  // Act
  server().process(client);

  // Assert
  EXPECT_EQ(client.output(),
            "HTTP/1.1 204 No Content\r\nConnection: close\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization\r\n\r\n");
}

TEST_F(ServerTest, PassesWildcardPathsThroughToTheHandler) {
  // Arrange
  const std::string path = "/files/" + data().word(6) + "/" + data().word(6);
  FakeClient client(request("GET " + path + " HTTP/1.1"));

  // Act
  server().process(client);

  // Assert
  EXPECT_NE(client.output().find("\r\n\r\n" + path), std::string::npos);
}

TEST_F(ServerTest, KeepsEveryBarrierIntactAcrossRequests) {
  // Arrange
  FakeClient first(request("GET /" + data().word(100) + " HTTP/1.1"));
  FakeClient second(request("GET /echo HTTP/1.1", "X-Trace: " + data().word(60) + "\r\n"));
  server().process(first);

  // Act
  server().process(second);

  // Assert
  EXPECT_EQ(server().barrier_faults(), 0U);
}

}
