#include <gtest/gtest.h>
#include <mado/kSha256Size.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "../src/files/adapters/percent.encoding.h"
#include "../src/files/domain/file_path.value.h"
#include "../src/files/domain/upload.policy.h"
#include "FakeData.h"

namespace {

files::FilePath path_of(const std::string& text) {
  files::FilePath path = {};
  static_cast<void>(memcpy(&path.text[0], text.data(), text.size()));
  return path;
}

class FilesDomainTest : public ::testing::Test {
 protected:
  FakeData& data() {
    return data_;
  }

 private:
  FakeData data_{20261107U};
};

TEST_F(FilesDomainTest, AcceptsARootedLongName) {
  // Arrange
  const files::FilePath path = path_of("/" + data().word(20U) + " (2).jpeg");

  // Act
  const bool valid = files::is_file_path(path);

  // Assert
  EXPECT_TRUE(valid);
}

TEST_F(FilesDomainTest, RefusesANameWithoutTheLeadingSlash) {
  // Arrange
  const files::FilePath path = path_of(data().word(8U));

  // Act
  const bool valid = files::is_file_path(path);

  // Assert
  EXPECT_FALSE(valid);
}

TEST_F(FilesDomainTest, RefusesAReservedCharacter) {
  // Arrange
  const files::FilePath path = path_of("/" + data().word(4U) + "?" + data().word(3U));

  // Act
  const bool valid = files::is_file_path(path);

  // Assert
  EXPECT_FALSE(valid);
}

TEST_F(FilesDomainTest, TakesUploadsUpToEightMebibytes) {
  // Arrange
  const uint32_t largest = files::kMaxUpload;

  // Act
  const bool fits = files::upload_fits(largest);

  // Assert
  EXPECT_TRUE(fits);
  EXPECT_FALSE(files::upload_fits(largest + 1U));
}

TEST_F(FilesDomainTest, AcceptsAnyBodyWhenNoDigestWasGiven) {
  // Arrange
  const files::DigestClaim claim = {};
  uint8_t actual[mado::kSha256Size] = {};
  actual[0] = static_cast<uint8_t>(data().number(1U, 255U));

  // Act
  const bool accepted = files::digest_accepted(claim, actual);

  // Assert
  EXPECT_TRUE(accepted);
}

TEST_F(FilesDomainTest, RefusesABodyThatDoesNotMatchItsDigest) {
  // Arrange
  files::DigestClaim claim = {};
  claim.given = true;
  uint8_t actual[mado::kSha256Size] = {};
  actual[mado::kSha256Size - 1U] = static_cast<uint8_t>(data().number(1U, 255U));

  // Act
  const bool accepted = files::digest_accepted(claim, actual);

  // Assert
  EXPECT_FALSE(accepted);
}

TEST_F(FilesDomainTest, PercentEncodesEverythingButUnreservedCharacters) {
  // Arrange
  char location[files::kLocationSize] = "/api/files/";
  const files::FilePath path = path_of("/a b(1)~.txt");

  // Act
  const bool encoded = files::encode_path(path, location);

  // Assert
  EXPECT_TRUE(encoded);
  EXPECT_EQ(std::string(&location[0]), "/api/files/a%20b%281%29~.txt");
}

}
