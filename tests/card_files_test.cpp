#include <Arduino.h>
#include <gtest/gtest.h>
#include <stddef.h>

#include <string>

#include "ExchangeHarness.h"
#include "FakeData.h"
#include "FakeSd.h"
#include "mado/HttpVersion.h"
#include "mado/Method.h"
#include "mado/RequestLine.h"
#include "mado/serve_card_file.h"

namespace {

const char kGzipHeader[] = "Accept-Encoding";
const char* const kCardHeaders[] = {&kGzipHeader[0], nullptr};
const char kSpecPath[] PROGMEM = "/openapi.json";

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

struct CardRequest {
  const char* path = "";
  const char* accept = "";
  mado::Method method = mado::Method::kGet;
};

std::string serve(FakeSd& card, const CardRequest& request) {
  ExchangeHarness harness;
  harness.set_line(
      mado::RequestLine{request.method, request.path, "", true, mado::HttpVersion::kHttp11});
  harness.capture(&kCardHeaders[0], 0U, request.accept);
  HarnessExchange exchange = harness.make();
  const bool served = mado::serve_card_file<FILE_READ>(exchange, card);
  return (served ? "served\n" : "failed\n") + harness.sent();
}

TEST(ServeCardFile, SendsAPageWithoutCachingIt) {
  // Arrange
  FakeData data(20261028U);
  const std::string page = "<p>" + data.word(12U) + "</p>";
  FakeSd card;
  card.files()["/index.html"] = page;

  // Act
  const std::string response = serve(card, {"/", ""});

  // Assert
  EXPECT_EQ(response,
            "served\nHTTP/1.1 200 OK\r\nConnection: close\r\n"
            "Content-Type: text/html; charset=utf-8\r\nContent-Length: " +
                std::to_string(page.size()) +
                "\r\nVary: Accept-Encoding\r\nCache-Control: no-cache\r\n\r\n" + page);
}

TEST(ServeCardFile, PrefersTheGzipCopyWhenTheClientAcceptsIt) {
  // Arrange
  FakeSd card;
  card.files()["/app.css"] = "a{}";
  card.files()["/app.css.gz"] = "\x1F\x8B";

  // Act
  const std::string response = serve(card, {"/app.css", "gzip, br"});

  // Assert
  EXPECT_NE(response.find("Content-Encoding: gzip\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), "\x1F\x8B");
}

TEST(ServeCardFile, FallsBackToThePlainFileWithoutAGzipCopy) {
  // Arrange
  FakeSd card;
  card.files()["/app.css"] = "a{}";

  // Act
  const std::string response = serve(card, {"/app.css", "gzip"});

  // Assert
  EXPECT_EQ(response.find("Content-Encoding"), std::string::npos);
  EXPECT_EQ(body_of(response), "a{}");
}

TEST(ServeCardFile, CachesHashedNamesForAYear) {
  // Arrange
  FakeSd card;
  card.files()["/9f86d081.js"] = "x";
  card.files()["/9f86d08.js"] = "y";
  card.files()["/9f86d08g.js"] = "z";

  // Act
  const std::string hashed = serve(card, {"/9f86d081.js", ""});

  // Assert
  EXPECT_NE(hashed.find("Cache-Control: public, max-age=31536000, immutable\r\n"),
            std::string::npos);
  EXPECT_NE(serve(card, {"/9f86d08.js", ""}).find("Cache-Control: no-cache\r\n"),
            std::string::npos);
  EXPECT_NE(serve(card, {"/9f86d08g.js", ""}).find("Cache-Control: no-cache\r\n"),
            std::string::npos);
}

TEST(ServeCardFile, ServesALongNameInASubfolder) {
  // Arrange
  FakeData data(20261101U);
  const std::string content = data.word(20U);
  FakeSd card;
  card.files()["/docs/swagger-initializer.js"] = content;

  // Act
  const std::string response = serve(card, {"/docs/swagger-initializer.js", ""});

  // Assert
  EXPECT_NE(response.find("Content-Type: text/javascript\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), content);
}

TEST(ServeCardFile, RefusesAFileTooLargeToCount) {
  // Arrange
  FakeSd card;
  card.files()["/movie.mkv"] = "x";
  card.report_size("/movie.mkv", 0x100000000ULL);

  // Act
  const std::string response = serve(card, {"/movie.mkv", ""});

  // Assert
  EXPECT_EQ(response.substr(0U, 41U), "served\nHTTP/1.1 500 Internal Server Error");
}

TEST(ServeCardFile, ServesThePlainFileWhenTheGzipNameWouldNotFit) {
  // Arrange
  const std::string path = "/" + std::string(mado::kCardPathSize - 4U, 'a');
  FakeSd card;
  card.files()[path] = "plain";
  card.files()[path + ".gz"] = "\x1F\x8B";

  // Act
  const std::string response = serve(card, {path.c_str(), "gzip"});

  // Assert
  EXPECT_EQ(response.find("Content-Encoding"), std::string::npos);
  EXPECT_EQ(body_of(response), "plain");
}

TEST(ServeCardFile, AnswersHeadWithHeadersOnly) {
  // Arrange
  FakeSd card;
  card.files()["/index.html"] = "<p>hi</p>";

  // Act
  const std::string response = serve(card, {"/", "", mado::Method::kHead});

  // Assert
  EXPECT_NE(response.find("Content-Length: 9\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), "");
}

TEST(ServeCardFile, RefusesAPathThatClimbsOut) {
  // Arrange
  FakeSd card;

  // Act
  const std::string response = serve(card, {"/../secret.txt", ""});

  // Assert
  EXPECT_EQ(response.substr(0U, 31U), "served\nHTTP/1.1 400 Bad Request");
}

TEST(ServeCardFile, AnswersNotFoundForAMissingFileOrAFolder) {
  // Arrange
  FakeSd card;
  card.files()["/a.txt"] = "x";

  // Act
  const std::string missing = serve(card, {"/b.txt", ""});

  // Assert
  EXPECT_EQ(missing.substr(0U, 29U), "served\nHTTP/1.1 404 Not Found");
  EXPECT_EQ(serve(card, {"/", ""}).substr(0U, 29U), "served\nHTTP/1.1 404 Not Found");
}

TEST(ServeCardPath, ServesAFixedFileAndItsGzipCopy) {
  // Arrange
  FakeSd card;
  card.files()["/openapi.json"] = "{}";
  card.files()["/openapi.json.gz"] = "\x1F\x8B";
  ExchangeHarness harness;
  harness.capture(&kCardHeaders[0], 0U, "gzip");
  HarnessExchange exchange = harness.make();

  // Act
  const bool served = mado::serve_card_path<FILE_READ>(exchange, card, &kSpecPath[0]);

  // Assert
  const std::string response = harness.sent();
  EXPECT_TRUE(served);
  EXPECT_NE(response.find("Content-Type: application/json\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), "\x1F\x8B");
}

TEST(ServeCardPath, RefusesAPathTooLongForItsBuffer) {
  // Arrange
  FakeSd card;
  const std::string longest(mado::kCardPathSize, 'a');
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool served = mado::serve_card_path<FILE_READ>(exchange, card, longest.c_str());

  // Assert
  EXPECT_TRUE(served);
  EXPECT_EQ(harness.sent().substr(0U, 34U), "HTTP/1.1 500 Internal Server Error");
}

}
