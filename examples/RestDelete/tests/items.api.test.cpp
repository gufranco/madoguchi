#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>

#include "../src/http/api.generated.h"
#include "../src/http/api.router.h"
#include "../src/identity/adapters/card_failure.store.h"
#include "../src/identity/adapters/flash_secrets.authority.h"
#include "../src/identity/application/access.service.h"
#include "../src/items/adapters/ram_item.repository.h"
#include "../src/items/application/item_catalog.service.h"
#include "../src/items/domain/item.entity.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeSd.h"
#include "TestAccess.h"
#include "TestSecrets.h"

namespace {

constexpr const char* kSpecText = R"({"openapi":"3.1.0"})";

using Catalog = items::ItemCatalog<items::RamItemRepository>;
using Authority = identity::FlashSecretsAuthority<test_secrets::TestSecrets>;
using Failures = identity::CardFailureLog<FakeSd, FILE_READ, FILE_WRITE>;
using Access = identity::AccessService<Authority, Failures>;
using Gatekeeper = TestAccess<Authority, Failures, Access>;
using App = api::Router<FakeClient, Access, items::RamItemRepository, FakeSd, FILE_READ>;
using AppServer = mado::Server<FakeClient, App, rest_delete::Limits>;

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

std::string login_body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

class RestDeleteTest : public ::testing::Test {
 public:
  std::string raw(const std::string& wire) {
    FakeClient client(wire);
    server_.process(client);
    return client.output();
  }

  std::string send(const std::string& line) {
    FakeClient client(line + "\r\nHost: mado.test\r\n" + bearer() + "\r\n");
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
    Gatekeeper& keeper = fresh_gatekeeper<Gatekeeper>();
    App::attach_access(keeper.access, keeper.clock);
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

TEST_F(RestDeleteTest, RefusesAnApiRequestWithoutAToken) {
  // Arrange
  const std::string wire = "DELETE /api/items/1 HTTP/1.1\r\nHost: mado.test\r\n\r\n";

  // Act
  const std::string response = raw(wire);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
  EXPECT_NE(response.find("WWW-Authenticate: Bearer"), std::string::npos);
}

TEST_F(RestDeleteTest, IssuesATokenForTheRightPassword) {
  // Arrange
  const std::string body =
      std::string(R"({"username":"admin","password":")") + &test_secrets::kPassword[0] + R"("})";

  // Act
  const std::string response =
      raw("POST /api/auth/login HTTP/1.1\r\nHost: mado.test\r\nContent-Type: application/json\r\n"
          "Content-Length: " +
          std::to_string(body.size()) + "\r\n\r\n" + body);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(login_body_of(response).find(R"("token_type":"Bearer")"), std::string::npos);
}

}
