#include <Arduino.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "mado/HmacSha256.h"
#include "mado/KeyMemory.h"
#include "mado/Pbkdf2Params.h"
#include "mado/Sha256.h"
#include "mado/constant_time_equal.h"
#include "mado/kSha256Size.h"
#include "mado/pbkdf2_sha256.h"

namespace {

std::string hex(const uint8_t* const bytes, const size_t length) {
  static const char kDigits[] = "0123456789abcdef";
  std::string text;
  for (size_t i = 0U; i < length; ++i) {
    text.push_back(kDigits[bytes[i] >> 4U]);
    text.push_back(kDigits[bytes[i] & 0x0FU]);
  }
  return text;
}

std::vector<uint8_t> bytes_of(const std::string& text) {
  return std::vector<uint8_t>(text.begin(), text.end());
}

std::string sha256_hex(const std::string& message) {
  mado::Sha256 hash;
  const std::vector<uint8_t> data = bytes_of(message);
  static_cast<void>(hash.update(data.data(), data.size()));
  uint8_t digest[mado::kSha256Size] = {};
  const bool done = hash.finish(&digest[0]);
  return done ? hex(&digest[0], sizeof(digest)) : std::string("<failed>");
}

std::string hmac_hex(const std::vector<uint8_t>& key, const std::string& message) {
  mado::HmacSha256 mac;
  const bool keyed = mac.begin(key.data(), static_cast<uint8_t>(key.size()), mado::KeyMemory::kRam);
  const std::vector<uint8_t> data = bytes_of(message);
  static_cast<void>(mac.update(data.data(), data.size()));
  uint8_t digest[mado::kSha256Size] = {};
  const bool done = mac.finish(&digest[0]);
  return (keyed && done) ? hex(&digest[0], sizeof(digest)) : std::string("<failed>");
}

struct DigestCase {
  std::string message;
  const char* expected = nullptr;
};

class Sha256VectorTest : public ::testing::TestWithParam<DigestCase> {};

TEST_P(Sha256VectorTest, MatchesThePublishedDigest) {
  // Arrange
  const DigestCase& sample = GetParam();

  // Act
  const std::string digest = sha256_hex(sample.message);

  // Assert
  EXPECT_EQ(digest, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Fips180, Sha256VectorTest,
    ::testing::Values(
        DigestCase{"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        DigestCase{"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        DigestCase{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                   "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
        DigestCase{std::string(1000000U, 'a'),
                   "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"}));

TEST(Sha256, AcceptsByteAtATimeInput) {
  // Arrange
  mado::Sha256 hash;
  const std::string message = "abc";
  uint8_t digest[mado::kSha256Size] = {};

  for (const char symbol : message) {
    static_cast<void>(hash.update(static_cast<uint8_t>(symbol)));
  }

  // Act
  const bool done = hash.finish(&digest[0]);

  // Assert
  EXPECT_TRUE(done);
  EXPECT_EQ(hex(&digest[0], sizeof(digest)),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256, RefusesASecondFinish) {
  // Arrange
  mado::Sha256 hash;
  uint8_t digest[mado::kSha256Size] = {};
  const bool first = hash.finish(&digest[0]);

  // Act
  const bool second = hash.finish(&digest[0]);

  // Assert
  EXPECT_TRUE(first);
  EXPECT_FALSE(second);
}

TEST(Sha256, IgnoresInputAfterFinish) {
  // Arrange
  mado::Sha256 hash;
  uint8_t digest[mado::kSha256Size] = {};
  static_cast<void>(hash.finish(&digest[0]));

  // Act
  const bool accepted = hash.update(0x61U);

  // Assert
  EXPECT_FALSE(accepted);
}

TEST(Sha256, RejectsAMissingOutput) {
  // Arrange
  mado::Sha256 hash;

  // Act
  const bool done = hash.finish(nullptr);

  // Assert
  EXPECT_FALSE(done);
}

TEST(Sha256, RejectsAMissingInputBlock) {
  // Arrange
  mado::Sha256 hash;

  // Act
  const bool accepted = hash.update(nullptr, 3U);

  // Assert
  EXPECT_FALSE(accepted);
}

TEST(Sha256, StartsOverAfterReset) {
  // Arrange
  mado::Sha256 hash;
  static_cast<void>(hash.update(0x7AU));
  uint8_t digest[mado::kSha256Size] = {};

  hash.reset();

  // Act
  const bool done = hash.finish(&digest[0]);

  // Assert
  EXPECT_TRUE(done);
  EXPECT_EQ(hex(&digest[0], sizeof(digest)),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

struct MacCase {
  std::vector<uint8_t> key;
  std::string message;
  const char* expected = nullptr;
};

class HmacVectorTest : public ::testing::TestWithParam<MacCase> {};

TEST_P(HmacVectorTest, MatchesRfc4231) {
  // Arrange
  const MacCase& sample = GetParam();

  // Act
  const std::string digest = hmac_hex(sample.key, sample.message);

  // Assert
  EXPECT_EQ(digest, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Rfc4231, HmacVectorTest,
    ::testing::Values(MacCase{std::vector<uint8_t>(20U, 0x0BU), "Hi There",
                              "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"},
                      MacCase{bytes_of("Jefe"), "what do ya want for nothing?",
                              "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"},
                      MacCase{std::vector<uint8_t>(20U, 0xAAU), std::string(50U, '\xDD'),
                              "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe"}));

TEST(HmacSha256, ReadsTheKeyFromFlash) {
  // Arrange
  static const uint8_t kKey[] PROGMEM = {'J', 'e', 'f', 'e'};
  mado::HmacSha256 mac;
  const std::vector<uint8_t> data = bytes_of("what do ya want for nothing?");
  uint8_t digest[mado::kSha256Size] = {};

  const bool keyed = mac.begin(&kKey[0], sizeof(kKey), mado::KeyMemory::kFlash);
  static_cast<void>(mac.update(data.data(), data.size()));

  // Act
  const bool done = mac.finish(&digest[0]);

  // Assert
  EXPECT_TRUE(keyed && done);
  EXPECT_EQ(hex(&digest[0], sizeof(digest)),
            "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

TEST(HmacSha256, RestartsWithTheSameKey) {
  // Arrange
  const std::vector<uint8_t> key = bytes_of("Jefe");
  const std::vector<uint8_t> data = bytes_of("what do ya want for nothing?");
  mado::HmacSha256 mac;
  static_cast<void>(mac.begin(key.data(), 4U, mado::KeyMemory::kRam));
  static_cast<void>(mac.update(0x00U));
  uint8_t digest[mado::kSha256Size] = {};
  static_cast<void>(mac.finish(&digest[0]));

  const bool restarted = mac.restart();
  static_cast<void>(mac.update(data.data(), data.size()));

  // Act
  const bool done = mac.finish(&digest[0]);

  // Assert
  EXPECT_TRUE(restarted && done);
  EXPECT_EQ(hex(&digest[0], sizeof(digest)),
            "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

TEST(HmacSha256, RejectsAKeyLongerThanOneBlock) {
  // Arrange
  const std::vector<uint8_t> key(65U, 0x01U);
  mado::HmacSha256 mac;

  // Act
  const bool keyed = mac.begin(key.data(), static_cast<uint8_t>(key.size()), mado::KeyMemory::kRam);

  // Assert
  EXPECT_FALSE(keyed);
}

TEST(HmacSha256, RefusesToFinishWithoutAKey) {
  // Arrange
  mado::HmacSha256 mac;
  uint8_t digest[mado::kSha256Size] = {};

  // Act
  const bool done = mac.finish(&digest[0]);

  // Assert
  EXPECT_FALSE(done);
}

TEST(HmacSha256, RefusesToRestartWithoutAKey) {
  // Arrange
  mado::HmacSha256 mac;

  // Act
  const bool restarted = mac.restart();

  // Assert
  EXPECT_FALSE(restarted);
}

struct DeriveCase {
  std::string password;
  std::string salt;
  uint32_t iterations = 0U;
  const char* expected = nullptr;
};

class Pbkdf2VectorTest : public ::testing::TestWithParam<DeriveCase> {};

TEST_P(Pbkdf2VectorTest, MatchesPublishedVectors) {
  // Arrange
  const DeriveCase& sample = GetParam();
  const std::vector<uint8_t> password = bytes_of(sample.password);
  const std::vector<uint8_t> salt = bytes_of(sample.salt);
  uint8_t derived[mado::kSha256Size] = {};
  const mado::Pbkdf2Params params = {
      password.data(),   static_cast<uint8_t>(password.size()),
      salt.data(),       static_cast<uint8_t>(salt.size()),
      sample.iterations,
  };

  // Act
  const bool done = mado::pbkdf2_sha256(params, &derived[0]);

  // Assert
  EXPECT_TRUE(done);
  EXPECT_EQ(hex(&derived[0], sizeof(derived)), sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Rfc7914, Pbkdf2VectorTest,
    ::testing::Values(
        DeriveCase{"password", "salt", 1U,
                   "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"},
        DeriveCase{"password", "salt", 2U,
                   "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43"},
        DeriveCase{"password", "salt", 4096U,
                   "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a"}));

TEST(Pbkdf2Sha256, RejectsZeroIterations) {
  // Arrange
  const std::vector<uint8_t> password = bytes_of("password");
  const std::vector<uint8_t> salt = bytes_of("salt");
  uint8_t derived[mado::kSha256Size] = {};

  const mado::Pbkdf2Params params = {password.data(), 8U, salt.data(), 4U, 0U};

  // Act
  const bool done = mado::pbkdf2_sha256(params, &derived[0]);

  // Assert
  EXPECT_FALSE(done);
}

TEST(Pbkdf2Sha256, RejectsAMissingSalt) {
  // Arrange
  const std::vector<uint8_t> password = bytes_of("password");
  uint8_t derived[mado::kSha256Size] = {};

  const mado::Pbkdf2Params params = {password.data(), 8U, nullptr, 4U, 1U};

  // Act
  const bool done = mado::pbkdf2_sha256(params, &derived[0]);

  // Assert
  EXPECT_FALSE(done);
}

TEST(ConstantTimeEqual, AcceptsIdenticalBytes) {
  // Arrange
  const uint8_t left[] = {1U, 2U, 3U, 4U};
  const uint8_t right[] = {1U, 2U, 3U, 4U};

  // Act
  const bool same = mado::constant_time_equal(&left[0], &right[0], sizeof(left));

  // Assert
  EXPECT_TRUE(same);
}

TEST(ConstantTimeEqual, RejectsADifferenceInTheLastByte) {
  // Arrange
  const uint8_t left[] = {1U, 2U, 3U, 4U};
  const uint8_t right[] = {1U, 2U, 3U, 5U};

  // Act
  const bool same = mado::constant_time_equal(&left[0], &right[0], sizeof(left));

  // Assert
  EXPECT_FALSE(same);
}

TEST(ConstantTimeEqual, RejectsMissingInput) {
  // Arrange
  const uint8_t left[] = {1U};

  // Act
  const bool compared = mado::constant_time_equal(&left[0], nullptr, 1U);

  // Assert
  EXPECT_FALSE(compared);
}

TEST(Sha256, RefusesToResumeFromAMissingState) {
  // Arrange
  mado::Sha256 hash;

  // Act
  const bool resumed = hash.resume(nullptr, 64U);

  // Assert
  EXPECT_FALSE(resumed);
}

TEST(Sha256, RefusesToResumeMidBlock) {
  // Arrange
  mado::Sha256 hash;
  const uint32_t state[mado::Sha256::kStateWords] = {};

  // Act
  const bool resumed = hash.resume(&state[0], 3U);

  // Assert
  EXPECT_FALSE(resumed);
}

TEST(Sha256, RefusesToExportMidBlock) {
  // Arrange
  mado::Sha256 hash;
  static_cast<void>(hash.update(0x61U));
  uint32_t state[mado::Sha256::kStateWords] = {};

  // Act
  const bool exported = hash.export_state(&state[0]);

  // Assert
  EXPECT_FALSE(exported);
}

TEST(Sha256, StopsABlockUpdateAfterFinish) {
  // Arrange
  mado::Sha256 hash;
  uint8_t digest[mado::kSha256Size] = {};
  static_cast<void>(hash.finish(&digest[0]));
  const std::vector<uint8_t> data = bytes_of("abc");

  // Act
  const bool accepted = hash.update(data.data(), data.size());

  // Assert
  EXPECT_FALSE(accepted);
}

}
