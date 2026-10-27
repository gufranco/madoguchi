#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>

#include "../../examples/RestFiles/FilesApp.h"
#include "../../examples/RestFiles/generated_api.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "FakeSd.h"

namespace {

using App = FilesApp<FakeClient, FakeSd, FakeSdFile, FILE_READ, FILE_WRITE>;
using AppServer = mado::Server<FakeClient, App, rest_files::Limits>;

constexpr uint32_t kBootSeed = 0x5EED0001UL;
constexpr const char* kHelloDigest = "sha-256=:LPJNul+wow4m6DsqxbninhsWHlwfp0JecwQzYpOLmCQ=:";

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

class RestFilesTest : public ::testing::Test {
 public:
  std::string send(const std::string& line, const std::string& headers = "",
                   const std::string& body = "") {
    const std::string length =
        body.empty() ? "" : "Content-Length: " + std::to_string(body.size()) + "\r\n";
    FakeClient client(line + "\r\nHost: mado.test\r\n" + headers + length + "\r\n" + body);
    server_.process(client);
    return client.output();
  }

  FakeSd& sd() {
    return sd_;
  }

  FakeData& data() {
    return data_;
  }

 protected:
  void SetUp() override {
    fake_clock::set(0U);
    App::begin(sd_, 4U, kBootSeed);
  }

 private:
  FakeSd sd_;
  AppServer server_{&rest_files::kRoutes[0], &rest_files::kHeaderNames[0]};
  FakeData data_{20261116U};
};

TEST_F(RestFilesTest, ListsTheRootFolder) {
  // Arrange
  sd().files()["/A.TXT"] = "abc";
  sd().files()["/LOG.TXT"] = "12345";

  // Act
  const std::string response = send("GET /api/files HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), R"([{"name":"A.TXT","size":3,"directory":false},)"
                               R"({"name":"LOG.TXT","size":5,"directory":false}])");
}

TEST_F(RestFilesTest, DownloadsAWholeFileWithAWeakTag) {
  // Arrange
  const std::string content = data().word(40U);
  sd().files()["/DATA.TXT"] = content;

  // Act
  const std::string response = send("GET /api/files/DATA.TXT HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("Content-Type: text/plain"), std::string::npos);
  EXPECT_NE(response.find("ETag: W/\""), std::string::npos);
  EXPECT_NE(response.find("Accept-Ranges: bytes\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), content);
}

TEST_F(RestFilesTest, SendsOneByteRange) {
  // Arrange
  sd().files()["/A.TXT"] = "0123456789";

  // Act
  const std::string response = send("GET /api/files/A.TXT HTTP/1.1", "Range: bytes=3-4\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 206 Partial Content");
  EXPECT_NE(response.find("Content-Range: bytes 3-4/10\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), "34");
}

TEST_F(RestFilesTest, AnswersNotModifiedForTheTagItSent) {
  // Arrange
  sd().files()["/A.TXT"] = "0123456789";
  const std::string first = send("GET /api/files/A.TXT HTTP/1.1");
  const size_t start = first.find("ETag: ") + 6U;
  const std::string tag = first.substr(start, first.find("\r\n", start) - start);

  // Act
  const std::string response =
      send("GET /api/files/A.TXT HTTP/1.1", "If-None-Match: " + tag + "\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 304 Not Modified");
  EXPECT_EQ(body_of(response), "");
}

std::string etag_of(const std::string& response) {
  const size_t start = response.find("ETag: ") + 6U;
  return response.substr(start, response.find("\r\n", start) - start);
}

TEST_F(RestFilesTest, ChangesTheTagWhenAFileOfTheSameSizeIsReplaced) {
  // Arrange
  sd().files()["/A.TXT"] = "0123456789";
  const std::string tag = etag_of(send("GET /api/files/A.TXT HTTP/1.1"));
  const std::string replaced = send("PUT /api/files/A.TXT HTTP/1.1",
                                    "Content-Type: application/octet-stream\r\n", "abcdefghij");

  // Act
  const std::string response =
      send("GET /api/files/A.TXT HTTP/1.1", "If-None-Match: " + tag + "\r\n");

  // Assert
  EXPECT_EQ(status_line(replaced), "HTTP/1.1 204 No Content");
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(etag_of(response), tag);
  EXPECT_EQ(body_of(response), "abcdefghij");
}

TEST_F(RestFilesTest, ChangesTheTagAfterADeleteAndAfterARestart) {
  // Arrange
  sd().files()["/A.TXT"] = "abc";
  sd().files()["/B.TXT"] = "xyz";
  const std::string before = etag_of(send("GET /api/files/A.TXT HTTP/1.1"));
  const std::string deleted = send("DELETE /api/files/B.TXT HTTP/1.1");
  const std::string after_delete = etag_of(send("GET /api/files/A.TXT HTTP/1.1"));
  App::begin(sd(), 4U, kBootSeed + 99U);

  // Act
  const std::string after_restart = etag_of(send("GET /api/files/A.TXT HTTP/1.1"));

  // Assert
  EXPECT_EQ(status_line(deleted), "HTTP/1.1 204 No Content");
  EXPECT_NE(after_delete, before);
  EXPECT_NE(after_restart, after_delete);
}

TEST_F(RestFilesTest, SendsTheWholeFileWhenARangeCarriesIfRange) {
  // Arrange
  sd().files()["/A.TXT"] = "0123456789";
  const std::string tag = etag_of(send("GET /api/files/A.TXT HTTP/1.1"));

  // Act
  const std::string response =
      send("GET /api/files/A.TXT HTTP/1.1", "Range: bytes=3-4\r\nIf-Range: " + tag + "\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), "0123456789");
}

TEST_F(RestFilesTest, AsksTheBrowserToSaveADownload) {
  // Arrange
  sd().files()["/A.TXT"] = "abc";

  // Act
  const std::string response = send("GET /api/files/A.TXT?download=1 HTTP/1.1");

  // Assert
  EXPECT_NE(response.find("Content-Disposition: attachment; filename=\"A.TXT\"\r\n"),
            std::string::npos);
}

TEST_F(RestFilesTest, AnswersNotFoundAndBadName) {
  // Arrange
  const std::string missing_line = "GET /api/files/NONE.TXT HTTP/1.1";

  // Act
  const std::string missing = send(missing_line);

  // Assert
  EXPECT_EQ(status_line(missing), "HTTP/1.1 404 Not Found");
  EXPECT_EQ(status_line(send("GET /api/files/LONGNAME1.TXT HTTP/1.1")), "HTTP/1.1 400 Bad Request");
  EXPECT_EQ(status_line(send("GET /api/files/..%2FX HTTP/1.1")), "HTTP/1.1 400 Bad Request");
  EXPECT_EQ(status_line(send("DELETE /api/files/NONE.TXT HTTP/1.1")), "HTTP/1.1 404 Not Found");
}

TEST_F(RestFilesTest, CreatesAFileAndPointsAtIt) {
  // Arrange
  const std::string content = data().word(700U);

  // Act
  const std::string response = send("PUT /api/files/NEW.TXT HTTP/1.1", "", content);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 201 Created");
  EXPECT_NE(response.find("Location: /api/files/NEW.TXT\r\n"), std::string::npos);
  EXPECT_EQ(sd().files().at("/NEW.TXT"), content);
  EXPECT_EQ(sd().files().count("/UPLOAD.TMP"), 0U);
}

TEST_F(RestFilesTest, ReplacesAFileThatMatchesItsDigest) {
  // Arrange
  sd().files()["/A.TXT"] = data().word(20U);

  // Act
  const std::string response = send("PUT /api/files/A.TXT HTTP/1.1",
                                    std::string("Repr-Digest: ") + kHelloDigest + "\r\n", "hello");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 204 No Content");
  EXPECT_EQ(sd().files().at("/A.TXT"), "hello");
}

TEST_F(RestFilesTest, KeepsTheOldFileWhenTheDigestDiffers) {
  // Arrange
  const std::string old = data().word(20U);
  sd().files()["/A.TXT"] = old;

  // Act
  const std::string response = send("PUT /api/files/A.TXT HTTP/1.1",
                                    std::string("Repr-Digest: ") + kHelloDigest + "\r\n", "hellO");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
  EXPECT_EQ(sd().files().at("/A.TXT"), old);
  EXPECT_EQ(sd().files().count("/UPLOAD.TMP"), 0U);
}

TEST_F(RestFilesTest, RefusesAnOversizedBodyBeforeReadingIt) {
  // Arrange
  const std::string line = "PUT /api/files/BIG.BIN HTTP/1.1";

  // Act
  const std::string response = send(line, "Expect: 100-continue\r\nContent-Length: 9000000\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 413 Content Too Large");
  EXPECT_EQ(response.find("100 Continue"), std::string::npos);
  EXPECT_EQ(sd().files().count("/BIG.BIN"), 0U);
}

TEST_F(RestFilesTest, ReportsAWriteTheCardRefused) {
  // Arrange
  sd().limit_writes(10U);

  // Act
  const std::string response = send("PUT /api/files/A.TXT HTTP/1.1", "", data().word(64U));

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 507 Insufficient Storage");
  EXPECT_EQ(sd().files().count("/UPLOAD.TMP"), 0U);
}

TEST_F(RestFilesTest, DeletesAFile) {
  // Arrange
  sd().files()["/A.TXT"] = "abc";

  // Act
  const std::string response = send("DELETE /api/files/A.TXT HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 204 No Content");
  EXPECT_EQ(sd().files().count("/A.TXT"), 0U);
}

TEST_F(RestFilesTest, ReportsADeleteTheCardRefused) {
  // Arrange
  sd().files()["/A.TXT"] = "abc";
  sd().refuse_remove();

  // Act
  const std::string response = send("DELETE /api/files/A.TXT HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 507 Insufficient Storage");
  EXPECT_EQ(sd().files().count("/A.TXT"), 1U);
}

TEST_F(RestFilesTest, RetriesACardThatWasMissing) {
  // Arrange
  sd().set_present(false);
  App::begin(sd(), 4U, kBootSeed);
  const std::string missing = send("GET /api/files HTTP/1.1");
  sd().set_present(true);

  // Act
  const std::string response = send("GET /api/files HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(missing), "HTTP/1.1 503 Service Unavailable");
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(sd().begins(), 4U);
}

TEST_F(RestFilesTest, ServesTheSiteFromFlash) {
  // Arrange
  const std::string line = "GET / HTTP/1.1";

  // Act
  const std::string response = send(line, "Accept-Encoding: gzip\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("Content-Encoding: gzip"), std::string::npos);
}

}
