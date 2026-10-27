#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>

#include "ExchangeHarness.h"
#include "FakeData.h"
#include "mado/BodySource.h"
#include "mado/JsonReader.h"
#include "mado/JsonStatus.h"
#include "mado/MemorySource.h"
#include "mado/Method.h"
#include "mado/json_write_string.h"

namespace {

using MemoryReader = mado::JsonReader<mado::MemorySource>;

struct Parsed {
  mado::JsonStatus status = mado::JsonStatus::kOk;
  std::string name;
  int32_t value = 0;
  bool on = false;
};

mado::JsonStatus read_member(MemoryReader& reader, const std::string& key, Parsed& parsed) {
  char text[16] = {};
  if (key == "name") {
    const mado::JsonStatus status = reader.read_string(&text[0], sizeof(text));
    parsed.name = &text[0];
    return status;
  }
  if (key == "value") {
    return reader.read_integer(parsed.value);
  }
  return (key == "on") ? reader.read_boolean(parsed.on) : mado::JsonStatus::kUnsupported;
}

Parsed parse(const std::string& json) {
  mado::MemorySource source(reinterpret_cast<const uint8_t*>(json.data()),
                            static_cast<uint16_t>(json.size()));
  MemoryReader reader(source);
  Parsed parsed{reader.begin_object(), "", 0, false};
  for (int i = 0; (i < 8) && (parsed.status == mado::JsonStatus::kOk); ++i) {
    char key[8] = {};
    const mado::JsonStatus next = reader.next_key(&key[0], sizeof(key));
    if (next == mado::JsonStatus::kEnd) {
      parsed.status = reader.finish();
      break;
    }
    parsed.status = (next == mado::JsonStatus::kOk) ? read_member(reader, &key[0], parsed) : next;
  }
  return parsed;
}

TEST(JsonReader, ReadsEveryFieldType) {
  // Arrange
  const std::string json = R"( { "name" : "lamp", "value": -42, "on": true } )";

  // Act
  const Parsed parsed = parse(json);

  // Assert
  EXPECT_EQ(parsed.status, mado::JsonStatus::kOk);
  EXPECT_EQ(parsed.name, "lamp");
  EXPECT_EQ(parsed.value, -42);
  EXPECT_TRUE(parsed.on);
}

TEST(JsonReader, ReadsAnEmptyObject) {
  // Arrange
  const std::string json = "{}";

  // Act
  const Parsed parsed = parse(json);

  // Assert
  EXPECT_EQ(parsed.status, mado::JsonStatus::kOk);
}

TEST(JsonReader, ReadsGeneratedStrings) {
  // Arrange
  FakeData data(20261026U);
  const std::string word = data.word(12U);
  const std::string json = R"({"name":")" + word + R"("})";

  // Act
  const Parsed parsed = parse(json);

  // Assert
  EXPECT_EQ(parsed.status, mado::JsonStatus::kOk);
  EXPECT_EQ(parsed.name, word);
}

struct EscapeCase {
  std::string json;
  std::string expected;
};

class JsonEscapeTest : public ::testing::TestWithParam<EscapeCase> {};

TEST_P(JsonEscapeTest, DecodesEscapes) {
  // Arrange
  const EscapeCase& sample = GetParam();

  // Act
  const Parsed parsed = parse(sample.json);

  // Assert
  EXPECT_EQ(parsed.status, mado::JsonStatus::kOk);
  EXPECT_EQ(parsed.name, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(Strings, JsonEscapeTest,
                         ::testing::Values(EscapeCase{R"({"name":"a\"b"})", "a\"b"},
                                           EscapeCase{R"({"name":"a\\b"})", "a\\b"},
                                           EscapeCase{R"({"name":"a\/b"})", "a/b"},
                                           EscapeCase{R"({"name":"\b\f\n\r\t"})", "\b\f\n\r\t"},
                                           EscapeCase{R"({"name":"\u0041"})", "A"},
                                           EscapeCase{R"({"name":"\u00e9"})", "\xC3\xA9"},
                                           EscapeCase{R"({"name":"\u20AC"})", "\xE2\x82\xAC"},
                                           EscapeCase{"{\"name\":\"\xC3\xA9\xF0\x9F\x92\xA1\"}",
                                                      "\xC3\xA9\xF0\x9F\x92\xA1"}));

struct RejectCase {
  std::string json;
  mado::JsonStatus expected = mado::JsonStatus::kOk;
};

class JsonRejectTest : public ::testing::TestWithParam<RejectCase> {};

TEST_P(JsonRejectTest, ReportsTheFailure) {
  // Arrange
  const RejectCase& sample = GetParam();

  // Act
  const Parsed parsed = parse(sample.json);

  // Assert
  EXPECT_EQ(parsed.status, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Malformed, JsonRejectTest,
    ::testing::Values(RejectCase{"", mado::JsonStatus::kTruncated},
                      RejectCase{"[]", mado::JsonStatus::kWrongType},
                      RejectCase{"{", mado::JsonStatus::kTruncated},
                      RejectCase{R"({"name":"lamp")", mado::JsonStatus::kTruncated},
                      RejectCase{R"({"name":"lamp",})", mado::JsonStatus::kSyntax},
                      RejectCase{R"({,"name":"lamp"})", mado::JsonStatus::kSyntax},
                      RejectCase{R"({"name" "lamp"})", mado::JsonStatus::kSyntax},
                      RejectCase{R"({"name":"lamp" "value":1})", mado::JsonStatus::kSyntax},
                      RejectCase{R"({"name":"lamp"} x)", mado::JsonStatus::kSyntax},
                      RejectCase{R"({"name":"aaaaaaaaaaaaaaaa"})", mado::JsonStatus::kTooLong},
                      RejectCase{R"({"toolongkey":1})", mado::JsonStatus::kTooLong},
                      RejectCase{R"({"name":3})", mado::JsonStatus::kWrongType},
                      RejectCase{R"({"value":"3"})", mado::JsonStatus::kWrongType},
                      RejectCase{R"({"value":1.5})", mado::JsonStatus::kWrongType},
                      RejectCase{R"({"value":1e3})", mado::JsonStatus::kWrongType},
                      RejectCase{R"({"value":01})", mado::JsonStatus::kSyntax},
                      RejectCase{R"({"value":-})", mado::JsonStatus::kWrongType},
                      RejectCase{R"({"value":12x})", mado::JsonStatus::kSyntax},
                      RejectCase{R"({"value":2147483648})", mado::JsonStatus::kOutOfRange},
                      RejectCase{R"({"value":-2147483649})", mado::JsonStatus::kOutOfRange},
                      RejectCase{R"({"value":99999999999})", mado::JsonStatus::kOutOfRange},
                      RejectCase{R"({"on":1})", mado::JsonStatus::kWrongType},
                      RejectCase{R"({"on":tru})", mado::JsonStatus::kSyntax},
                      RejectCase{R"({"on":truex})", mado::JsonStatus::kSyntax},
                      RejectCase{R"({"on":null})", mado::JsonStatus::kWrongType},
                      RejectCase{R"({"name":"\x"})", mado::JsonStatus::kSyntax},
                      RejectCase{R"({"name":"\u00"})", mado::JsonStatus::kSyntax},
                      RejectCase{R"({"name":"\u0000"})", mado::JsonStatus::kUnsupported},
                      RejectCase{R"({"name":"\uD83D"})", mado::JsonStatus::kUnsupported},
                      RejectCase{"{\"name\":\"a\nb\"}", mado::JsonStatus::kSyntax},
                      RejectCase{"{\"name\":\"\xC3\"}", mado::JsonStatus::kSyntax},
                      RejectCase{"{\"name\":\"\xC3\x41\"}", mado::JsonStatus::kSyntax},
                      RejectCase{"{\"name\":\"\xC0\xAF\"}", mado::JsonStatus::kSyntax},
                      RejectCase{"{\"name\":\"\xFF\"}", mado::JsonStatus::kSyntax},
                      RejectCase{R"({"name":"\u20AC\u20AC\u20AC\u20AC\u20AC\u20AC"})",
                                 mado::JsonStatus::kTooLong}));

TEST(JsonReader, IntegerBoundsAreInclusive) {
  // Arrange
  const std::string json = R"({"value":-2147483648})";

  // Act
  const Parsed parsed = parse(json);

  // Assert
  EXPECT_EQ(parsed.status, mado::JsonStatus::kOk);
  EXPECT_EQ(parsed.value, INT32_MIN);
}

TEST(JsonReader, ReadsTheLargestPositiveInteger) {
  // Arrange
  const std::string json = R"({"value":2147483647})";

  // Act
  const Parsed parsed = parse(json);

  // Assert
  EXPECT_EQ(parsed.status, mado::JsonStatus::kOk);
  EXPECT_EQ(parsed.value, INT32_MAX);
}

TEST(JsonReader, ReadsZeroAndNegativeZero) {
  // Arrange
  const std::string json = R"({"value":-0})";

  // Act
  const Parsed parsed = parse(json);

  // Assert
  EXPECT_EQ(parsed.status, mado::JsonStatus::kOk);
  EXPECT_EQ(parsed.value, 0);
}

TEST(JsonReader, KeepsReportingTheFirstError) {
  // Arrange
  const std::string json = R"({"value":"x"})";
  mado::MemorySource source(reinterpret_cast<const uint8_t*>(json.data()),
                            static_cast<uint16_t>(json.size()));
  MemoryReader reader(source);
  char key[8] = {};
  static_cast<void>(reader.begin_object());
  static_cast<void>(reader.next_key(&key[0], sizeof(key)));
  int32_t value = 0;
  static_cast<void>(reader.read_integer(value));

  // Act
  const mado::JsonStatus status = reader.finish();

  // Assert
  EXPECT_EQ(status, mado::JsonStatus::kWrongType);
}

TEST(JsonReader, RefusesAValueBeforeAKey) {
  // Arrange
  const std::string json = R"({"value":1})";
  mado::MemorySource source(reinterpret_cast<const uint8_t*>(json.data()),
                            static_cast<uint16_t>(json.size()));
  MemoryReader reader(source);
  static_cast<void>(reader.begin_object());
  bool flag = false;

  // Act
  const mado::JsonStatus status = reader.read_boolean(flag);

  // Assert
  EXPECT_EQ(status, mado::JsonStatus::kSyntax);
}

TEST(JsonReader, RefusesAMissingKeyBuffer) {
  // Arrange
  const std::string json = R"({"value":1})";
  mado::MemorySource source(reinterpret_cast<const uint8_t*>(json.data()),
                            static_cast<uint16_t>(json.size()));
  MemoryReader reader(source);
  static_cast<void>(reader.begin_object());

  // Act
  const mado::JsonStatus status = reader.next_key(nullptr, 8U);

  // Assert
  EXPECT_EQ(status, mado::JsonStatus::kSyntax);
}

TEST(JsonReader, RefusesASecondBegin) {
  // Arrange
  const std::string json = "{}";
  mado::MemorySource source(reinterpret_cast<const uint8_t*>(json.data()),
                            static_cast<uint16_t>(json.size()));
  MemoryReader reader(source);
  static_cast<void>(reader.begin_object());

  // Act
  const mado::JsonStatus status = reader.begin_object();

  // Assert
  EXPECT_EQ(status, mado::JsonStatus::kSyntax);
}

TEST(JsonReader, RefusesToFinishAnOpenObject) {
  // Arrange
  const std::string json = R"({"value":1})";
  mado::MemorySource source(reinterpret_cast<const uint8_t*>(json.data()),
                            static_cast<uint16_t>(json.size()));
  MemoryReader reader(source);
  static_cast<void>(reader.begin_object());

  // Act
  const mado::JsonStatus status = reader.finish();

  // Assert
  EXPECT_EQ(status, mado::JsonStatus::kSyntax);
}

TEST(MemorySource, TreatsAMissingBufferAsEmpty) {
  // Arrange
  mado::MemorySource source(nullptr, 4U);

  // Act
  const int16_t symbol = source.next();

  // Assert
  EXPECT_EQ(symbol, -1);
}

TEST(BodySource, ReadsTheBodyAcrossChunks) {
  // Arrange
  const std::string body = R"({"name":"a fairly long lamp name","value":7})";
  ExchangeHarness harness(body, static_cast<uint32_t>(body.size()), mado::Method::kPost);
  HarnessExchange exchange = harness.make();
  mado::BodySource<HarnessExchange> source(exchange);
  std::string seen;
  for (size_t i = 0U; i < body.size(); ++i) {
    seen.push_back(static_cast<char>(source.next()));
  }

  // Act
  const int16_t after = source.next();

  // Assert
  EXPECT_EQ(seen, body);
  EXPECT_EQ(after, -1);
}

TEST(BodySource, StaysAtTheEndWhenAskedAgain) {
  // Arrange
  const std::string body = "{}";
  ExchangeHarness harness(body, 2U, mado::Method::kPost);
  HarnessExchange exchange = harness.make();
  mado::BodySource<HarnessExchange> source(exchange);
  static_cast<void>(source.next());
  static_cast<void>(source.next());
  static_cast<void>(source.next());

  // Act
  const int16_t after = source.next();

  // Assert
  EXPECT_EQ(after, -1);
}

TEST(BodySource, StopsAtTheDeclaredLength) {
  // Arrange
  const std::string body = "{}extra";
  ExchangeHarness harness(body, 2U, mado::Method::kPost);
  HarnessExchange exchange = harness.make();
  mado::BodySource<HarnessExchange> source(exchange);
  static_cast<void>(source.next());
  static_cast<void>(source.next());

  // Act
  const int16_t after = source.next();

  // Assert
  EXPECT_EQ(after, -1);
}

TEST(JsonWriteString, EscapesQuotesBackslashesAndControls) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool written = mado::json_write_string(exchange, "a\"b\\c\n\x01\xC3\xA9");

  // Assert
  EXPECT_TRUE(written);
  EXPECT_EQ(harness.sent(), "\"a\\\"b\\\\c\\u000a\\u0001\xC3\xA9\"");
}

TEST(JsonWriteString, WritesAnEmptyString) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool written = mado::json_write_string(exchange, "");

  // Assert
  EXPECT_TRUE(written);
  EXPECT_EQ(harness.sent(), "\"\"");
}

TEST(JsonWriteString, RejectsAMissingString) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool written = mado::json_write_string(exchange, nullptr);

  // Assert
  EXPECT_FALSE(written);
  EXPECT_EQ(harness.sent(), "");
}

TEST(JsonWriteString, ReportsAFailedOutput) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  harness.detach_output();

  // Act
  const bool written = mado::json_write_string(exchange, "lamp");

  // Assert
  EXPECT_FALSE(written);
}

}
