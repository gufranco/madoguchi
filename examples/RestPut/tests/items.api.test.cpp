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
#include "FakeData.h"
#include "FakeSd.h"
#include "TestAccess.h"
#include "TestSecrets.h"

namespace {

constexpr const char* kSpecText = R"({"openapi":"3.1.0"})";
constexpr uint32_t kRevisionSeed = 0x00C0FFEEUL;

using Catalog = items::ItemCatalog<items::RamItemRepository>;
using Authority = identity::FlashSecretsAuthority<test_secrets::TestSecrets>;
using Failures = identity::CardFailureLog<FakeSd, FILE_READ, FILE_WRITE>;
using Access = identity::AccessService<Authority, Failures>;
using Gatekeeper = TestAccess<Authority, Failures, Access>;
using App = api::Router<FakeClient, Access, items::RamItemRepository, FakeSd, FILE_READ>;
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

std::string etag_of(const std::string& response) {
  const size_t start = response.find("ETag: ");
  if (start == std::string::npos) {
    return "";
  }
  return response.substr(start + 6U, response.find("\r\n", start) - (start + 6U));
}

std::string item_body(const std::string& name) {
  return R"({"name":")" + name + R"(","on":true})";
}

std::string login_body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

class PutHarness {
 public:
  std::string raw(const std::string& wire) {
    FakeClient client(wire);
    server_.process(client);
    return client.output();
  }

  PutHarness() {
    fake_clock::set(0U);
    repository_.begin(kRevisionSeed);
    card_.files()["/openapi.json"] = kSpecText;
    static_cast<void>(Catalog::add(repository_, items::ItemDraft{"lamp", true}));
    static_cast<void>(Catalog::add(repository_, items::ItemDraft{"fan", false}));
    Gatekeeper& keeper = fresh_gatekeeper<Gatekeeper>();
    App::attach_access(keeper.access, keeper.clock);
    App::attach(repository_, &card_);
  }

  std::string stored_name(const uint8_t item_id) const {
    const items::ItemLookup lookup = Catalog::find(repository_, item_id);
    return std::string(&lookup.item.name[0]);
  }

  std::string send(const ExchangePut& request, const std::string& headers = "") {
    FakeClient client(request.line + "\r\nHost: mado.test\r\n" + bearer() + "" + headers +
                      "Content-Type: " + request.type + "\r\nContent-Length: " +
                      std::to_string(request.body.size()) + "\r\n\r\n" + request.body);
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

TEST_F(RestPutTest, SendsATagThatChangesWithEveryReplacement) {
  // Arrange
  const std::string first =
      harness().send(ExchangePut{"PUT /api/items/1 HTTP/1.1", "application/json", item_body("a")});

  // Act
  const std::string second =
      harness().send(ExchangePut{"PUT /api/items/1 HTTP/1.1", "application/json", item_body("a")});

  // Assert
  EXPECT_EQ(etag_of(first), "\"1-00c0ffef\"");
  EXPECT_EQ(etag_of(second), "\"1-00c0fff0\"");
}

TEST_F(RestPutTest, ReplacesWhenIfMatchNamesTheCurrentTag) {
  // Arrange
  const std::string tag = etag_of(harness().send(
      ExchangePut{"PUT /api/items/2 HTTP/1.1", "application/json", item_body(data().word(5U))}));
  const std::string name = data().word(6U);

  // Act
  const std::string response =
      harness().send(ExchangePut{"PUT /api/items/2 HTTP/1.1", "application/json", item_body(name)},
                     "If-Match: " + tag + "\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(harness().stored_name(2U), name);
}

TEST_F(RestPutTest, RefusesAReplacementBasedOnAStaleTag) {
  // Arrange
  const std::string stale = etag_of(harness().send(
      ExchangePut{"PUT /api/items/1 HTTP/1.1", "application/json", item_body(data().word(5U))}));
  const std::string winner = data().word(7U);
  static_cast<void>(harness().send(
      ExchangePut{"PUT /api/items/1 HTTP/1.1", "application/json", item_body(winner)}));

  // Act
  const std::string response = harness().send(
      ExchangePut{"PUT /api/items/1 HTTP/1.1", "application/json", item_body(data().word(8U))},
      "If-Match: " + stale + "\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 412 Precondition Failed");
  EXPECT_EQ(harness().stored_name(1U), winner);
}

TEST_F(RestPutTest, RefusesAWeakTagInIfMatch) {
  // Arrange
  const std::string tag = etag_of(harness().send(
      ExchangePut{"PUT /api/items/1 HTTP/1.1", "application/json", item_body(data().word(5U))}));

  // Act
  const std::string response = harness().send(
      ExchangePut{"PUT /api/items/1 HTTP/1.1", "application/json", item_body(data().word(5U))},
      "If-Match: W/" + tag + "\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 412 Precondition Failed");
}

TEST_F(RestPutTest, RefusesAnApiRequestWithoutAToken) {
  // Arrange
  const std::string wire = "PUT /api/items/1 HTTP/1.1\r\nHost: mado.test\r\n\r\n";

  // Act
  const std::string response = harness().raw(wire);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
  EXPECT_NE(response.find("WWW-Authenticate: Bearer"), std::string::npos);
}

TEST_F(RestPutTest, IssuesATokenForTheRightPassword) {
  // Arrange
  const std::string body =
      std::string(R"({"username":"admin","password":")") + test_secrets::kPassword + R"("})";

  // Act
  const std::string response = harness().raw(
      "POST /api/auth/login HTTP/1.1\r\nHost: mado.test\r\nContent-Type: application/json\r\n"
      "Content-Length: " +
      std::to_string(body.size()) + "\r\n\r\n" + body);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(login_body_of(response).find(R"("token_type":"Bearer")"), std::string::npos);
}

}
