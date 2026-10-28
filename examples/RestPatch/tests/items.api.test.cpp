#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>

#include <string>

#include "../src/http/api.generated.h"
#include "../src/http/api.router.h"
#include "../src/items/adapters/ram_item.repository.h"
#include "../src/items/application/item_catalog.service.h"
#include "../src/items/domain/item.entity.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "FakeSd.h"

namespace {

constexpr const char* kSpecText = R"({"openapi":"3.1.0"})";

using Catalog = items::ItemCatalog<items::RamItemRepository>;
using App = api::Router<FakeClient, items::RamItemRepository, FakeSd, FILE_READ>;
using AppServer = mado::Server<FakeClient, App, rest_patch::Limits>;

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

struct ExchangePatch {
  std::string line;
  std::string type;
  std::string body;
};

class PatchHarness {
 public:
  PatchHarness() {
    fake_clock::set(0U);
    card_.files()["/openapi.json"] = kSpecText;
    static_cast<void>(Catalog::add(repository_, items::ItemDraft{"lamp", true}));
    static_cast<void>(Catalog::add(repository_, items::ItemDraft{"fan", false}));
    App::attach(repository_, &card_);
  }

  std::string send(const ExchangePatch& request) {
    FakeClient client(request.line + "\r\nHost: mado.test\r\nContent-Type: " + request.type +
                      "\r\nContent-Length: " + std::to_string(request.body.size()) + "\r\n\r\n" +
                      request.body);
    server_.process(client);
    return client.output();
  }

 private:
  FakeSd card_;
  items::RamItemRepository repository_;
  AppServer server_{&rest_patch::kRoutes[0], &rest_patch::kHeaderNames[0]};
};

class RestPatchTest : public ::testing::Test {
 public:
  PatchHarness& harness() {
    return harness_;
  }

  FakeData& data() {
    return data_;
  }

 private:
  PatchHarness harness_;
  FakeData data_{20261029U};
};

TEST_F(RestPatchTest, ChangesOnlyTheGivenField) {
  // Arrange
  const std::string body = R"({"on":false})";

  // Act
  const std::string response =
      harness().send(ExchangePatch{"PATCH /api/items/1 HTTP/1.1", "application/json", body});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), R"({"id":1,"name":"lamp","on":false})");
}

TEST_F(RestPatchTest, RenamesAnItem) {
  // Arrange
  const std::string name = data().word(6U);

  // Act
  const std::string response = harness().send(ExchangePatch{
      "PATCH /api/items/2 HTTP/1.1",
      "application/json",
      R"({"name":")" + name + R"("})",
  });

  // Assert
  EXPECT_EQ(body_of(response), R"({"id":2,"name":")" + name + R"(","on":false})");
}

class RestPatchRejectTest : public RestPatchTest,
                            public ::testing::WithParamInterface<const char*> {};

TEST_P(RestPatchRejectTest, RefusesTheBody) {
  // Arrange
  const std::string body = GetParam();

  // Act
  const std::string response =
      harness().send(ExchangePatch{"PATCH /api/items/1 HTTP/1.1", "application/json", body});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
}

INSTANTIATE_TEST_SUITE_P(Bodies, RestPatchRejectTest,
                         ::testing::Values("{}", R"({"id":5})", R"({"on":"yes"})",
                                           R"({"color":"red"})"));

TEST_F(RestPatchTest, AnswersBadRequestForBrokenJson) {
  // Arrange
  const std::string body = R"({"on":)";

  // Act
  const std::string response =
      harness().send(ExchangePatch{"PATCH /api/items/1 HTTP/1.1", "application/json", body});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 400 Bad Request");
}

TEST_F(RestPatchTest, AnswersNotFoundForAnUnknownId) {
  // Arrange
  const std::string body = R"({"on":true})";

  // Act
  const std::string response =
      harness().send(ExchangePatch{"PATCH /api/items/9 HTTP/1.1", "application/json", body});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 404 Not Found");
}

TEST_F(RestPatchTest, ExplainsAPatchThatChangesNothing) {
  // Arrange
  const std::string body = "{}";

  // Act
  const std::string response =
      harness().send(ExchangePatch{"PATCH /api/items/1 HTTP/1.1", "application/json", body});

  // Assert
  EXPECT_NE(response.find("Content-Type: application/problem+json\r\n"), std::string::npos);
  EXPECT_EQ(
      body_of(response),
      R"({"title":"Unprocessable Content","status":422,"detail":"the body changes no property"})");
}

TEST_F(RestPatchTest, ExplainsAnUnknownId) {
  // Arrange
  const std::string body = R"({"on":true})";

  // Act
  const std::string response =
      harness().send(ExchangePatch{"PATCH /api/items/9 HTTP/1.1", "application/json", body});

  // Assert
  EXPECT_EQ(body_of(response),
            R"({"title":"Not Found","status":404,"detail":"no item has this id"})");
}

}
