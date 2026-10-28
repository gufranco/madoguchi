#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>

#include <string>

#include "../../examples/RestPost/PostApp.h"
#include "../../examples/RestPost/generated_api.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "FakeSd.h"

namespace {

constexpr const char* kSpecText = R"({"openapi":"3.1.0"})";

using App = PostApp<FakeClient, FakeSd, FILE_READ>;
using AppServer = mado::Server<FakeClient, App, rest_post::Limits>;

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

class RestPostTest : public ::testing::Test {
 public:
  std::string post(const std::string& type, const std::string& body) {
    FakeClient client("POST /api/items HTTP/1.1\r\nHost: mado.test\r\nContent-Type: " + type +
                      "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body);
    server_.process(client);
    return client.output();
  }

  FakeData& data() {
    return data_;
  }

 protected:
  void SetUp() override {
    fake_clock::set(0U);
    card_.files()["/openapi.json"] = kSpecText;
    App::begin(card_, 4U);
  }

 private:
  FakeSd card_;
  AppServer server_{&rest_post::kRoutes[0], &rest_post::kHeaderNames[0]};
  FakeData data_{20261027U};
};

TEST_F(RestPostTest, CreatesAnItemAndPointsAtIt) {
  // Arrange
  const std::string name = data().word(9U);

  // Act
  const std::string response = post("application/json", R"({"name":")" + name + R"(","on":true})");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 201 Created");
  EXPECT_NE(response.find("Location: /api/items/1\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), R"({"id":1,"name":")" + name + R"(","on":true})");
}

TEST_F(RestPostTest, NumbersItemsInOrder) {
  // Arrange
  static_cast<void>(post("application/json", R"({"name":"a","on":true})"));

  // Act
  const std::string response = post("application/json", R"({"name":"b","on":false})");

  // Assert
  EXPECT_NE(response.find("Location: /api/items/2\r\n"), std::string::npos);
}

TEST_F(RestPostTest, ReportsAFullTable) {
  // Arrange
  for (int slot = 0; slot < 4; ++slot) {
    static_cast<void>(post("application/json", R"({"name":"x","on":true})"));
  }

  // Act
  const std::string response = post("application/json", R"({"name":"y","on":true})");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 507 Insufficient Storage");
}

TEST_F(RestPostTest, ExplainsAMissingRequiredProperty) {
  // Arrange
  const std::string body = R"({"name":")" + data().word(5U) + R"("})";

  // Act
  const std::string response = post("application/json", body);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
  EXPECT_NE(response.find("Content-Type: application/problem+json\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), R"({"title":"Unprocessable Content","status":422,)"
                               R"("detail":"missing required property: on"})");
}

TEST_F(RestPostTest, ExplainsAWrongMediaType) {
  // Arrange
  const std::string body = R"({"name":"a","on":true})";

  // Act
  const std::string response = post("text/plain", body);

  // Assert
  EXPECT_EQ(body_of(response), R"({"title":"Unsupported Media Type","status":415,)"
                               R"("detail":"Content-Type must be application/json"})");
}

struct PostCase {
  std::string type;
  std::string body;
  const char* expected = nullptr;
};

class RestPostRejectTest : public RestPostTest, public ::testing::WithParamInterface<PostCase> {};

TEST_P(RestPostRejectTest, AnswersTheRightStatus) {
  // Arrange
  const PostCase& sample = GetParam();

  // Act
  const std::string response = post(sample.type, sample.body);

  // Assert
  EXPECT_EQ(status_line(response), sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Bodies, RestPostRejectTest,
    ::testing::Values(
        PostCase{"text/plain", R"({"name":"a","on":true})", "HTTP/1.1 415 Unsupported Media Type"},
        PostCase{"application/json", "", "HTTP/1.1 400 Bad Request"},
        PostCase{"application/json", R"({"name":"a",)", "HTTP/1.1 400 Bad Request"},
        PostCase{"application/json", R"({"name":"a"})", "HTTP/1.1 422 Unprocessable Content"},
        PostCase{"application/json", R"({"name":"a","on":1})",
                 "HTTP/1.1 422 Unprocessable Content"},
        PostCase{"application/json", R"({"id":3,"name":"a","on":true})",
                 "HTTP/1.1 422 Unprocessable Content"},
        PostCase{"application/json", R"({"name":"aaaaaaaaaaaaaaaaa","on":true})",
                 "HTTP/1.1 422 Unprocessable Content"},
        PostCase{"application/json", std::string(300U, ' ') + "{}",
                 "HTTP/1.1 413 Content Too Large"}));

}
