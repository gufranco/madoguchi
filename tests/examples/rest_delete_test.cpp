#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>

#include <string>

#include "../../examples/RestDelete/DeleteApp.h"
#include "../../examples/RestDelete/generated_api.h"
#include "FakeClient.h"
#include "FakeClock.h"

namespace {

using App = DeleteApp<FakeClient>;
using AppServer = mado::Server<FakeClient, App, rest_delete::Limits>;

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

class RestDeleteTest : public ::testing::Test {
 public:
  std::string send(const std::string& line) {
    FakeClient client(line + "\r\nHost: mado.test\r\n\r\n");
    server_.process(client);
    return client.output();
  }

 protected:
  void SetUp() override {
    fake_clock::set(0U);
    App::begin();
  }

 private:
  AppServer server_{&rest_delete::kRoutes[0], &rest_delete::kHeaderNames[0]};
};

TEST_F(RestDeleteTest, DeletesAnItemWithNoBody) {
  // Arrange
  const std::string line = "DELETE /api/items/1 HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(response, "HTTP/1.1 204 No Content\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
  EXPECT_EQ(App::state().items.count(), 1U);
}

TEST_F(RestDeleteTest, AnswersNotFoundTheSecondTime) {
  // Arrange
  static_cast<void>(send("DELETE /api/items/2 HTTP/1.1"));

  // Act
  const std::string response = send("DELETE /api/items/2 HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 404 Not Found");
}

TEST_F(RestDeleteTest, AnswersNotFoundForAMalformedId) {
  // Arrange
  const std::string line = "DELETE /api/items/01 HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 404 Not Found");
  EXPECT_EQ(App::state().items.count(), 2U);
}

TEST_F(RestDeleteTest, RefusesToDeleteTheCollection) {
  // Arrange
  const std::string line = "DELETE /api/items HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 405 Method Not Allowed");
}

}
