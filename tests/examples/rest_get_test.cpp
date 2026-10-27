#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>

#include <string>

#include "../../examples/RestGet/GetApp.h"
#include "../../examples/RestGet/generated_api.h"
#include "FakeClient.h"
#include "FakeClock.h"

namespace {

using App = GetApp<FakeClient>;
using AppServer = mado::Server<FakeClient, App, rest_get::Limits>;

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

class RestGetTest : public ::testing::Test {
 public:
  std::string send(const std::string& line) {
    FakeClient client(line + "\r\nHost: mado.test\r\nAccept-Encoding: gzip\r\n\r\n");
    server_.process(client);
    return client.output();
  }

 protected:
  void SetUp() override {
    fake_clock::set(0U);
    App::begin();
  }

 private:
  AppServer server_{&rest_get::kRoutes[0], &rest_get::kHeaderNames[0]};
};

TEST_F(RestGetTest, ListsTheSeededItems) {
  // Arrange
  const std::string line = "GET /api/items HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("Content-Type: application/json\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response),
            R"([{"id":1,"name":"lamp","on":true},{"id":2,"name":"fan","on":false}])");
}

TEST_F(RestGetTest, ReadsOneItem) {
  // Arrange
  const std::string line = "GET /api/items/2 HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), R"({"id":2,"name":"fan","on":false})");
}

TEST_F(RestGetTest, SendsOnlyHeadersForHead) {
  // Arrange
  const std::string line = "HEAD /api/items/1 HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), "");
}

class RestGetMissingTest : public RestGetTest, public ::testing::WithParamInterface<const char*> {};

TEST_P(RestGetMissingTest, AnswersNotFound) {
  // Arrange
  const std::string line = GetParam();

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 404 Not Found");
  EXPECT_NE(response.find(R"("detail":"no item has this id")"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(Paths, RestGetMissingTest,
                         ::testing::Values("GET /api/items/3 HTTP/1.1", "GET /api/items/0 HTTP/1.1",
                                           "GET /api/items/x HTTP/1.1"));

TEST_F(RestGetTest, ListsTheOnlyAllowedMethods) {
  // Arrange
  const std::string line = "POST /api/items HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 405 Method Not Allowed");
  EXPECT_NE(response.find("Allow: GET, HEAD\r\n"), std::string::npos);
}

TEST_F(RestGetTest, ServesItsOwnSpec) {
  // Arrange
  const std::string line = "GET /api/openapi.json HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("Content-Encoding: gzip\r\n"), std::string::npos);
}

}
