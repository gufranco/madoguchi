#include <Arduino.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <string.h>

#include <string>

#include "ExchangeHarness.h"
#include "mado/Base64UrlDecoder.h"
#include "mado/HmacSha256.h"
#include "mado/JsonReader.h"
#include "mado/JsonStatus.h"
#include "mado/KeyMemory.h"
#include "mado/LoginGuard.h"
#include "mado/MemorySource.h"
#include "mado/Method.h"
#include "mado/Pbkdf2Params.h"
#include "mado/Route.h"
#include "mado/Sha256.h"
#include "mado/SlotTable.h"
#include "mado/TokenClaims.h"
#include "mado/TokenStatus.h"
#include "mado/etag_matches.h"
#include "mado/issue_token.h"
#include "mado/kSha256Size.h"
#include "mado/parse_token_claims.h"
#include "mado/pbkdf2_sha256.h"
#include "mado/send_method_not_allowed.h"
#include "mado/verify_bearer.h"

namespace {

struct Slot {
  int32_t value = 0;
};

bool claims_parse(const std::string& payload) {
  mado::TokenClaims claims = {};
  return mado::parse_token_claims(reinterpret_cast<const uint8_t*>(payload.data()),
                                  static_cast<uint8_t>(payload.size()), claims);
}

class ClaimsRejectTest : public ::testing::TestWithParam<std::string> {};

TEST_P(ClaimsRejectTest, RejectsAnythingButTheIssuedShape) {
  // Arrange
  const std::string& payload = GetParam();

  // Act
  const bool parsed = claims_parse(payload);

  // Assert
  EXPECT_FALSE(parsed);
}

INSTANTIATE_TEST_SUITE_P(
    Payloads, ClaimsRejectTest,
    ::testing::Values(
        R"({"sub":"","ses":7,"end":900})", R"({"sub":"a b","ses":7,"end":900})",
        R"({"sub":"abcdefghijklmnopq","ses":7,"end":900})", R"({"sub":"admin","ses":x,"end":900})",
        R"({"sub":"admin","ses":007,"end":900})", R"({"sub":"admin","ses":2147483648,"end":900})",
        R"({"sub":"admin","ses":12345678901,"end":900})", R"({"sub":"admin","ses":7,"fin":900})",
        R"({"sub":"admin","ses":7,"end":})", R"({"sub":"admin","ses":7,"end":900} )",
        R"({"sub":"admin","ses":7,"end":900)", R"({"sub":"admin")", R"({"sub")", R"([])",
        R"({"sub":"a~","ses":7,"end":9})", R"({"sub":"a@b","ses":7,"end":9})",
        R"({"sub":"a`b","ses":7,"end":9})", R"({"sub":"abc)", R"({"sub":"a","ses":7)"));

TEST(ParseTokenClaims, AcceptsZeroAsAClaim) {
  // Arrange
  const std::string payload = R"({"sub":"Za.b-c_9","ses":0,"end":2147483647})";
  mado::TokenClaims claims = {};

  // Act
  const bool parsed = mado::parse_token_claims(reinterpret_cast<const uint8_t*>(payload.data()),
                                               static_cast<uint8_t>(payload.size()), claims);

  // Assert
  EXPECT_TRUE(parsed);
  EXPECT_STREQ(&claims.subject[0], "Za.b-c_9");
  EXPECT_EQ(claims.session, 0U);
  EXPECT_EQ(claims.ends_at, 2147483647U);
}

TEST(ParseTokenClaims, RejectsAMissingPayload) {
  // Arrange
  mado::TokenClaims claims = {};

  // Act
  const bool parsed = mado::parse_token_claims(nullptr, 10U, claims);

  // Assert
  EXPECT_FALSE(parsed);
}

TEST(ParseTokenClaims, RejectsAnEmptyPayload) {
  // Arrange
  const uint8_t payload[] = {'{'};
  mado::TokenClaims claims = {};

  // Act
  const bool parsed = mado::parse_token_claims(&payload[0], 0U, claims);

  // Assert
  EXPECT_FALSE(parsed);
}

const mado::Route kMixedRoutes[] = {
    {"/a", mado::Method::kPut, 0U},     {"/a", mado::Method::kOptions, 1U},
    {"/a", mado::Method::kUnknown, 2U}, {"/b", mado::Method::kAny, 3U},
    {"/a", mado::Method::kPatch, 4U},   {nullptr, mado::Method::kAny, 0U},
};

std::string allow_header(const char* path, const mado::Route* routes) {
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  static_cast<void>(mado::send_method_not_allowed<16U>(exchange, routes, path));
  const std::string sent = harness.sent();
  const size_t start = sent.find("Allow: ");
  return (start == std::string::npos)
             ? "<none>"
             : sent.substr(start + 7U, sent.find('\r', start) - start - 7U);
}

TEST(SendMethodNotAllowed, SkipsAnyAndUnknownRoutes) {
  // Arrange
  const char* const path = "/a";

  // Act
  const std::string allow = allow_header(path, &kMixedRoutes[0]);

  // Assert
  EXPECT_EQ(allow, "PUT, PATCH, OPTIONS");
}

TEST(SendMethodNotAllowed, SendsAnEmptyListForAMissingTable) {
  // Arrange
  const mado::Route* const routes = nullptr;

  // Act
  const std::string allow = allow_header("/a", routes);

  // Assert
  EXPECT_EQ(allow, "");
}

TEST(SendMethodNotAllowed, SendsAnEmptyListForAMissingPath) {
  // Arrange
  const char* const path = nullptr;

  // Act
  const std::string allow = allow_header(path, &kMixedRoutes[0]);

  // Assert
  EXPECT_EQ(allow, "");
}

TEST(SendMethodNotAllowed, ListsEveryMethodWhenAllAreAllowed) {
  // Arrange
  const uint16_t every = 0xFFFFU;
  char list[mado::detail::kAllowListSize] = {};

  // Act
  const bool formatted = mado::detail::format_allow(every, &list[0], sizeof(list));

  // Assert
  EXPECT_TRUE(formatted);
  EXPECT_STREQ(&list[0], "GET, HEAD, POST, PUT, DELETE, PATCH, OPTIONS");
}

TEST(SendMethodNotAllowed, RefusesAListOfTheWrongSize) {
  // Arrange
  char list[8] = {};

  // Act
  const bool formatted = mado::detail::format_allow(0xFFFFU, &list[0], sizeof(list));

  // Assert
  EXPECT_FALSE(formatted);
}

TEST(SendMethodNotAllowed, ReportsAFailedOutput) {
  // Arrange
  ExchangeHarness harness;
  harness.detach_output();
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_method_not_allowed<16U>(exchange, &kMixedRoutes[0], "/a");

  // Assert
  EXPECT_FALSE(sent);
}

TEST(SlotTable, RefusesToEraseAnIdPastTheEnd) {
  // Arrange
  mado::SlotTable<Slot, 2U> table;
  static_cast<void>(table.insert(Slot{1}));

  // Act
  const bool erased = table.erase(3U);

  // Assert
  EXPECT_FALSE(erased);
  EXPECT_EQ(table.count(), 1U);
}

TEST(SlotTable, RefusesToEraseIdZero) {
  // Arrange
  mado::SlotTable<Slot, 2U> table;
  static_cast<void>(table.insert(Slot{1}));

  // Act
  const bool erased = table.erase(0U);

  // Assert
  EXPECT_FALSE(erased);
}

TEST(SlotTable, FindsNothingAtIdZero) {
  // Arrange
  mado::SlotTable<Slot, 2U> table;
  static_cast<void>(table.insert(Slot{1}));

  // Act
  const Slot* const found = table.find(0U);

  // Assert
  EXPECT_EQ(found, nullptr);
}

TEST(SlotTable, FindsAStoredValueThroughAConstTable) {
  // Arrange
  mado::SlotTable<Slot, 2U> table;
  const uint8_t slot_id = table.insert(Slot{41});
  const mado::SlotTable<Slot, 2U>& view = table;

  // Act
  const Slot* const found = view.find(slot_id);

  // Assert
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->value, 41);
}

TEST(LoginGuard, StaysLockedWhenTheClockRunsBackwards) {
  // Arrange
  mado::LoginGuard guard;
  for (int attempt = 0; attempt < 5; ++attempt) {
    guard.record_failure(1000U);
  }

  // Act
  const bool locked = guard.locked(10U);

  // Assert
  EXPECT_TRUE(locked);
  EXPECT_EQ(guard.retry_after(10U), mado::LoginGuard::kLockSeconds);
}

TEST(LoginGuard, IgnoresAFailureFromBeforeTheLock) {
  // Arrange
  mado::LoginGuard guard;
  for (int attempt = 0; attempt < 5; ++attempt) {
    guard.record_failure(1000U);
  }

  // Act
  guard.record_failure(10U);

  // Assert
  EXPECT_EQ(guard.retry_after(1000U), mado::LoginGuard::kLockSeconds);
}

TEST(LoginGuard, ClearsTheLockAfterASuccess) {
  // Arrange
  mado::LoginGuard guard;
  for (int attempt = 0; attempt < 5; ++attempt) {
    guard.record_failure(1000U);
  }

  // Act
  guard.record_success();

  // Assert
  EXPECT_FALSE(guard.locked(1001U));
}

struct TagCase {
  const char* header = nullptr;
  bool expected = false;
};

class EtagListTest : public ::testing::TestWithParam<TagCase> {};

TEST_P(EtagListTest, HandlesSpacingAndWeakTags) {
  // Arrange
  const TagCase& sample = GetParam();

  // Act
  const bool matched = mado::etag_matches(sample.header, F("\"ab\""));

  // Assert
  EXPECT_EQ(matched, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Lists, EtagListTest,
    ::testing::Values(TagCase{"  \"ab\"  ", true}, TagCase{"\"x\" , W/\"ab\" ", true},
                      TagCase{"W/\"x\", \"y\",", false}, TagCase{"  *  ", true},
                      TagCase{"*, \"x\"", false}, TagCase{"\"ab\"x", false}, TagCase{"W/", false},
                      TagCase{"Wx", false}, TagCase{"\"ab\" x", false}, TagCase{"\t\"ab\"", true}));

TEST(EtagMatches, RejectsAMissingTag) {
  // Arrange
  const __FlashStringHelper* const tag = nullptr;

  // Act
  const bool matched = mado::etag_matches("\"ab\"", tag);

  // Assert
  EXPECT_FALSE(matched);
}

using MemoryReader = mado::JsonReader<mado::MemorySource>;

class JsonMisuseTest : public ::testing::Test {
 public:
  MemoryReader& reader() {
    return reader_;
  }

 private:
  std::string json_ = R"({"key":"text","n":5})";
  mado::MemorySource source_{reinterpret_cast<const uint8_t*>(json_.data()),
                             static_cast<uint16_t>(json_.size())};
  MemoryReader reader_{source_};
};

TEST_F(JsonMisuseTest, RefusesAKeyBufferOfOneByte) {
  // Arrange
  static_cast<void>(reader().begin_object());
  char key[1] = {};

  // Act
  const mado::JsonStatus status = reader().next_key(&key[0], sizeof(key));

  // Assert
  EXPECT_EQ(status, mado::JsonStatus::kSyntax);
}

TEST_F(JsonMisuseTest, RefusesAStringIntoAMissingBuffer) {
  // Arrange
  static_cast<void>(reader().begin_object());
  char key[8] = {};
  static_cast<void>(reader().next_key(&key[0], sizeof(key)));

  // Act
  const mado::JsonStatus status = reader().read_string(nullptr, 4U);

  // Assert
  EXPECT_EQ(status, mado::JsonStatus::kSyntax);
}

TEST_F(JsonMisuseTest, RefusesAnIntegerBeforeAKey) {
  // Arrange
  static_cast<void>(reader().begin_object());
  int32_t number = 0;

  // Act
  const mado::JsonStatus status = reader().read_integer(number);

  // Assert
  EXPECT_EQ(status, mado::JsonStatus::kSyntax);
}

TEST_F(JsonMisuseTest, RefusesAStringBeforeTheObject) {
  // Arrange
  char text[8] = {};

  // Act
  const mado::JsonStatus status = reader().read_string(&text[0], sizeof(text));

  // Assert
  EXPECT_EQ(status, mado::JsonStatus::kSyntax);
}

const uint8_t kSigningKey[32] PROGMEM = {
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
};

class SignedRejectTest : public ::testing::TestWithParam<const char*> {};

TEST_P(SignedRejectTest, RejectsAMalformedSignedToken) {
  // Arrange
  const std::string authorization = std::string("Bearer ") + GetParam();
  mado::TokenClaims claims = {};

  // Act
  const mado::TokenStatus status =
      mado::verify_bearer(authorization.c_str(), &kSigningKey[0], 7U, 10U, claims);

  // Assert
  EXPECT_NE(status, mado::TokenStatus::kValid);
}

INSTANTIATE_TEST_SUITE_P(
    Tokens, SignedRejectTest,
    ::testing::Values(
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.ab*cd.1YMC6jnbO7ZU1jbF-QRVMvql1FMYr6_x8ZTwM9mnbeo",
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
        "eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDAsInBhZCI6Inh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eH"
        "h4eHh4eHh4eHh4eHgifQ.-vSuzPZQJ4n05qCFFjEbe86YZXT9Ddf4Qil9Zud_m68",
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDBR."
        "wO25e2lHVGfh06afhEUYC8UXuazlXJ_MHqViMXB9E10",
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDB9."
        "9PQ6D5Wpr82DfguGZcQy8Q8cfHT24Tg0qGQ3xAtxJD!",
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDB9."
        "9PQ6D5Wpr82DfguGZcQy8Q8cfHT24Tg0qGQ3xAtxJDQAAAA",
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJX.eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDB9."
        "9PQ6D5Wpr82DfguGZcQy8Q8cfHT24Tg0qGQ3xAtxJDQ"));

TEST(VerifyBearer, ExpiresWhenTheClockPassesTheSignedRange) {
  // Arrange
  const std::string authorization =
      "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
      "eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDB9.9PQ6D5Wpr82DfguGZcQy8Q8cfHT24Tg0qGQ3xAtxJDQ";
  mado::TokenClaims claims = {};

  // Act
  const mado::TokenStatus status =
      mado::verify_bearer(authorization.c_str(), &kSigningKey[0], 7U, 0x80000000U, claims);

  // Assert
  EXPECT_EQ(status, mado::TokenStatus::kExpired);
}

class IssueFailureTest : public ::testing::TestWithParam<size_t> {};

TEST_P(IssueFailureTest, ReportsAnOutputThatStopsPartWay) {
  // Arrange
  ExchangeHarness harness;
  harness.client().fail_writes_after(GetParam());
  harness.client().limit_write(1U);
  HarnessExchange exchange = harness.make();
  mado::TokenClaims claims = {};
  strncpy(&claims.subject[0], "admin", sizeof(claims.subject) - 1U);
  claims.session = 7U;
  claims.ends_at = 900U;

  // Act
  const bool issued = mado::issue_token(exchange, &kSigningKey[0], claims);

  // Assert
  EXPECT_FALSE(issued);
}

INSTANTIATE_TEST_SUITE_P(Offsets, IssueFailureTest,
                         ::testing::Values(0U, 10U, 37U, 60U, 82U, 100U, 124U));

TEST(HmacSha256, RefusesABlockBeforeAKey) {
  // Arrange
  mado::HmacSha256 mac;
  const uint8_t data[] = {1U, 2U};

  // Act
  const bool accepted = mac.update(&data[0], sizeof(data));

  // Assert
  EXPECT_FALSE(accepted);
}

TEST(HmacSha256, RefusesAByteBeforeAKey) {
  // Arrange
  mado::HmacSha256 mac;

  // Act
  const bool accepted = mac.update(1U);

  // Assert
  EXPECT_FALSE(accepted);
}

TEST(HmacSha256, RefusesAMissingKey) {
  // Arrange
  mado::HmacSha256 mac;

  // Act
  const bool keyed = mac.begin(nullptr, 4U, mado::KeyMemory::kRam);

  // Assert
  EXPECT_FALSE(keyed);
}

TEST(HmacSha256, RefusesAMissingBlock) {
  // Arrange
  mado::HmacSha256 mac;
  const uint8_t key[] = {7U};
  static_cast<void>(mac.begin(&key[0], sizeof(key), mado::KeyMemory::kRam));

  // Act
  const bool accepted = mac.update(nullptr, 3U);

  // Assert
  EXPECT_FALSE(accepted);
}

TEST(HmacSha256, RefusesAMissingOutput) {
  // Arrange
  mado::HmacSha256 mac;
  const uint8_t key[] = {7U};
  static_cast<void>(mac.begin(&key[0], sizeof(key), mado::KeyMemory::kRam));

  // Act
  const bool done = mac.finish(nullptr);

  // Assert
  EXPECT_FALSE(done);
}

class Pbkdf2RejectTest : public ::testing::TestWithParam<mado::Pbkdf2Params> {};

TEST_P(Pbkdf2RejectTest, RefusesInvalidParameters) {
  // Arrange
  const mado::Pbkdf2Params& params = GetParam();
  uint8_t derived[mado::kSha256Size] = {};

  // Act
  const bool done = mado::pbkdf2_sha256(params, &derived[0]);

  // Assert
  EXPECT_FALSE(done);
}

const uint8_t kSecret[] = {'p', 'w'};

INSTANTIATE_TEST_SUITE_P(
    Params, Pbkdf2RejectTest,
    ::testing::Values(mado::Pbkdf2Params{nullptr, 2U, &kSecret[0], 2U, 1U},
                      mado::Pbkdf2Params{&kSecret[0], 2U, &kSecret[0], 2U, 1000001U},
                      mado::Pbkdf2Params{&kSecret[0], 65U, &kSecret[0], 2U, 1U}));

TEST(Pbkdf2Sha256, RefusesAMissingOutput) {
  // Arrange
  const mado::Pbkdf2Params params = {&kSecret[0], 2U, &kSecret[0], 2U, 1U};

  // Act
  const bool done = mado::pbkdf2_sha256(params, nullptr);

  // Assert
  EXPECT_FALSE(done);
}

TEST(Sha256, AcceptsAnEmptyBlockWithoutData) {
  // Arrange
  mado::Sha256 hash;

  // Act
  const bool accepted = hash.update(nullptr, 0U);

  // Assert
  EXPECT_TRUE(accepted);
}

void read_value_for(MemoryReader& reader, const std::string& name) {
  if (name == "s") {
    char text[8] = {};
    static_cast<void>(reader.read_string(&text[0], sizeof(text)));
    return;
  }
  if (name == "n") {
    int32_t number = 0;
    static_cast<void>(reader.read_integer(number));
    return;
  }
  bool flag = false;
  static_cast<void>(reader.read_boolean(flag));
}

class JsonShapeTest : public ::testing::TestWithParam<std::string> {};

TEST_P(JsonShapeTest, ReportsAFailureForMalformedText) {
  // Arrange
  const std::string& json = GetParam();
  mado::MemorySource source(reinterpret_cast<const uint8_t*>(json.data()),
                            static_cast<uint16_t>(json.size()));
  MemoryReader reader(source);
  char key[8] = {};
  static_cast<void>(reader.begin_object());
  static_cast<void>(reader.next_key(&key[0], sizeof(key)));
  read_value_for(reader, &key[0]);
  static_cast<void>(reader.next_key(&key[0], sizeof(key)));

  // Act
  const mado::JsonStatus status = reader.finish();

  // Assert
  EXPECT_NE(status, mado::JsonStatus::kOk);
}

INSTANTIATE_TEST_SUITE_P(
    Texts, JsonShapeTest,
    ::testing::Values("{\"s", "{\"s\"", "{\"s\":", "{\"s\":\"ab", "{\"s\":\"a\\", "{\"s\":\"\\u12",
                      "{\"s\":\"\\uZZZZ\"}", "{\"n\":-", "{\"n\":-a}", "{\"n\":1", "{\"n\":1 2}",
                      "{\"b\":fals}", "{\"b\":f", "{\"b\":t", "{\"b\":nul}", "{\"b\":true",
                      "{\"s\":\"\xE2\x82\"}", "{\"s\":\"\xF5\"}", "{\"s\":\"\xC1\x80\"}",
                      "{\"s\":\"\xED\xA0\"}", "\t\r\n ", "{\"a\":1}x", "{ \"a\"\t:\n1 \r} {"));

struct TextCase {
  std::string json;
  mado::JsonStatus expected = mado::JsonStatus::kOk;
};

class JsonTextEdgeTest : public ::testing::TestWithParam<TextCase> {};

TEST_P(JsonTextEdgeTest, ReadsTheStringOrStops) {
  // Arrange
  const TextCase& sample = GetParam();
  mado::MemorySource source(reinterpret_cast<const uint8_t*>(sample.json.data()),
                            static_cast<uint16_t>(sample.json.size()));
  MemoryReader reader(source);
  char key[4] = {};
  char text[8] = {};
  static_cast<void>(reader.begin_object());
  static_cast<void>(reader.next_key(&key[0], sizeof(key)));

  // Act
  const mado::JsonStatus status = reader.read_string(&text[0], sizeof(text));

  // Assert
  EXPECT_EQ(status, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Strings, JsonTextEdgeTest,
    ::testing::Values(TextCase{"{\"s\":\"\xC3\\n\"}", mado::JsonStatus::kSyntax},
                      TextCase{"{\"s\":\"abcdef\\u00e9\"}", mado::JsonStatus::kTooLong},
                      TextCase{"{\"s\":\"abcde\\u20AC\"}", mado::JsonStatus::kTooLong},
                      TextCase{"{\"s\":\"abcdef\\u20AC\"}", mado::JsonStatus::kTooLong},
                      TextCase{"{\"s\":\"abcd\\u20AC\"}", mado::JsonStatus::kOk},
                      TextCase{"{\"s\":\"\x80\"}", mado::JsonStatus::kSyntax}));

class JsonDelimiterTest : public ::testing::TestWithParam<std::string> {};

TEST_P(JsonDelimiterTest, AcceptsWhitespaceAfterAValue) {
  // Arrange
  const std::string& json = GetParam();
  mado::MemorySource source(reinterpret_cast<const uint8_t*>(json.data()),
                            static_cast<uint16_t>(json.size()));
  MemoryReader reader(source);
  char key[4] = {};
  bool flag = false;
  static_cast<void>(reader.begin_object());
  static_cast<void>(reader.next_key(&key[0], sizeof(key)));
  static_cast<void>(reader.read_boolean(flag));
  static_cast<void>(reader.next_key(&key[0], sizeof(key)));

  // Act
  const mado::JsonStatus status = reader.finish();

  // Assert
  EXPECT_EQ(status, mado::JsonStatus::kOk);
}

INSTANTIATE_TEST_SUITE_P(Texts, JsonDelimiterTest,
                         ::testing::Values("{\"b\":true\t}", "{\"b\":false\r}", "{\"b\":true\n}",
                                           "{\"b\":true }"));

TEST(Base64UrlDecoder, RefusesToFinishAfterABadSymbol) {
  // Arrange
  mado::Base64UrlDecoder decoder;
  uint8_t chunk[mado::Base64UrlDecoder::kMaxOutput] = {};
  static_cast<void>(decoder.push('*', &chunk[0]));

  // Act
  const int8_t count = decoder.finish(&chunk[0]);

  // Assert
  EXPECT_EQ(count, -1);
}

}
