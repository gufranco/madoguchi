#include <Arduino.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <cstddef>
#include <string>

#include "FakeData.h"
#include "FakeSd.h"
#include "mado/FileSink.h"
#include "mado/FileSource.h"
#include "mado/LogEntry.h"
#include "mado/Method.h"
#include "mado/SdLog.h"
#include "mado/SlotTable.h"
#include "mado/count_file.h"
#include "mado/method_name.h"
#include "mado/replace_file.h"
#include "mado/write_log_line.h"

namespace {

using Log = mado::SdLog<FakeSd, FILE_READ, FILE_WRITE>;

const char kLogPath[] PROGMEM = "/LOG.TXT";
const char kOldPath[] PROGMEM = "/LOG.OLD";

std::string flash_text(const __FlashStringHelper* text) {
  return reinterpret_cast<const char*>(text);
}

mado::LogStamp stamp() {
  return mado::LogStamp{3U, 1500U, F("board")};
}

TEST(FileSink, WritesTextNumbersAndFlashStrings) {
  // Arrange
  FakeSd card;
  FakeSdFile file = card.open("/A.TXT", FILE_WRITE);
  mado::FileSink<FakeSdFile> sink(file);

  // Act
  const size_t written = sink.print(F("n=")) + sink.print(static_cast<uint32_t>(42U)) +
                         sink.print(" ") + sink.print_signed(-7) + sink.write('\n');

  // Assert
  EXPECT_EQ(written, 8U);
  EXPECT_TRUE(sink.ok());
  EXPECT_EQ(card.files().at("/A.TXT"), "n=42 -7\n");
}

TEST(FileSink, StopsAfterTheCardRefusesAWrite) {
  // Arrange
  FakeSd card;
  card.limit_writes(3U);
  FakeSdFile file = card.open("/A.TXT", FILE_WRITE);
  mado::FileSink<FakeSdFile> sink(file);

  // Act
  const size_t written = sink.print(F("abcdef"));

  // Assert
  EXPECT_EQ(written, 0U);
  EXPECT_FALSE(sink.ok());
  EXPECT_EQ(sink.print("x"), 0U);
}

TEST(FileSource, ReadsEveryByteThenEnds) {
  // Arrange
  FakeSd card;
  card.files()["/A.TXT"] = "ab";
  FakeSdFile file = card.open("/A.TXT", FILE_READ);
  mado::FileSource<FakeSdFile> source(file);
  const int16_t first = source.next();

  // Act
  const int16_t second = source.next();

  // Assert
  EXPECT_EQ(first, 'a');
  EXPECT_EQ(second, 'b');
  EXPECT_EQ(source.next(), -1);
  EXPECT_EQ(source.next(), -1);
}

TEST(WriteRequestLog, WritesAPinoHttpShapedLine) {
  // Arrange
  FakeSd card;
  FakeSdFile file = card.open("/LOG.TXT", FILE_WRITE);
  mado::FileSink<FakeSdFile> sink(file);
  const mado::RequestLog entry = {12U, mado::Method::kPut, "/api/items/2", 200U, 9U};

  // Act
  const bool written = mado::write_request_log(sink, stamp(), entry);

  // Assert
  EXPECT_TRUE(written);
  EXPECT_EQ(card.files().at("/LOG.TXT"),
            R"({"level":30,"boot":3,"uptime":1500,"hostname":"board","req":{"id":12,)"
            R"("method":"PUT","url":"/api/items/2"},"res":{"statusCode":200},)"
            R"("responseTime":9,"msg":"request completed"})"
            "\n");
}

TEST(WriteRequestLog, MarksServerErrorsAsErrored) {
  // Arrange
  FakeSd card;
  FakeSdFile file = card.open("/LOG.TXT", FILE_WRITE);
  mado::FileSink<FakeSdFile> sink(file);
  const mado::RequestLog entry = {1U, mado::Method::kGet, "/", 503U, 0U};

  // Act
  const bool written = mado::write_request_log(sink, stamp(), entry);

  // Assert
  const std::string& line = card.files().at("/LOG.TXT");
  EXPECT_TRUE(written);
  EXPECT_EQ(line.substr(0U, 11U), R"({"level":50)");
  EXPECT_NE(line.find(R"("msg":"request errored"})"), std::string::npos);
}

TEST(WriteEventLog, WritesALevelAndAMessage) {
  // Arrange
  FakeSd card;
  FakeSdFile file = card.open("/LOG.TXT", FILE_WRITE);
  mado::FileSink<FakeSdFile> sink(file);

  // Act
  const bool written = mado::write_event_log(sink, stamp(), mado::kLogWarn, F("login failed"));

  // Assert
  EXPECT_TRUE(written);
  EXPECT_EQ(card.files().at("/LOG.TXT"),
            R"({"level":40,"boot":3,"uptime":1500,"hostname":"board","msg":"login failed"})"
            "\n");
}

TEST(MethodName, NamesEveryMethod) {
  // Arrange
  const mado::Method patch = mado::Method::kPatch;

  // Act
  const std::string name = flash_text(mado::method_name(patch));

  // Assert
  EXPECT_EQ(name, "PATCH");
  EXPECT_EQ(flash_text(mado::method_name(mado::Method::kOptions)), "OPTIONS");
  EXPECT_EQ(flash_text(mado::method_name(mado::Method::kUnknown)), "UNKNOWN");
  EXPECT_EQ(flash_text(mado::method_name(mado::Method::kAny)), "UNKNOWN");
}

TEST(ReplaceFile, MovesTheSourceOverTheTarget) {
  // Arrange
  FakeData data(20261117U);
  const std::string content = data.word(90U);
  FakeSd card;
  card.files()["/NEW.TMP"] = content;
  card.files()["/OLD.TXT"] = "stale";

  // Act
  const bool replaced = mado::replace_file<FILE_READ, FILE_WRITE>(card, "/NEW.TMP", "/OLD.TXT");

  // Assert
  EXPECT_TRUE(replaced);
  EXPECT_EQ(card.files().at("/OLD.TXT"), content);
  EXPECT_EQ(card.files().count("/NEW.TMP"), 0U);
}

TEST(ReplaceFile, RefusesAMissingSource) {
  // Arrange
  FakeSd card;
  card.files()["/OLD.TXT"] = "kept";

  // Act
  const bool replaced = mado::replace_file<FILE_READ, FILE_WRITE>(card, "/NONE.TMP", "/OLD.TXT");

  // Assert
  EXPECT_FALSE(replaced);
  EXPECT_EQ(card.files().at("/OLD.TXT"), "kept");
}

TEST(CountFile, RoundTripsACount) {
  // Arrange
  FakeSd card;
  const bool written = mado::write_count_file<FILE_READ, FILE_WRITE>(card, "/C.TXT", "/C.TMP", 4U);

  // Act
  const uint8_t value = mado::read_count_file<FILE_READ>(card, "/C.TXT");

  // Assert
  EXPECT_TRUE(written);
  EXPECT_EQ(value, 4U);
  EXPECT_EQ(card.files().at("/C.TXT"), "4\n");
  EXPECT_EQ(card.files().count("/C.TMP"), 0U);
}

TEST(CountFile, ReportsAMissingOrUnreadableCount) {
  // Arrange
  FakeSd card;
  card.files()["/BAD.TXT"] = "x\n";

  // Act
  const uint8_t missing = mado::read_count_file<FILE_READ>(card, "/C.TXT");

  // Assert
  EXPECT_EQ(missing, mado::kNoCount);
  EXPECT_EQ(mado::read_count_file<FILE_READ>(card, "/BAD.TXT"), mado::kNoCount);
  EXPECT_FALSE(
      (mado::write_count_file<FILE_READ, FILE_WRITE>(card, "/C.TXT", "/C.TMP", mado::kNoCount)));
}

TEST(SdLog, AppendsLinesAndRotatesPastTheLimit) {
  // Arrange
  FakeSd card;
  Log log(&kLogPath[0], &kOldPath[0], 100U);
  log.attach(&card);
  const mado::RequestLog entry = {1U, mado::Method::kGet, "/", 200U, 1U};
  const bool first = log.request(stamp(), entry);

  // Act
  const bool second = log.request(stamp(), entry);

  // Assert
  EXPECT_TRUE(first);
  EXPECT_TRUE(second);
  EXPECT_EQ(card.files().count("/LOG.OLD"), 1U);
  EXPECT_EQ(card.files().at("/LOG.TXT").find('\n'), card.files().at("/LOG.TXT").size() - 1U);
}

TEST(SdLog, ClearsBothFilesAndReportsAMissingCard) {
  // Arrange
  FakeSd card;
  card.files()["/LOG.TXT"] = "a\n";
  card.files()["/LOG.OLD"] = "b\n";
  Log log(&kLogPath[0], &kOldPath[0], 100U);
  Log detached(&kLogPath[0], &kOldPath[0], 100U);
  log.attach(&card);

  // Act
  const bool cleared = log.clear();

  // Assert
  EXPECT_TRUE(cleared);
  EXPECT_TRUE(card.files().empty());
  EXPECT_FALSE(detached.clear());
  EXPECT_FALSE(detached.event(stamp(), mado::kLogInfo, F("x")));
}

TEST(SlotTable, PlacesARecordInTheSlotItNames) {
  // Arrange
  mado::SlotTable<int, 4U> table;

  // Act
  const bool placed = table.place(3U, 9);

  // Assert
  EXPECT_TRUE(placed);
  ASSERT_NE(table.find(3U), nullptr);
  EXPECT_EQ(*table.find(3U), 9);
  EXPECT_FALSE(table.place(3U, 1));
  EXPECT_FALSE(table.place(5U, 1));
  EXPECT_EQ(table.insert(1), 1U);
}

}
