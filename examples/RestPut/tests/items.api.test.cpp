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
using AppServer = mado::Server<FakeClient, App, rest_put::Limits>;

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

struct ExchangePut {
  std::string line;
  std::string type;
  std::string body;
};

class PutHarness {
 public:
  PutHarness() {
    fake_clock::set(0U);
    card_.files()["/openapi.json"] = kSpecText;
    static_cast<void>(Catalog::add(repository_, items::ItemDraft{"lamp", true}));
    static_cast<void>(Catalog::add(repository_, items::ItemDraft{"fan", false}));
    App::attach(repository_, &card_);
  }

  std::string send(const ExchangePut& request) {
    FakeClient client(request.line + "\r\nHost: mado.test\r\nContent-Type: " + request.type +
                      "\r\nContent-Length: " + std::to_string(request.body.size()) + "\r\n\r\n" +
                      request.body);
    server_.process(client);
    return client.output();
  }

 private:
  FakeSd card_;
  items::RamItemRepository repository_;
  AppServer server_{&rest_put::kRoutes[0], &rest_put::kHeaderNames[0]};
};

class RestPutTest : public ::testing::Test {
 public:
  PutHarness& harness() {
    return harness_;
  }

  FakeData& data() {
    return data_;
  }

 private:
  PutHarness harness_;
  FakeData data_{20261028U};
};

TEST_F(RestPutTest, ReplacesEveryField) {
  // Arrange
  const std::string name = data().word(8U);

  // Act
  const std::string response = harness().send(ExchangePut{
      "PUT /api/items/2 HTTP/1.1",
      "application/json",
      R"({"name":")" + name + R"(","on":true})",
  });

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), R"({"id":2,"name":")" + name + R"(","on":true})");
}

TEST_F(RestPutTest, RefusesAPartialReplacement) {
  // Arrange
  const std::string body = R"({"on":true})";

  // Act
  const std::string response =
      harness().send(ExchangePut{"PUT /api/items/1 HTTP/1.1", "application/json", body});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
}

TEST_F(RestPutTest, AnswersNotFoundForAnUnknownId) {
  // Arrange
  const std::string body = R"({"name":"a","on":true})";

  // Act
  const std::string response =
      harness().send(ExchangePut{"PUT /api/items/3 HTTP/1.1", "application/json", body});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 404 Not Found");
}

TEST_F(RestPutTest, RefusesTextBodies) {
  // Arrange
  const std::string body = R"({"name":"a","on":true})";

  // Act
  const std::string response =
      harness().send(ExchangePut{"PUT /api/items/1 HTTP/1.1", "text/plain", body});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 415 Unsupported Media Type");
}

TEST_F(RestPutTest, ListsTheOnlyAllowedMethod) {
  // Arrange
  const std::string body = R"({"name":"a","on":true})";

  // Act
  const std::string response =
      harness().send(ExchangePut{"POST /api/items/1 HTTP/1.1", "application/json", body});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 405 Method Not Allowed");
  EXPECT_NE(response.find("Allow: GET, HEAD, PUT\r\n"), std::string::npos);
}

}
