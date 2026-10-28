#include <gtest/gtest.h>

#include <string>

#include "mado/parse_idempotency_key.h"

namespace {

struct KeyCase {
  const char* header = nullptr;
  bool parsed = false;
  const char* key = "";
};

class ParseIdempotencyKeyTest : public ::testing::TestWithParam<KeyCase> {};

TEST_P(ParseIdempotencyKeyTest, ReadsAQuotedStructuredString) {
  // Arrange
  const KeyCase& sample = GetParam();
  char key[mado::kIdempotencyKeySize] = {};

  // Act
  const bool parsed = mado::parse_idempotency_key(sample.header, key);

  // Assert
  EXPECT_EQ(parsed, sample.parsed);
  EXPECT_EQ(std::string(&key[0]), sample.key);
}

INSTANTIATE_TEST_SUITE_P(
    Headers, ParseIdempotencyKeyTest,
    ::testing::Values(KeyCase{"\"8e03978e-40d5-43e8\"", true, "8e03978e-40d5-43e8"},
                      KeyCase{"\"a b\"", true, "a b"}, KeyCase{nullptr, false, ""},
                      KeyCase{"8e03978e", false, ""}, KeyCase{"\"\"", false, ""},
                      KeyCase{"\"", false, ""}, KeyCase{"\"a\\\"b\"", false, ""},
                      KeyCase{"\"a\tb\"", false, ""}, KeyCase{"\"abc", false, ""}));

TEST(ParseIdempotencyKey, RefusesAKeyLongerThanSixtyFourCharacters) {
  // Arrange
  const std::string header = "\"" + std::string(65U, 'k') + "\"";
  char key[mado::kIdempotencyKeySize] = {};

  // Act
  const bool parsed = mado::parse_idempotency_key(header.c_str(), key);

  // Assert
  EXPECT_EQ(parsed, false);
  EXPECT_EQ(key[0], '\0');
}

TEST(ParseIdempotencyKey, AcceptsAKeyOfExactlySixtyFourCharacters) {
  // Arrange
  const std::string body(64U, 'k');
  const std::string header = "\"" + body + "\"";
  char key[mado::kIdempotencyKeySize] = {};

  // Act
  const bool parsed = mado::parse_idempotency_key(header.c_str(), key);

  // Assert
  EXPECT_EQ(parsed, true);
  EXPECT_EQ(std::string(&key[0]), body);
}

}
