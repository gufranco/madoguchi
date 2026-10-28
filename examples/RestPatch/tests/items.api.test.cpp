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
constexpr uint32_t kRevisionSeed = 0x0BADF00DUL;

using Catalog = items::ItemCatalog<items::RamItemRepository>;
using Authority = identity::FlashSecretsAuthority<test_secrets::TestSecrets>;
using Failures = identity::CardFailureLog<FakeSd, FILE_READ, FILE_WRITE>;
using Access = identity::AccessService<Authority, Failures>;
using Gatekeeper = TestAccess<Authority, Failures, Access>;
using App = api::Router<FakeClient, Access, items::RamItemRepository, FakeSd, FILE_READ>;
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

std::string etag_of(const std::string& response) {
  const size_t start = response.find("ETag: ");
  if (start == std::string::npos) {
    return "";
  }
  return response.substr(start + 6U, response.find("\r\n", start) - (start + 6U));
}

std::string rename_body(const std::string& name) {
  return R"({"name":")" + name + R"("})";
}

std::string login_body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

class PatchHarness {
 public:
  std::string raw(const std::string& wire) {
    FakeClient client(wire);
    server_.process(client);
    return client.output();
  }

  PatchHarness() {
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

  std::string send(const ExchangePatch& request, const std::string& headers = "") {
    FakeClient client(request.line + "\r\nHost: mado.test\r\n" + bearer() + "" + headers +
                      "Content-Type: " + request.type + "\r\nContent-Length: " +
                      std::to_string(request.body.size()) + "\r\n\r\n" + request.body);
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

TEST_F(RestPatchTest, SendsTheTagOfTheChangedItem) {
  // Arrange
  const std::string body = rename_body(data().word(5U));

  // Act
  const std::string response =
      harness().send(ExchangePatch{"PATCH /api/items/2 HTTP/1.1", "application/json", body});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(etag_of(response), "\"2-0badf00f\"");
}

TEST_F(RestPatchTest, RefusesAChangeBasedOnAStaleTag) {
  // Arrange
  const std::string stale = etag_of(harness().send(ExchangePatch{
      "PATCH /api/items/1 HTTP/1.1",
      "application/json",
      rename_body(data().word(5U)),
  }));
  const std::string winner = data().word(6U);
  static_cast<void>(harness().send(
      ExchangePatch{"PATCH /api/items/1 HTTP/1.1", "application/json", rename_body(winner)}));

  // Act
  const std::string response = harness().send(
      ExchangePatch{
          "PATCH /api/items/1 HTTP/1.1",
          "application/json",
          rename_body(data().word(7U)),
      },
      "If-Match: " + stale + "\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 412 Precondition Failed");
  EXPECT_EQ(harness().stored_name(1U), winner);
}

TEST_F(RestPatchTest, ChangesWhenIfMatchNamesTheCurrentTag) {
  // Arrange
  const std::string tag = etag_of(harness().send(ExchangePatch{
      "PATCH /api/items/1 HTTP/1.1",
      "application/json",
      rename_body(data().word(5U)),
  }));
  const std::string name = data().word(8U);

  // Act
  const std::string response = harness().send(
      ExchangePatch{"PATCH /api/items/1 HTTP/1.1", "application/json", rename_body(name)},
      "If-Match: " + tag + "\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(harness().stored_name(1U), name);
}

TEST_F(RestPatchTest, RefusesAnApiRequestWithoutAToken) {
  // Arrange
  const std::string wire = "PATCH /api/items/1 HTTP/1.1\r\nHost: mado.test\r\n\r\n";

  // Act
  const std::string response = harness().raw(wire);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
  EXPECT_NE(response.find("WWW-Authenticate: Bearer"), std::string::npos);
}

TEST_F(RestPatchTest, IssuesATokenForTheRightPassword) {
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
