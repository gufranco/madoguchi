#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "mado/Base64UrlDecoder.h"
#include "mado/Base64UrlEncoder.h"

namespace {

std::string encode(const std::vector<uint8_t>& input) {
  mado::Base64UrlEncoder encoder;
  std::string text;
  char chunk[mado::Base64UrlEncoder::kMaxOutput] = {};
  for (const uint8_t value : input) {
    const uint8_t count = encoder.push(value, &chunk[0]);
    text.append(&chunk[0], count);
  }
  const uint8_t tail = encoder.finish(&chunk[0]);
  text.append(&chunk[0], tail);
  return text;
}

std::string decode(const std::string& input) {
  mado::Base64UrlDecoder decoder;
  std::string bytes;
  uint8_t chunk[mado::Base64UrlDecoder::kMaxOutput] = {};
  for (const char symbol : input) {
    const int8_t count = decoder.push(symbol, &chunk[0]);
    if (count < 0) {
      return "<invalid>";
    }
    bytes.append(reinterpret_cast<const char*>(&chunk[0]), static_cast<size_t>(count));
  }
  const int8_t tail = decoder.finish(&chunk[0]);
  if (tail < 0) {
    return "<invalid>";
  }
  bytes.append(reinterpret_cast<const char*>(&chunk[0]), static_cast<size_t>(tail));
  return bytes;
}

struct CodecCase {
  std::string bytes;
  std::string text;
};

class Base64UrlVectorTest : public ::testing::TestWithParam<CodecCase> {};

TEST_P(Base64UrlVectorTest, EncodesWithoutPadding) {
  // Arrange
  const CodecCase& sample = GetParam();

  // Act
  const std::string text = encode(std::vector<uint8_t>(sample.bytes.begin(), sample.bytes.end()));

  // Assert
  EXPECT_EQ(text, sample.text);
}

TEST_P(Base64UrlVectorTest, DecodesBackToTheBytes) {
  // Arrange
  const CodecCase& sample = GetParam();

  // Act
  const std::string bytes = decode(sample.text);

  // Assert
  EXPECT_EQ(bytes, sample.bytes);
}

INSTANTIATE_TEST_SUITE_P(Rfc4648, Base64UrlVectorTest,
                         ::testing::Values(CodecCase{"", ""}, CodecCase{"f", "Zg"},
                                           CodecCase{"fo", "Zm8"}, CodecCase{"foo", "Zm9v"},
                                           CodecCase{"foob", "Zm9vYg"},
                                           CodecCase{"fooba", "Zm9vYmE"},
                                           CodecCase{"foobar", "Zm9vYmFy"},
                                           CodecCase{"\xFB\xFF", "-_8"}));

class Base64UrlRejectTest : public ::testing::TestWithParam<std::string> {};

TEST_P(Base64UrlRejectTest, RejectsNonCanonicalText) {
  // Arrange
  const std::string& text = GetParam();

  // Act
  const std::string bytes = decode(text);

  // Assert
  EXPECT_EQ(bytes, "<invalid>");
}

INSTANTIATE_TEST_SUITE_P(Malformed, Base64UrlRejectTest,
                         ::testing::Values("Z", "Zh", "Zm9", "Zm+v", "Zm/v", "Zg==", "Zm9v Y",
                                           "Zm9vYm"));

TEST(Base64UrlEncoder, RejectsAMissingOutput) {
  // Arrange
  mado::Base64UrlEncoder encoder;

  // Act
  const uint8_t count = encoder.push(0x41U, nullptr);

  // Assert
  EXPECT_EQ(count, 0U);
}

TEST(Base64UrlEncoder, FinishRejectsAMissingOutput) {
  // Arrange
  mado::Base64UrlEncoder encoder;
  char chunk[mado::Base64UrlEncoder::kMaxOutput] = {};
  static_cast<void>(encoder.push(0x41U, &chunk[0]));

  // Act
  const uint8_t count = encoder.finish(nullptr);

  // Assert
  EXPECT_EQ(count, 0U);
}

TEST(Base64UrlDecoder, RejectsAMissingOutput) {
  // Arrange
  mado::Base64UrlDecoder decoder;

  // Act
  const int8_t count = decoder.push('Z', nullptr);

  // Assert
  EXPECT_EQ(count, -1);
}

TEST(Base64UrlDecoder, FinishRejectsAMissingOutput) {
  // Arrange
  mado::Base64UrlDecoder decoder;

  // Act
  const int8_t count = decoder.finish(nullptr);

  // Assert
  EXPECT_EQ(count, -1);
}

TEST(Base64UrlDecoder, StaysFailedAfterABadSymbol) {
  // Arrange
  mado::Base64UrlDecoder decoder;
  uint8_t chunk[mado::Base64UrlDecoder::kMaxOutput] = {};
  static_cast<void>(decoder.push('*', &chunk[0]));

  // Act
  const int8_t count = decoder.push('Z', &chunk[0]);

  // Assert
  EXPECT_EQ(count, -1);
}

}
