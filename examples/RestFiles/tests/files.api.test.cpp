#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>

#include "mado/Sha256.h"
#include "mado/kSha256Size.h"

#include "../src/files/application/file_library.service.h"
#include "../src/http/api.generated.h"
#include "../src/http/api.router.h"
#include "../src/identity/adapters/card_failure.store.h"
#include "../src/identity/adapters/flash_secrets.authority.h"
#include "../src/identity/application/access.service.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "FakeSd.h"
#include "TestAccess.h"
#include "TestSecrets.h"

namespace {

using Library = files::FileLibrary<FakeSd, FakeSdFile, FILE_READ, FILE_WRITE>;
using Authority = identity::FlashSecretsAuthority<test_secrets::TestSecrets>;
using Failures = identity::CardFailureLog<FakeSd, FILE_READ, FILE_WRITE>;
using Access = identity::AccessService<Authority, Failures>;
using Gatekeeper = TestAccess<Authority, Failures, Access>;
using App = api::Router<FakeClient, Access, Library, FakeSd, FakeSdFile, FILE_READ>;
using AppServer = mado::Server<FakeClient, App, rest_files::Limits>;

constexpr uint32_t kBootSeed = 0x5EED0001UL;
constexpr const char* kHelloDigest = "sha-256=:LPJNul+wow4m6DsqxbninhsWHlwfp0JecwQzYpOLmCQ=:";

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

std::string base64_of(const uint8_t* const bytes, const size_t length) {
  static const char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0U; i < length; i += 3U) {
    const uint32_t first = bytes[i];
    const uint32_t second = ((i + 1U) < length) ? bytes[i + 1U] : 0U;
    const uint32_t third = ((i + 2U) < length) ? bytes[i + 2U] : 0U;
    const uint32_t group = (first << 16U) | (second << 8U) | third;
    out += kAlphabet[(group >> 18U) & 0x3FU];
    out += kAlphabet[(group >> 12U) & 0x3FU];
    out += ((i + 1U) < length) ? kAlphabet[(group >> 6U) & 0x3FU] : '=';
    out += ((i + 2U) < length) ? kAlphabet[group & 0x3FU] : '=';
  }
  return out;
}

std::string digest_header(const std::string& content) {
  mado::Sha256 hash;
  for (const char symbol : content) {
    static_cast<void>(hash.update(static_cast<uint8_t>(symbol)));
  }
  uint8_t digest[mado::kSha256Size] = {};
  static_cast<void>(hash.finish(&digest[0]));
  return "Repr-Digest: sha-256=:" + base64_of(&digest[0], sizeof(digest)) + ":\r\n";
}

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

std::string login_body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

class RestFilesTest : public ::testing::Test {
 public:
  std::string raw(const std::string& wire) {
    FakeClient client(wire);
    server_.process(client);
    return client.output();
  }

  std::string send(const std::string& line, const std::string& headers = "",
                   const std::string& body = "") {
    const std::string length =
        body.empty() ? "" : "Content-Length: " + std::to_string(body.size()) + "\r\n";
    FakeClient client(line + "\r\nHost: mado.test\r\n" + bearer() + "" + headers + length + "\r\n" +
                      body);
    server_.process(client);
    return client.output();
  }

  std::string upload(const std::string& name, const std::string& content,
                     const std::string& headers = "") {
    return send("PUT /api/files/" + name + " HTTP/1.1", headers + digest_header(content), content);
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
    restart(kBootSeed);
  }

  void restart(const uint32_t seed) {
    library_ = Library();
    static_cast<void>(library_.begin(4U, sd_, seed));
    Gatekeeper& keeper = fresh_gatekeeper<Gatekeeper>();
    App::attach_access(keeper.access, keeper.clock);
    App::attach(library_);
  }

 private:
  FakeSd sd_;
  Library library_;
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

TEST_F(RestFilesTest, ListsLongNamesAndSkipsOnesTooLongToServe) {
  // Arrange
  sd().files()["/holiday photo (2).jpeg"] = "abc";
  sd().files()["/" + std::string(65U, 'n')] = "x";
  sd().files()["/z.txt"] = "12";

  // Act
  const std::string response = send("GET /api/files HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), R"([{"name":"holiday photo (2).jpeg","size":3,"directory":false},)"
                               R"({"name":"z.txt","size":2,"directory":false}])");
}

TEST_F(RestFilesTest, DownloadsAWholeFileWithAStrongTag) {
  // Arrange
  const std::string content = data().word(40U);
  sd().files()["/DATA.TXT"] = content;

  // Act
  const std::string response = send("GET /api/files/DATA.TXT HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("Content-Type: text/plain"), std::string::npos);
  EXPECT_NE(response.find("ETag: \""), std::string::npos);
  EXPECT_EQ(response.find("ETag: W/"), std::string::npos);
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
  const std::string replaced =
      upload("A.TXT", "abcdefghij", "Content-Type: application/octet-stream\r\n");

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
  restart(kBootSeed + 99U);

  // Act
  const std::string after_restart = etag_of(send("GET /api/files/A.TXT HTTP/1.1"));

  // Assert
  EXPECT_EQ(status_line(deleted), "HTTP/1.1 204 No Content");
  EXPECT_NE(after_delete, before);
  EXPECT_NE(after_restart, after_delete);
}

TEST_F(RestFilesTest, SendsTheRangeWhenIfRangeNamesTheCurrentTag) {
  // Arrange
  sd().files()["/A.TXT"] = "0123456789";
  const std::string tag = etag_of(send("GET /api/files/A.TXT HTTP/1.1"));

  // Act
  const std::string response =
      send("GET /api/files/A.TXT HTTP/1.1", "Range: bytes=3-4\r\nIf-Range: " + tag + "\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 206 Partial Content");
  EXPECT_EQ(body_of(response), "34");
}

TEST_F(RestFilesTest, SendsTheWholeFileWhenIfRangeNamesAnOldTag) {
  // Arrange
  sd().files()["/A.TXT"] = "0123456789";
  const std::string stale = etag_of(send("GET /api/files/A.TXT HTTP/1.1"));
  const std::string fresh = data().word(10U);
  static_cast<void>(upload("A.TXT", fresh));

  // Act
  const std::string response =
      send("GET /api/files/A.TXT HTTP/1.1", "Range: bytes=3-4\r\nIf-Range: " + stale + "\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), fresh);
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
  EXPECT_EQ(status_line(send("GET /api/files/a%3Fb.txt HTTP/1.1")), "HTTP/1.1 400 Bad Request");
  EXPECT_EQ(status_line(send("GET /api/files/" + std::string(65U, 'a') + " HTTP/1.1")),
            "HTTP/1.1 400 Bad Request");
  EXPECT_EQ(status_line(send("GET /api/files/..%2FX HTTP/1.1")), "HTTP/1.1 400 Bad Request");
  EXPECT_EQ(status_line(send("DELETE /api/files/NONE.TXT HTTP/1.1")), "HTTP/1.1 404 Not Found");
}

TEST_F(RestFilesTest, CreatesAFileAndPointsAtIt) {
  // Arrange
  const std::string content = data().word(700U);

  // Act
  const std::string response = upload("NEW.TXT", content);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 201 Created");
  EXPECT_NE(response.find("Location: /api/files/NEW.TXT\r\n"), std::string::npos);
  EXPECT_EQ(sd().files().at("/NEW.TXT"), content);
  EXPECT_EQ(sd().files().count("/UPLOAD.TMP"), 0U);
}

TEST_F(RestFilesTest, StoresALongNameAndEncodesItsLocation) {
  // Arrange
  const std::string content = data().word(40U);

  // Act
  const std::string response = upload("holiday%20photo%20(2).jpeg", content);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 201 Created");
  EXPECT_NE(response.find("Location: /api/files/holiday%20photo%20%282%29.jpeg\r\n"),
            std::string::npos);
  EXPECT_EQ(sd().files().at("/holiday photo (2).jpeg"), content);
}

TEST_F(RestFilesTest, NamesALongFileInTheDownloadHeader) {
  // Arrange
  sd().files()["/swagger-initializer.js"] = data().word(12U);

  // Act
  const std::string response = send("GET /api/files/swagger-initializer.js?download=1 HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(
      response.find("Content-Disposition: attachment; filename=\"swagger-initializer.js\"\r\n"),
      std::string::npos);
}

TEST_F(RestFilesTest, RefusesToServeAFileOverFourGibibytes) {
  // Arrange
  sd().files()["/movie.mkv"] = "x";
  sd().report_size("/movie.mkv", 0x100000000ULL);

  // Act
  const std::string response = send("GET /api/files/movie.mkv HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 500 Internal Server Error");
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

TEST_F(RestFilesTest, RefusesAnUploadWithoutADigest) {
  // Arrange
  const std::string old = data().word(20U);
  sd().files()["/A.TXT"] = old;

  // Act
  const std::string response = send("PUT /api/files/A.TXT HTTP/1.1", "", data().word(30U));

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 400 Bad Request");
  EXPECT_NE(body_of(response).find("a Repr-Digest with a sha-256 digest is required"),
            std::string::npos);
  EXPECT_EQ(sd().files().at("/A.TXT"), old);
}

TEST_F(RestFilesTest, ReplacesAFileWhoseTagStillMatches) {
  // Arrange
  sd().files()["/A.TXT"] = data().word(20U);
  const std::string tag = etag_of(send("GET /api/files/A.TXT HTTP/1.1"));
  const std::string content = data().word(30U);

  // Act
  const std::string response = upload("A.TXT", content, "If-Match: " + tag + "\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 204 No Content");
  EXPECT_EQ(sd().files().at("/A.TXT"), content);
}

TEST_F(RestFilesTest, RefusesAReplaceAfterTheFileChanged) {
  // Arrange
  sd().files()["/A.TXT"] = data().word(20U);
  const std::string stale = etag_of(send("GET /api/files/A.TXT HTTP/1.1"));
  const std::string winner = data().word(25U);
  const std::string first = upload("A.TXT", winner);

  // Act
  const std::string response = upload("A.TXT", data().word(30U), "If-Match: " + stale + "\r\n");

  // Assert
  EXPECT_EQ(status_line(first), "HTTP/1.1 204 No Content");
  EXPECT_EQ(status_line(response), "HTTP/1.1 412 Precondition Failed");
  EXPECT_EQ(sd().files().at("/A.TXT"), winner);
  EXPECT_EQ(sd().files().count("/UPLOAD.TMP"), 0U);
}

TEST_F(RestFilesTest, RefusesIfMatchForAFileThatDoesNotExist) {
  // Arrange
  const std::string content = data().word(30U);

  // Act
  const std::string response = upload("NEW.TXT", content, "If-Match: *\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 412 Precondition Failed");
  EXPECT_EQ(sd().files().count("/NEW.TXT"), 0U);
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
  const std::string response = upload("A.TXT", data().word(64U));

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
  restart(kBootSeed);
  const std::string missing = send("GET /api/files HTTP/1.1");
  sd().set_present(true);

  // Act
  const std::string response = send("GET /api/files HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(missing), "HTTP/1.1 503 Service Unavailable");
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(sd().begins(), 4U);
}

TEST_F(RestFilesTest, ServesTheSiteFromTheCard) {
  // Arrange
  sd().files()["/index.html"] = "<!doctype html>";
  sd().files()["/index.html.gz"] = "\x1F\x8B";
  const std::string line = "GET / HTTP/1.1";

  // Act
  const std::string response = send(line, "Accept-Encoding: gzip\r\n");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("Content-Encoding: gzip"), std::string::npos);
}

TEST_F(RestFilesTest, RefusesAnApiRequestWithoutAToken) {
  // Arrange
  const std::string wire = "DELETE /api/files/A.TXT HTTP/1.1\r\nHost: mado.test\r\n\r\n";

  // Act
  const std::string response = raw(wire);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
  EXPECT_NE(response.find("WWW-Authenticate: Bearer"), std::string::npos);
}

TEST_F(RestFilesTest, IssuesATokenForTheRightPassword) {
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
