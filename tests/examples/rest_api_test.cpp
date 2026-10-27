#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "../../examples/RestApi/RestApp.h"
#include "../../examples/RestApi/generated_api.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "FakeSd.h"
#include "TestSecrets.h"
#include "mado/RequestLine.h"
#include "mado/ResponseSummary.h"

namespace {

FakeSd& store() {
  static FakeSd instance;
  return instance;
}

using test_secrets::kPassword;
using test_secrets::kToken;
using test_secrets::TestSecrets;
using test_secrets::UnprovisionedSecrets;

using App = RestApp<FakeClient, TestSecrets, FakeSd, FILE_READ, FILE_WRITE>;
struct TestLog {
  static constexpr bool kTrustedHostsOnly = false;

  static void on_response(const mado::RequestLine& request, const mado::ResponseSummary& response) {
    App::log_response(request, response);
  }
};

using AppServer = mado::Server<FakeClient, App, rest_api::Limits, TestLog>;
using BareApp = RestApp<FakeClient, UnprovisionedSecrets, FakeSd, FILE_READ, FILE_WRITE>;
using BareServer = mado::Server<FakeClient, BareApp, rest_api::Limits>;

constexpr uint32_t kSession = 7U;

struct Request {
  std::string line;
  std::string headers;
  std::string body;
};

std::string wire(const Request& request) {
  const std::string length =
      request.body.empty() ? "" : "Content-Length: " + std::to_string(request.body.size()) + "\r\n";
  return request.line + "\r\nHost: mado.test\r\n" + request.headers + length + "\r\n" +
         request.body;
}

std::string json_headers(const std::string& token = "") {
  const std::string auth = token.empty() ? "" : "Authorization: Bearer " + token + "\r\n";
  return "Content-Type: application/json\r\n" + auth;
}

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

class RestApiTest : public ::testing::Test {
 public:
  std::string send(const Request& request) {
    FakeClient client(wire(request));
    server_.process(client);
    return client.output();
  }

  std::string create(const std::string& name) {
    return send({
        "POST /api/items HTTP/1.1",
        json_headers(&kToken[0]),
        R"({"name":")" + name + R"(","value":3,"on":true})",
    });
  }

  FakeData& data() {
    return data_;
  }

 protected:
  void SetUp() override {
    fake_clock::set(0U);
    store() = FakeSd();
    App::begin(kSession, store(), 4U);
  }

 private:
  AppServer server_{&rest_api::kRoutes[0], &rest_api::kHeaderNames[0]};
  FakeData data_{20261026U};
};

TEST_F(RestApiTest, ServesTheHomePageGzipped) {
  // Arrange
  const Request request{"GET / HTTP/1.1", "Accept-Encoding: gzip\r\n", ""};

  // Act
  const std::string response = send(request);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("Content-Type: text/html; charset=utf-8\r\n"), std::string::npos);
  EXPECT_NE(response.find("Content-Encoding: gzip\r\n"), std::string::npos);
  EXPECT_NE(response.find("Content-Security-Policy: default-src 'none';"), std::string::npos);
  EXPECT_EQ(body_of(response).substr(0U, 2U), "\x1F\x8B");
}

TEST_F(RestApiTest, ServesTheDocsPageAtBothSpellings) {
  // Arrange
  const Request request{"GET /docs/ HTTP/1.1", "Accept-Encoding: gzip\r\n", ""};

  // Act
  const std::string response = send(request);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("script-src 'self' https://unpkg.com;"), std::string::npos);
}

TEST_F(RestApiTest, ServesTheSpecAsJson) {
  // Arrange
  const Request request{"GET /api/openapi.json HTTP/1.1", "Accept-Encoding: gzip\r\n", ""};

  // Act
  const std::string response = send(request);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("Content-Type: application/json\r\n"), std::string::npos);
}

TEST_F(RestApiTest, RevalidatesAStaticFile) {
  // Arrange
  const std::string first = send({"GET / HTTP/1.1", "Accept-Encoding: gzip\r\n", ""});
  const size_t start = first.find("ETag: ") + 6U;
  const std::string tag = first.substr(start, first.find("\r\n", start) - start);

  // Act
  const std::string response =
      send({"GET / HTTP/1.1", "Accept-Encoding: gzip\r\nIf-None-Match: " + tag + "\r\n", ""});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 304 Not Modified");
  EXPECT_NE(response.find("Cache-Control: max-age=300, must-revalidate\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), "");
  EXPECT_EQ(status_line(send({"GET /app.js HTTP/1.1", "Accept-Encoding: gzip\r\n", ""})),
            "HTTP/1.1 404 Not Found");
}

TEST_F(RestApiTest, RefusesClientsWithoutGzip) {
  // Arrange
  const Request request{"GET / HTTP/1.1", "", ""};

  // Act
  const std::string response = send(request);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 406 Not Acceptable");
}

TEST_F(RestApiTest, AnswersNotFoundForAnUnknownFile) {
  // Arrange
  const Request request{
      "GET /" + data().word(6U) + ".html HTTP/1.1",
      "Accept-Encoding: gzip\r\n",
      "",
  };

  // Act
  const std::string response = send(request);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 404 Not Found");
}

TEST_F(RestApiTest, IssuesATokenForValidCredentials) {
  // Arrange
  const Request request{
      "POST /api/auth/login HTTP/1.1",
      json_headers(),
      std::string(R"({"username":"admin","password":")") + &kPassword[0] + R"("})",
  };

  // Act
  const std::string response = send(request);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("Cache-Control: no-store\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), std::string(R"({"access_token":")") + &kToken[0] +
                                   R"(","token_type":"Bearer","expires_in":900})");
}

TEST_F(RestApiTest, RefusesAWrongPassword) {
  // Arrange
  const Request request{
      "POST /api/auth/login HTTP/1.1",
      json_headers(),
      R"({"username":"admin","password":")" + data().word(20U) + R"("})",
  };

  // Act
  const std::string response = send(request);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
  EXPECT_NE(response.find("WWW-Authenticate: Bearer realm=\"api\"\r\n"), std::string::npos);
}

TEST_F(RestApiTest, RefusesAWrongUserWithTheRightPassword) {
  // Arrange
  const Request request{
      "POST /api/auth/login HTTP/1.1",
      json_headers(),
      R"({"username":")" + data().word(5U) + R"(","password":")" + &kPassword[0] + R"("})",
  };

  // Act
  const std::string response = send(request);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
}

TEST_F(RestApiTest, LocksLoginAfterFiveFailures) {
  // Arrange
  const Request wrong{
      "POST /api/auth/login HTTP/1.1",
      json_headers(),
      R"({"username":"admin","password":"wrong-password-1"})",
  };
  for (int attempt = 0; attempt < 5; ++attempt) {
    static_cast<void>(send(wrong));
  }
  const Request right{
      "POST /api/auth/login HTTP/1.1",
      json_headers(),
      std::string(R"({"username":"admin","password":")") + &kPassword[0] + R"("})",
  };

  // Act
  const std::string response = send(right);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 429 Too Many Requests");
  EXPECT_NE(response.find("Retry-After: 900\r\n"), std::string::npos);
}

TEST_F(RestApiTest, KeepsTheLoginLockAcrossARestart) {
  // Arrange
  const Request wrong{
      "POST /api/auth/login HTTP/1.1",
      json_headers(),
      R"({"username":"admin","password":"wrong-password-1"})",
  };
  for (int attempt = 0; attempt < 5; ++attempt) {
    static_cast<void>(send(wrong));
  }
  App::begin(kSession + 1U, store(), 4U);
  const Request right{
      "POST /api/auth/login HTTP/1.1",
      json_headers(),
      std::string(R"({"username":"admin","password":")") + &kPassword[0] + R"("})",
  };

  // Act
  const std::string response = send(right);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 429 Too Many Requests");
}

TEST_F(RestApiTest, KeepsItemsAcrossARestart) {
  // Arrange
  const std::string name = data().word(7U);
  static_cast<void>(create(name));
  App::begin(kSession + 1U, store(), 4U);

  // Act
  const std::string response = send({"GET /api/items/1 HTTP/1.1", "", ""});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), R"({"id":1,"name":")" + name + R"(","value":3,"on":true})");
}

struct LoginCase {
  std::string headers;
  std::string body;
  const char* expected = nullptr;
};

class LoginRejectTest : public RestApiTest, public ::testing::WithParamInterface<LoginCase> {};

TEST_P(LoginRejectTest, AnswersTheRightStatus) {
  // Arrange
  const LoginCase& sample = GetParam();

  // Act
  const std::string response = send({"POST /api/auth/login HTTP/1.1", sample.headers, sample.body});

  // Assert
  EXPECT_EQ(status_line(response), sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Bodies, LoginRejectTest,
    ::testing::Values(
        LoginCase{"Content-Type: text/plain\r\n", R"({"username":"a","password":"b"})",
                  "HTTP/1.1 415 Unsupported Media Type"},
        LoginCase{"Content-Type: application/json\r\n", "", "HTTP/1.1 400 Bad Request"},
        LoginCase{"Content-Type: application/json\r\n", R"({"username":)",
                  "HTTP/1.1 400 Bad Request"},
        LoginCase{"Content-Type: application/json\r\n", R"({"username":"admin"})",
                  "HTTP/1.1 422 Unprocessable Content"},
        LoginCase{"Content-Type: application/json\r\n",
                  R"({"username":"admin","password":"x","role":"root"})",
                  "HTTP/1.1 422 Unprocessable Content"},
        LoginCase{"Content-Type: application/json\r\n", std::string(300U, ' ') + "{}",
                  "HTTP/1.1 413 Content Too Large"}));

TEST(RestApiUnprovisioned, AnswersServiceUnavailable) {
  // Arrange
  fake_clock::set(0U);
  store() = FakeSd();
  BareApp::begin(kSession, store(), 4U);
  BareServer server(&rest_api::kRoutes[0], &rest_api::kHeaderNames[0]);
  FakeClient client(wire({
      "POST /api/auth/login HTTP/1.1",
      json_headers(),
      std::string(R"({"username":"admin","password":")") + &kPassword[0] + R"("})",
  }));

  // Act
  server.process(client);

  // Assert
  EXPECT_EQ(status_line(client.output()), "HTTP/1.1 503 Service Unavailable");
}

TEST_F(RestApiTest, RefusesAWriteWithoutAToken) {
  // Arrange
  const Request request{
      "POST /api/items HTTP/1.1",
      json_headers(),
      R"({"name":"lamp","value":3,"on":true})",
  };

  // Act
  const std::string response = send(request);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
  EXPECT_EQ(response.find("invalid_token"), std::string::npos);
}

TEST_F(RestApiTest, CreatesAnItem) {
  // Arrange
  const std::string name = data().word(10U);

  // Act
  const std::string response = create(name);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 201 Created");
  EXPECT_NE(response.find("Location: /api/items/1\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), R"({"id":1,"name":")" + name + R"(","value":3,"on":true})");
}

TEST_F(RestApiTest, ReportsAFullTable) {
  // Arrange
  for (int slot = 0; slot < 4; ++slot) {
    static_cast<void>(create(data().word(4U)));
  }

  // Act
  const std::string response = create(data().word(4U));

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 507 Insufficient Storage");
}

TEST_F(RestApiTest, ListsItemsInSlotOrder) {
  // Arrange
  const std::string first = data().word(5U);
  const std::string second = data().word(5U);
  static_cast<void>(create(first));
  static_cast<void>(create(second));

  // Act
  const std::string response = send({"GET /api/items HTTP/1.1", "", ""});

  // Assert
  EXPECT_EQ(body_of(response), R"([{"id":1,"name":")" + first + R"(","value":3,"on":true},)" +
                                   R"({"id":2,"name":")" + second + R"(","value":3,"on":true}])");
}

TEST_F(RestApiTest, ListsNothingAsAnEmptyArray) {
  // Arrange
  const Request request{"GET /api/items HTTP/1.1", "", ""};

  // Act
  const std::string response = send(request);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), "[]");
}

TEST_F(RestApiTest, ReadsOneItem) {
  // Arrange
  const std::string name = data().word(7U);
  static_cast<void>(create(name));

  // Act
  const std::string response = send({"GET /api/items/1 HTTP/1.1", "", ""});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), R"({"id":1,"name":")" + name + R"(","value":3,"on":true})");
}

class MissingItemTest : public RestApiTest, public ::testing::WithParamInterface<const char*> {};

TEST_P(MissingItemTest, AnswersNotFound) {
  // Arrange
  const std::string line = GetParam();

  // Act
  const std::string response = send({line, json_headers(&kToken[0]), R"({"on":false})"});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 404 Not Found");
}

INSTANTIATE_TEST_SUITE_P(Paths, MissingItemTest,
                         ::testing::Values("GET /api/items/1 HTTP/1.1",
                                           "GET /api/items/abc HTTP/1.1",
                                           "GET /api/items/9 HTTP/1.1", "PUT /api/items/2 HTTP/1.1",
                                           "PATCH /api/items/2 HTTP/1.1",
                                           "DELETE /api/items/2 HTTP/1.1"));

TEST_F(RestApiTest, ReplacesEveryField) {
  // Arrange
  static_cast<void>(create(data().word(6U)));
  const std::string name = data().word(6U);

  // Act
  const std::string response = send({
      "PUT /api/items/1 HTTP/1.1",
      json_headers(&kToken[0]),
      R"({"name":")" + name + R"(","value":-5,"on":false})",
  });

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), R"({"id":1,"name":")" + name + R"(","value":-5,"on":false})");
}

TEST_F(RestApiTest, RefusesAReplacementMissingAField) {
  // Arrange
  static_cast<void>(create(data().word(6U)));

  // Act
  const std::string response =
      send({"PUT /api/items/1 HTTP/1.1", json_headers(&kToken[0]), R"({"value":1,"on":false})"});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
}

TEST_F(RestApiTest, PatchesOnlyTheGivenFields) {
  // Arrange
  const std::string name = data().word(6U);
  static_cast<void>(create(name));

  // Act
  const std::string response =
      send({"PATCH /api/items/1 HTTP/1.1", json_headers(&kToken[0]), R"({"on":false})"});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), R"({"id":1,"name":")" + name + R"(","value":3,"on":false})");
}

TEST_F(RestApiTest, RefusesAnEmptyPatch) {
  // Arrange
  static_cast<void>(create(data().word(6U)));

  // Act
  const std::string response =
      send({"PATCH /api/items/1 HTTP/1.1", json_headers(&kToken[0]), "{}"});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
}

TEST_F(RestApiTest, RefusesAValueOutOfRange) {
  // Arrange
  static_cast<void>(create(data().word(6U)));

  // Act
  const std::string response =
      send({"PATCH /api/items/1 HTTP/1.1", json_headers(&kToken[0]), R"({"value":100001})"});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
}

TEST_F(RestApiTest, RefusesToSetTheReadOnlyId) {
  // Arrange
  static_cast<void>(create(data().word(6U)));

  // Act
  const std::string response =
      send({"PATCH /api/items/1 HTTP/1.1", json_headers(&kToken[0]), R"({"id":2})"});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
}

TEST_F(RestApiTest, DeletesAnItemOnce) {
  // Arrange
  static_cast<void>(create(data().word(6U)));
  const Request request{
      "DELETE /api/items/1 HTTP/1.1",
      std::string("Authorization: Bearer ") + &kToken[0] + "\r\n",
      "",
  };
  const std::string first = send(request);

  // Act
  const std::string second = send(request);

  // Assert
  EXPECT_EQ(status_line(first), "HTTP/1.1 204 No Content");
  EXPECT_EQ(status_line(second), "HTTP/1.1 404 Not Found");
}

TEST_F(RestApiTest, ListsAllowedMethodsForAWrongVerb) {
  // Arrange
  const Request request{"DELETE /api/items HTTP/1.1", "", ""};

  // Act
  const std::string response = send(request);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 405 Method Not Allowed");
  EXPECT_NE(response.find("Allow: GET, HEAD, POST\r\n"), std::string::npos);
}

TEST_F(RestApiTest, RefusesAnExpiredToken) {
  // Arrange
  fake_clock::set(900000U);

  // Act
  const std::string response = create(data().word(6U));

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
  EXPECT_NE(response.find("error=\"invalid_token\""), std::string::npos);
}

TEST_F(RestApiTest, RefusesATokenFromAnEarlierBoot) {
  // Arrange
  App::begin(kSession + 1U, store(), 4U);

  // Act
  const std::string response = create(data().word(6U));

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
}

std::vector<std::string> log_lines() {
  std::vector<std::string> lines;
  const std::string& text = store().files()["/LOG.TXT"];
  for (size_t start = 0U; start < text.size();) {
    const size_t end = text.find('\n', start);
    lines.push_back(text.substr(start, end - start));
    start = (end == std::string::npos) ? text.size() : end + 1U;
  }
  return lines;
}

TEST_F(RestApiTest, WritesOnePinoStyleJsonLinePerRequest) {
  // Arrange
  const std::string name = data().word(6U);
  static_cast<void>(create(name));

  // Act
  const std::string response = send({"GET /api/items/9 HTTP/1.1", "", ""});

  // Assert
  const std::vector<std::string> lines = log_lines();
  ASSERT_EQ(lines.size(), 2U);
  EXPECT_EQ(status_line(response), "HTTP/1.1 404 Not Found");
  EXPECT_EQ(lines.at(0).substr(0U, 27U), R"({"level":30,"boot":7,"uptim)");
  EXPECT_NE(lines.at(0).find(
                R"("hostname":"madoguchi","req":{"id":1,"method":"POST","url":"/api/items"},)"
                R"("res":{"statusCode":201},"responseTime":)"),
            std::string::npos);
  EXPECT_NE(lines.at(0).find(R"("msg":"request completed"})"), std::string::npos);
  EXPECT_NE(lines.at(1).find(
                R"("req":{"id":2,"method":"GET","url":"/api/items/9"},"res":{"statusCode":404})"),
            std::string::npos);
}

TEST_F(RestApiTest, LogsLoginOutcomesWithoutTheUserName) {
  // Arrange
  const Request wrong{
      "POST /api/auth/login HTTP/1.1",
      json_headers(),
      R"({"username":"admin","password":"wrong-password-1"})",
  };

  // Act
  static_cast<void>(send(wrong));

  // Assert
  const std::vector<std::string> lines = log_lines();
  ASSERT_EQ(lines.size(), 2U);
  EXPECT_EQ(lines.at(0).substr(0U, 11U), R"({"level":40)");
  EXPECT_NE(lines.at(0).find(R"("msg":"login failed"})"), std::string::npos);
  EXPECT_EQ(lines.at(0).find("admin"), std::string::npos);
  EXPECT_NE(lines.at(1).find(R"("statusCode":401)"), std::string::npos);
}

TEST_F(RestApiTest, EscapesAPathThatTriesToForgeALine) {
  // Arrange
  const std::string line = "GET /a\"b\x01x HTTP/1.1";

  // Act
  static_cast<void>(send({line, "", ""}));

  // Assert
  const std::vector<std::string> lines = log_lines();
  ASSERT_EQ(lines.size(), 1U);
  EXPECT_NE(lines.at(0).find(R"("url":"/a\"b\u0001x")"), std::string::npos);
}

TEST_F(RestApiTest, ServesTheLogOnlyWithAToken) {
  // Arrange
  static_cast<void>(create(data().word(5U)));

  // Act
  const std::string refused = send({"GET /api/log HTTP/1.1", "", ""});

  // Assert
  const std::string allowed = send(
      {"GET /api/log HTTP/1.1", std::string("Authorization: Bearer ") + &kToken[0] + "\r\n", ""});
  EXPECT_EQ(status_line(refused), "HTTP/1.1 401 Unauthorized");
  EXPECT_EQ(status_line(allowed), "HTTP/1.1 200 OK");
  EXPECT_NE(allowed.find("Content-Type: application/x-ndjson\r\n"), std::string::npos);
  EXPECT_NE(allowed.find("Cache-Control: no-store\r\n"), std::string::npos);
  EXPECT_NE(body_of(allowed).find(R"("msg":"request completed")"), std::string::npos);
}

TEST_F(RestApiTest, ServesTheEndOfTheLog) {
  // Arrange
  static_cast<void>(create(data().word(5U)));
  const std::string auth = std::string("Authorization: Bearer ") + &kToken[0] + "\r\n";

  // Act
  const std::string response = send({"GET /api/log HTTP/1.1", auth + "Range: bytes=-10\r\n", ""});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 206 Partial Content");
  EXPECT_EQ(body_of(response), "mpleted\"}\n");
}

TEST_F(RestApiTest, ClearsTheLog) {
  // Arrange
  static_cast<void>(create(data().word(5U)));
  store().files()["/LOG.OLD"] = "{}\n";
  const std::string auth = std::string("Authorization: Bearer ") + &kToken[0] + "\r\n";

  // Act
  const std::string response = send({"DELETE /api/log HTTP/1.1", auth, ""});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 204 No Content");
  EXPECT_EQ(store().files().count("/LOG.OLD"), 0U);
  EXPECT_EQ(log_lines().size(), 1U);
}

TEST_F(RestApiTest, StoresEachItemAsOneJsonObjectWithoutItsId) {
  // Arrange
  const std::string name = data().word(7U);

  // Act
  static_cast<void>(create(name));

  // Assert
  EXPECT_EQ(store().files().at("/ITEM1.TXT"), R"({"name":")" + name +
                                                  R"(","value":3,"on":true})"
                                                  "\n");
  EXPECT_EQ(store().files().count("/STORE.TMP"), 0U);
}

TEST_F(RestApiTest, RemovesTheFileOfADeletedItem) {
  // Arrange
  static_cast<void>(create(data().word(7U)));
  const std::string auth = std::string("Authorization: Bearer ") + &kToken[0] + "\r\n";

  // Act
  const std::string response = send({"DELETE /api/items/1 HTTP/1.1", auth, ""});

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 204 No Content");
  EXPECT_EQ(store().files().count("/ITEM1.TXT"), 0U);
}

TEST_F(RestApiTest, KeepsWorkingInRamWithoutACard) {
  // Arrange
  store() = FakeSd();
  store().set_present(false);
  App::begin(kSession, store(), 4U);
  const std::string auth = std::string("Authorization: Bearer ") + &kToken[0] + "\r\n";

  // Act
  const std::string created = create(data().word(6U));

  // Assert
  EXPECT_EQ(status_line(created), "HTTP/1.1 201 Created");
  EXPECT_EQ(status_line(send({"GET /api/items/1 HTTP/1.1", "", ""})), "HTTP/1.1 200 OK");
  EXPECT_EQ(status_line(send({"GET /api/log HTTP/1.1", auth, ""})),
            "HTTP/1.1 503 Service Unavailable");
  EXPECT_TRUE(store().files().empty());
}

TEST_F(RestApiTest, SkipsAStoredItemItCannotRead) {
  // Arrange
  store() = FakeSd();
  store().files()["/ITEM2.TXT"] = "{\"name\":";
  store().files()["/ITEM3.TXT"] = "{\"name\":\"fan\",\"value\":1,\"on\":false}\n";

  // Act
  App::begin(kSession, store(), 4U);

  // Assert
  EXPECT_EQ(status_line(send({"GET /api/items/2 HTTP/1.1", "", ""})), "HTTP/1.1 404 Not Found");
  EXPECT_EQ(body_of(send({"GET /api/items/3 HTTP/1.1", "", ""})),
            R"({"id":3,"name":"fan","value":1,"on":false})");
}

}
