#include <Arduino.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <cstddef>
#include <ostream>
#include <string>

#include "ExchangeHarness.h"
#include "FakeData.h"
#include "FakeFile.h"
#include "mado/ByteRange.h"
#include "mado/FileReply.h"
#include "mado/FileStamp.h"
#include "mado/Method.h"
#include "mado/copy_file.h"
#include "mado/file_etag.h"
#include "mado/kSha256Size.h"
#include "mado/parse_byte_range.h"
#include "mado/parse_repr_digest.h"
#include "mado/send_file.h"
#include "mado/valid_short_name.h"

namespace {

using TestExchange = HarnessExchange;

const char* const* file_headers() {
  static const char* const kNames[] = {"Range", "If-None-Match", "If-Range", nullptr};
  return &kNames[0];
}

struct RangeCase {
  const char* header;
  uint32_t size;
  mado::RangeStatus status;
  uint32_t first;
  uint32_t last;
};

std::ostream& operator<<(std::ostream& stream, const RangeCase& sample) {
  return stream << (sample.header == nullptr ? "<none>" : sample.header) << " of " << sample.size;
}

class ByteRangeTest : public ::testing::TestWithParam<RangeCase> {};

TEST_P(ByteRangeTest, ReadsOneRangeAsRfc9110Describes) {
  // Arrange
  const RangeCase& sample = GetParam();
  mado::ByteRange range = {7U, 7U};

  // Act
  const mado::RangeStatus status = mado::parse_byte_range(sample.header, sample.size, range);

  // Assert
  EXPECT_EQ(status, sample.status);
  EXPECT_EQ(range.first, sample.first);
  EXPECT_EQ(range.last, sample.last);
}

INSTANTIATE_TEST_SUITE_P(
    Ranges, ByteRangeTest,
    ::testing::Values(RangeCase{nullptr, 100U, mado::RangeStatus::kWhole, 0U, 99U},
                      RangeCase{"bytes=0-9", 100U, mado::RangeStatus::kPartial, 0U, 9U},
                      RangeCase{"BYTES=10-", 100U, mado::RangeStatus::kPartial, 10U, 99U},
                      RangeCase{"bytes=90-500", 100U, mado::RangeStatus::kPartial, 90U, 99U},
                      RangeCase{"bytes=-10", 100U, mado::RangeStatus::kPartial, 90U, 99U},
                      RangeCase{"bytes=-500", 100U, mado::RangeStatus::kPartial, 0U, 99U},
                      RangeCase{"bytes=100-", 100U, mado::RangeStatus::kUnsatisfiable, 0U, 99U},
                      RangeCase{"bytes=-0", 100U, mado::RangeStatus::kUnsatisfiable, 0U, 99U},
                      RangeCase{"bytes=0-0", 0U, mado::RangeStatus::kUnsatisfiable, 0U, 0U},
                      RangeCase{"bytes=99999999999-", 100U, mado::RangeStatus::kUnsatisfiable, 0U,
                                99U},
                      RangeCase{"bytes=0-99999999999", 100U, mado::RangeStatus::kPartial, 0U, 99U},
                      RangeCase{"bytes=-99999999999", 100U, mado::RangeStatus::kPartial, 0U, 99U},
                      RangeCase{"bytes=9-3", 100U, mado::RangeStatus::kWhole, 0U, 99U},
                      RangeCase{"bytes=0-1,5-6", 100U, mado::RangeStatus::kWhole, 0U, 99U},
                      RangeCase{"bytes=-", 100U, mado::RangeStatus::kWhole, 0U, 99U},
                      RangeCase{"bytes=a-b", 100U, mado::RangeStatus::kWhole, 0U, 99U},
                      RangeCase{"items=0-9", 100U, mado::RangeStatus::kWhole, 0U, 99U},
                      RangeCase{"bytes=5", 100U, mado::RangeStatus::kWhole, 0U, 99U}));

TEST(FileEtag, NamesThePathAndSizeAsAWeakTag) {
  // Arrange
  char tag[mado::kFileEtagSize] = {};

  // Act
  const bool made = mado::file_etag("/INDEX.HTM", mado::FileStamp{0x1234U, 7U}, tag);

  // Assert
  EXPECT_TRUE(made);
  EXPECT_EQ(std::string(&tag[0]).substr(0U, 3U), "W/\"");
  EXPECT_EQ(std::string(&tag[0]).substr(11U), "-00001234\"");
  EXPECT_EQ(std::string(&tag[0]).size(), 21U);
}

TEST(FileEtag, ChangesWithThePathAndWithTheSize) {
  // Arrange
  char first[mado::kFileEtagSize] = {};
  char other_path[mado::kFileEtagSize] = {};
  char other_size[mado::kFileEtagSize] = {};
  static_cast<void>(mado::file_etag("/A.TXT", mado::FileStamp{10U, 7U}, first));
  static_cast<void>(mado::file_etag("/B.TXT", mado::FileStamp{10U, 7U}, other_path));

  // Act
  const bool made = mado::file_etag("/A.TXT", mado::FileStamp{11U, 7U}, other_size);

  // Assert
  EXPECT_TRUE(made);
  EXPECT_NE(std::string(&first[0]), std::string(&other_path[0]));
  EXPECT_NE(std::string(&first[0]), std::string(&other_size[0]));
}

TEST(FileEtag, ChangesWithTheVersion) {
  // Arrange
  char before[mado::kFileEtagSize] = {};
  static_cast<void>(mado::file_etag("/A.TXT", mado::FileStamp{10U, 7U}, before));
  char after[mado::kFileEtagSize] = {};

  // Act
  const bool made = mado::file_etag("/A.TXT", mado::FileStamp{10U, 8U}, after);

  // Assert
  EXPECT_TRUE(made);
  EXPECT_NE(std::string(&before[0]), std::string(&after[0]));
}

TEST(FileEtag, RefusesAnEmptyPath) {
  // Arrange
  char tag[mado::kFileEtagSize] = {'x'};

  // Act
  const bool made = mado::file_etag("", mado::FileStamp{1U, 7U}, tag);

  // Assert
  EXPECT_FALSE(made);
  EXPECT_EQ(tag[0], '\0');
}

struct NameCase {
  const char* name;
  bool valid;
};

std::ostream& operator<<(std::ostream& stream, const NameCase& sample) {
  return stream << sample.name;
}

class ShortNameTest : public ::testing::TestWithParam<NameCase> {};

TEST_P(ShortNameTest, AcceptsOnlyEightDotThreeNames) {
  // Arrange
  const NameCase& sample = GetParam();

  // Act
  const bool valid = mado::valid_short_name(sample.name);

  // Assert
  EXPECT_EQ(valid, sample.valid);
}

INSTANTIATE_TEST_SUITE_P(Names, ShortNameTest,
                         ::testing::Values(NameCase{"INDEX.HTM", true}, NameCase{"a", true},
                                           NameCase{"LOG_1-2~.TXT", true}, NameCase{"README", true},
                                           NameCase{"ABCDEFGH.TXT", true},
                                           NameCase{"ABCDEFGHI.TXT", false},
                                           NameCase{"A.HTML", false}, NameCase{"A.", false},
                                           NameCase{".A", false}, NameCase{"", false},
                                           NameCase{"A.B.C", false}, NameCase{"A B.TXT", false},
                                           NameCase{"../X", false}, NameCase{"DIR/A.TXT", false},
                                           NameCase{"A%2F", false}));

TEST(ReprDigest, ReadsASha256DigestInBase64) {
  // Arrange
  const char* const header = "sha-256=:LPJNul+wow4m6DsqxbninhsWHlwfp0JecwQzYpOLmCQ=:";
  uint8_t digest[mado::kSha256Size] = {};

  // Act
  const bool parsed = mado::parse_repr_digest(header, digest);

  // Assert
  EXPECT_TRUE(parsed);
  EXPECT_EQ(digest[0], 0x2CU);
  EXPECT_EQ(digest[1], 0xF2U);
  EXPECT_EQ(digest[31], 0x24U);
}

TEST(ReprDigest, FindsTheShaEntryAmongOthers) {
  // Arrange
  const char* const header =
      "sha-512=:AAAA:, SHA-256=:LPJNul+wow4m6DsqxbninhsWHlwfp0JecwQzYpOLmCQ=:";
  uint8_t digest[mado::kSha256Size] = {};

  // Act
  const bool parsed = mado::parse_repr_digest(header, digest);

  // Assert
  EXPECT_TRUE(parsed);
  EXPECT_EQ(digest[31], 0x24U);
}

class ReprDigestRejectTest : public ::testing::TestWithParam<const char*> {};

TEST_P(ReprDigestRejectTest, RefusesAnythingButOne32ByteDigest) {
  // Arrange
  const char* const header = GetParam();
  uint8_t digest[mado::kSha256Size] = {};

  // Act
  const bool parsed = mado::parse_repr_digest(header, digest);

  // Assert
  EXPECT_FALSE(parsed);
}

INSTANTIATE_TEST_SUITE_P(
    Headers, ReprDigestRejectTest,
    ::testing::Values("md5=:AAAA:", "sha-256=:AAAA:",
                      "sha-256=:LPJNul+wow4m6DsqxbninhsWHlwfp0JecwQzYpOLmC*=:",
                      "xsha-256=:LPJNul+wow4m6DsqxbninhsWHlwfp0JecwQzYpOLmCQ=:", ""));

TEST(ReprDigest, RefusesAMissingHeader) {
  // Arrange
  uint8_t digest[mado::kSha256Size] = {};

  // Act
  const bool parsed = mado::parse_repr_digest(nullptr, digest);

  // Assert
  EXPECT_FALSE(parsed);
}

TEST(CopyFile, CopiesEveryByte) {
  // Arrange
  FakeData data(20261114U);
  const std::string content = data.word(150);
  FakeFile from(content);
  FakeFile target("");

  // Act
  const bool copied = mado::copy_file(from, target, static_cast<uint32_t>(content.size()));

  // Assert
  EXPECT_TRUE(copied);
  EXPECT_EQ(target.written(), content);
}

TEST(CopyFile, ReportsAShortSourceAndAFullTarget) {
  // Arrange
  FakeData data(20261115U);
  const std::string content = data.word(100);
  FakeFile short_source(content.substr(0U, 40U));
  FakeFile target("");
  FakeFile source(content);
  FakeFile full_target("", 10U);
  const bool short_copied = mado::copy_file(short_source, target, 100U);

  // Act
  const bool full_copied = mado::copy_file(source, full_target, 100U);

  // Assert
  EXPECT_FALSE(short_copied);
  EXPECT_FALSE(full_copied);
}

TEST(CopyFile, RefusesAClosedFile) {
  // Arrange
  FakeFile from("abc");
  FakeFile target("");
  target.close_file();

  // Act
  const bool copied = mado::copy_file(from, target, 3U);

  // Assert
  EXPECT_FALSE(copied);
}

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

std::string send(const std::string& content, const mado::FileReply& reply, const char* range,
                 const char* if_none_match, const mado::Method method = mado::Method::kGet) {
  ExchangeHarness harness("", 0U, method);
  if (range != nullptr) {
    harness.capture(file_headers(), 0U, range);
  }
  if (if_none_match != nullptr) {
    harness.capture(file_headers(), 1U, if_none_match);
  }
  if ((range == nullptr) && (if_none_match == nullptr)) {
    harness.capture(file_headers(), 0U, "");
  }
  TestExchange exchange = harness.make();
  FakeFile file(content);
  const bool sent = mado::send_file(exchange, file, reply);
  return (sent ? "sent\n" : "failed\n") + harness.sent();
}

std::string send_if_range(const mado::FileReply& reply, const char* if_range) {
  ExchangeHarness harness("", 0U, mado::Method::kGet);
  harness.capture(file_headers(), 0U, "bytes=2-5");
  harness.capture(file_headers(), 2U, if_range);
  TestExchange exchange = harness.make();
  FakeFile file("0123456789");
  const bool sent = mado::send_file(exchange, file, reply);
  return (sent ? "sent\n" : "failed\n") + harness.sent();
}

TEST(SendFileReply, SendsTheWholeFileWhenIfRangeNamesAWeakTag) {
  // Arrange
  const mado::FileReply reply = {10U, F("text/plain"), false, "W/\"abc\"", nullptr, nullptr};

  // Act
  const std::string response = send_if_range(reply, "W/\"abc\"");

  // Assert
  EXPECT_EQ(response.substr(0U, 20U), "sent\nHTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), "0123456789");
}

TEST(SendFileReply, SendsThePartWhenIfRangeMatchesAStrongTag) {
  // Arrange
  const mado::FileReply reply = {10U, F("text/plain"), false, "\"abc\"", nullptr, nullptr};

  // Act
  const std::string response = send_if_range(reply, "\"abc\"");

  // Assert
  EXPECT_EQ(response.substr(0U, 33U), "sent\nHTTP/1.1 206 Partial Content");
  EXPECT_EQ(body_of(response), "2345");
  EXPECT_EQ(body_of(send_if_range(reply, "\"old\"")), "0123456789");
}

TEST(SendFileReply, SendsTheWholeFileWithRangesAdvertised) {
  // Arrange
  const mado::FileReply reply = {10U, F("text/plain"), false, nullptr, nullptr, nullptr};

  // Act
  const std::string response = send("0123456789", reply, nullptr, nullptr);

  // Assert
  EXPECT_EQ(response,
            "sent\nHTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\n"
            "Content-Length: 10\r\nAccept-Ranges: bytes\r\n\r\n0123456789");
}

TEST(SendFileReply, SendsOnePartOfTheFile) {
  // Arrange
  const mado::FileReply reply = {10U, F("text/plain"), false, nullptr, nullptr, nullptr};

  // Act
  const std::string response = send("0123456789", reply, "bytes=2-5", nullptr);

  // Assert
  EXPECT_EQ(
      response,
      "sent\nHTTP/1.1 206 Partial Content\r\nConnection: close\r\nContent-Type: text/plain\r\n"
      "Content-Length: 4\r\nAccept-Ranges: bytes\r\nContent-Range: bytes 2-5/10\r\n\r\n2345");
}

TEST(SendFileReply, RefusesARangePastTheEnd) {
  // Arrange
  const mado::FileReply reply = {10U, F("text/plain"), false, nullptr, nullptr, nullptr};

  // Act
  const std::string response = send("0123456789", reply, "bytes=10-", nullptr);

  // Assert
  EXPECT_EQ(response,
            "sent\nHTTP/1.1 416 Range Not Satisfiable\r\nConnection: close\r\n"
            "Content-Range: bytes */10\r\nContent-Length: 0\r\n\r\n");
}

TEST(SendFileReply, AnswersNotModifiedForAMatchingTag) {
  // Arrange
  const mado::FileReply reply = {10U, F("text/plain"), false, "W/\"abc\"", nullptr, nullptr};

  // Act
  const std::string response = send("0123456789", reply, nullptr, "W/\"abc\"");

  // Assert
  EXPECT_EQ(response,
            "sent\nHTTP/1.1 304 Not Modified\r\nConnection: close\r\nETag: W/\"abc\"\r\n"
            "Cache-Control: no-cache\r\nContent-Length: 0\r\n\r\n");
}

TEST(SendFileReply, SendsTheTagAndADownloadName) {
  // Arrange
  const mado::FileReply reply = {3U, F("text/plain"), true, "W/\"abc\"", "LOG.TXT", nullptr};

  // Act
  const std::string response = send("abc", reply, nullptr, "\"other\"");

  // Assert
  EXPECT_EQ(response,
            "sent\nHTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\n"
            "Content-Length: 3\r\nAccept-Ranges: bytes\r\nContent-Encoding: gzip\r\n"
            "ETag: W/\"abc\"\r\nCache-Control: no-cache\r\n"
            "Content-Disposition: attachment; filename=\"LOG.TXT\"\r\n\r\nabc");
}

TEST(SendFileReply, LeavesOutAnUnsafeDownloadName) {
  // Arrange
  const mado::FileReply reply = {3U, F("text/plain"), false, nullptr, "a\"b.txt", nullptr};

  // Act
  const std::string response = send("abc", reply, nullptr, nullptr);

  // Assert
  EXPECT_NE(response.find("Content-Disposition: attachment\r\n"), std::string::npos);
  EXPECT_EQ(response.find("filename"), std::string::npos);
}

TEST(SendFileReply, SendsOnlyHeadersForHead) {
  // Arrange
  const mado::FileReply reply = {10U, F("text/plain"), false, nullptr, nullptr, nullptr};

  // Act
  const std::string response = send("0123456789", reply, "bytes=0-1", nullptr, mado::Method::kHead);

  // Assert
  EXPECT_EQ(body_of(response), "");
  EXPECT_NE(response.find("206 Partial Content"), std::string::npos);
}

TEST(SendFileReply, RefusesAMissingContentType) {
  // Arrange
  const mado::FileReply reply = {10U, nullptr, false, nullptr, nullptr, nullptr};

  // Act
  const std::string response = send("0123456789", reply, nullptr, nullptr);

  // Assert
  EXPECT_EQ(response, "failed\n");
}

TEST(SendFileReply, ReportsARangeTheFileCannotSeekTo) {
  // Arrange
  const mado::FileReply reply = {10U, F("text/plain"), false, nullptr, nullptr, nullptr};

  // Act
  const std::string response = send("01", reply, "bytes=5-6", nullptr);

  // Assert
  EXPECT_EQ(response.substr(0U, 7U), "failed\n");
}

}
