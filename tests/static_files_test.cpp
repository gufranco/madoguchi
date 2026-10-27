#include <Arduino.h>
#include <gtest/gtest.h>

#include <string>

#include "ExchangeHarness.h"
#include "FakeData.h"
#include "FakeFile.h"
#include "mado/Method.h"
#include "mado/accepts_gzip.h"
#include "mado/content_type_for.h"
#include "mado/http_status.h"
#include "mado/resolve_static_path.h"
#include "mado/send_file.h"

namespace {

struct TypeCase {
  const char* file;
  const char* expected;
};

class ContentTypeForTest : public ::testing::TestWithParam<TypeCase> {};

TEST_P(ContentTypeForTest, MapsExtensionsCaseInsensitively) {
  // Arrange
  const TypeCase sample = GetParam();

  // Act
  const __FlashStringHelper* type = mado::content_type_for(sample.file);

  // Assert
  EXPECT_STREQ(reinterpret_cast<const char*>(type), sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Files, ContentTypeForTest,
    ::testing::Values(TypeCase{"/INDEX.HTM", "text/html; charset=utf-8"},
                      TypeCase{"/a/page.html", "text/html; charset=utf-8"},
                      TypeCase{"STYLE.CSS", "text/css"}, TypeCase{"app.js", "text/javascript"},
                      TypeCase{"m.mjs", "text/javascript"}, TypeCase{"d.json", "application/json"},
                      TypeCase{"r.txt", "text/plain; charset=utf-8"}, TypeCase{"f.xml", "text/xml"},
                      TypeCase{"i.svg", "image/svg+xml"}, TypeCase{"LED.PNG", "image/png"},
                      TypeCase{"p.jpg", "image/jpeg"}, TypeCase{"p.jpeg", "image/jpeg"},
                      TypeCase{"a.gif", "image/gif"},
                      TypeCase{"favicon.ico", "image/vnd.microsoft.icon"},
                      TypeCase{"w.webp", "image/webp"}, TypeCase{"f.woff2", "font/woff2"},
                      TypeCase{"s.mp3", "audio/mpeg"}, TypeCase{"d.pdf", "application/pdf"},
                      TypeCase{"blob.bin", "application/octet-stream"},
                      TypeCase{"/dir.v2/README", "application/octet-stream"},
                      TypeCase{nullptr, "application/octet-stream"}));

struct PathCase {
  const char* url;
  const char* expected;
};

class ResolveStaticPathTest : public ::testing::TestWithParam<PathCase> {};

TEST_P(ResolveStaticPathTest, BuildsSafePathsInsideTheRoot) {
  // Arrange
  const PathCase sample = GetParam();
  char out[32] = {};

  // Act
  const bool resolved =
      mado::resolve_static_path(sample.url, "/WWW", F("INDEX.HTM"), &out[0], sizeof(out));

  // Assert
  EXPECT_EQ(resolved ? std::string(&out[0]) : std::string("<rejected>"), sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Paths, ResolveStaticPathTest,
    ::testing::Values(
        PathCase{"/css/site.css", "/WWW/css/site.css"}, PathCase{"/", "/WWW/INDEX.HTM"},
        PathCase{"/docs/", "/WWW/docs/INDEX.HTM"}, PathCase{"/my%20file.txt", "/WWW/my file.txt"},
        PathCase{"/../secret.txt", "<rejected>"}, PathCase{"/%2e%2e/secret.txt", "<rejected>"},
        PathCase{"/a/./b", "<rejected>"}, PathCase{"/a//b", "<rejected>"},
        PathCase{"/a%5cb", "<rejected>"}, PathCase{"/a%00b", "<rejected>"},
        PathCase{"/a/..", "<rejected>"}, PathCase{"relative", "<rejected>"},
        PathCase{"/0123456789012345678901234567", "<rejected>"},
        PathCase{"/abcdefghij0123456789/", "<rejected>"}, PathCase{nullptr, "<rejected>"}));

TEST(ResolveStaticPath, RejectsARootThatFillsTheBuffer) {
  // Arrange
  char out[4] = {};

  // Act
  const bool resolved =
      mado::resolve_static_path("/a", "/WWW", F("INDEX.HTM"), &out[0], sizeof(out));

  // Assert
  EXPECT_FALSE(resolved);
  EXPECT_STREQ(&out[0], "");
}

struct GzipCase {
  const char* header;
  bool expected;
};

class AcceptsGzipTest : public ::testing::TestWithParam<GzipCase> {};

TEST_P(AcceptsGzipTest, AcceptsGzipUnlessItsWeightIsZero) {
  // Arrange
  const GzipCase sample = GetParam();

  // Act
  const bool accepted = mado::accepts_gzip(sample.header);

  // Assert
  EXPECT_EQ(accepted, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Headers, AcceptsGzipTest,
    ::testing::Values(GzipCase{"gzip, deflate, br", true}, GzipCase{"br, GZIP", true},
                      GzipCase{"deflate", false}, GzipCase{"x-gzip", false},
                      GzipCase{"gzipped", false}, GzipCase{"gzip;q=0", false},
                      GzipCase{"gzip; q=0.000", false}, GzipCase{"gzip;q=0.5", true},
                      GzipCase{"gzip;q=1, br", true}, GzipCase{"gzip;level=1", true},
                      GzipCase{"", false}, GzipCase{nullptr, false}));

TEST(SendFile, StreamsTheFileWithItsHeaders) {
  // Arrange
  FakeData data(41U);
  const std::string body = data.word(70);
  FakeFile file(body);
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_file(exchange, file, 70U, F("text/css"), false);

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/css\r\n"
            "Content-Length: 70\r\n\r\n" +
                body);
}

TEST(SendFile, MarksAGzipVariant) {
  // Arrange
  FakeFile file("zz");
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_file(exchange, file, 2U, F("text/javascript"), true);

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_NE(harness.sent().find("Content-Encoding: gzip\r\n"), std::string::npos);
}

TEST(SendFile, SendsTheCacheControlItIsGiven) {
  // Arrange
  FakeFile file("abc");
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_file(exchange, file, 3U, F("text/plain"), false, F("max-age=300"));

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\n"
            "Content-Length: 3\r\nCache-Control: max-age=300\r\n\r\nabc");
}

TEST(SendFile, SendsOnlyHeadersForHead) {
  // Arrange
  FakeFile file("abc");
  ExchangeHarness harness("", 0U, mado::Method::kHead);
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_file(exchange, file, 3U, F("text/plain"), false);

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\n"
            "Content-Length: 3\r\n\r\n");
}

TEST(SendFile, ReportsAFileThatEndsEarly) {
  // Arrange
  FakeFile file("abcdef", 4U);
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_file(exchange, file, 6U, F("text/plain"), false);

  // Assert
  EXPECT_FALSE(sent);
}

TEST(SendFile, ReportsAClientThatStopsAccepting) {
  // Arrange
  FakeData data(42U);
  FakeFile file(data.word(200));
  ExchangeHarness harness;
  harness.client().limit_write(1U);
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_file(exchange, file, 200U, F("text/plain"), false);

  // Assert
  EXPECT_FALSE(sent);
}

TEST(SendFile, RefusesAfterTheStatusWasSent) {
  // Arrange
  FakeFile file("abc");
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);

  // Act
  const bool sent = mado::send_file(exchange, file, 3U, F("text/plain"), false);

  // Assert
  EXPECT_FALSE(sent);
}

}
