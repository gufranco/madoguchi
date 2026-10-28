#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>

#include "../src/http/api.generated.h"
#include "../src/http/api.router.h"
#include "../src/items/adapters/ram_item.repository.h"
#include "../src/items/application/item_catalog.service.h"
#include "../src/items/domain/item.entity.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeSd.h"

namespace {

constexpr const char* kSpecText = R"({"openapi":"3.1.0"})";

using Catalog = items::ItemCatalog<items::RamItemRepository>;
using App = api::Router<FakeClient, items::RamItemRepository, FakeSd, FILE_READ>;
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

  uint8_t remaining() const {
    uint8_t count = 0U;
    for (uint8_t item_id = 1U; item_id <= items::kCapacity; ++item_id) {
      count = static_cast<uint8_t>(count + (Catalog::find(repository_, item_id).found ? 1U : 0U));
    }
    return count;
  }

 protected:
  void SetUp() override {
    fake_clock::set(0U);
    card_.files()["/openapi.json"] = kSpecText;
    static_cast<void>(Catalog::add(repository_, items::ItemDraft{"lamp", true}));
    static_cast<void>(Catalog::add(repository_, items::ItemDraft{"fan", false}));
    App::attach(repository_, &card_);
  }

 private:
  FakeSd card_;
  items::RamItemRepository repository_;
  AppServer server_{&rest_delete::kRoutes[0], &rest_delete::kHeaderNames[0]};
};

TEST_F(RestDeleteTest, DeletesAnItemWithNoBody) {
  // Arrange
  const std::string line = "DELETE /api/items/1 HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(response, "HTTP/1.1 204 No Content\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
  EXPECT_EQ(remaining(), 1U);
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
  EXPECT_EQ(remaining(), 2U);
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
