#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>

#include "FakeClient.h"
#include "FakeData.h"
#include "mado/DefaultLimits.h"
#include "mado/OutputBuffer.h"

namespace {

using Buffer = mado::OutputBuffer<FakeClient, mado::DefaultLimits>;

TEST(OutputBuffer, SendsAWriteLargerThanTheBufferInOneCall) {
  // Arrange
  FakeData data(11U);
  const std::string payload = data.word(100);
  FakeClient client("");
  Buffer buffer;
  buffer.attach(client);

  // Act
  const size_t written =
      buffer.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

  // Assert
  EXPECT_EQ(written, 100U);
  EXPECT_EQ(client.write_calls(), 1U);
  EXPECT_TRUE(buffer.flush());
  EXPECT_EQ(client.output(), payload);
}

TEST(OutputBuffer, BatchesSmallWritesIntoBufferSizedChunks) {
  // Arrange
  FakeData data(13U);
  const std::string first = data.word(20);
  const std::string second = data.word(20);
  FakeClient client("");
  Buffer buffer;
  buffer.attach(client);
  static_cast<void>(buffer.write(reinterpret_cast<const uint8_t*>(first.data()), first.size()));

  // Act
  const size_t written =
      buffer.write(reinterpret_cast<const uint8_t*>(second.data()), second.size());

  // Assert
  EXPECT_EQ(written, 20U);
  EXPECT_EQ(client.write_calls(), 1U);
  EXPECT_TRUE(buffer.flush());
  EXPECT_EQ(client.output(), first + second);
}

TEST(OutputBuffer, FlushesPendingBytesBeforeALargeWrite) {
  // Arrange
  FakeData data(14U);
  const std::string head = data.word(5);
  const std::string body = data.word(64);
  FakeClient client("");
  Buffer buffer;
  buffer.attach(client);
  static_cast<void>(buffer.write(reinterpret_cast<const uint8_t*>(head.data()), head.size()));

  // Act
  const size_t written = buffer.write(reinterpret_cast<const uint8_t*>(body.data()), body.size());

  // Assert
  EXPECT_EQ(written, 64U);
  EXPECT_EQ(client.write_calls(), 2U);
  EXPECT_EQ(client.output(), head + body);
}

TEST(OutputBuffer, ReportsAShortWriteToTheClient) {
  // Arrange
  FakeClient client("");
  client.limit_write(5U);
  Buffer buffer;
  buffer.attach(client);
  buffer.write_byte(static_cast<uint8_t>('a'));
  buffer.write(reinterpret_cast<const uint8_t*>("0123456789"), 10U);

  // Act
  const bool flushed = buffer.flush();

  // Assert
  EXPECT_FALSE(flushed);
  EXPECT_FALSE(buffer.flush());
}

TEST(OutputBuffer, StopsAcceptingBytesOnceAWriteFails) {
  // Arrange
  FakeData data(12U);
  const std::string payload = data.word(40);
  FakeClient client("");
  client.limit_write(1U);
  Buffer buffer;
  buffer.attach(client);

  // Act
  const size_t written =
      buffer.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

  // Assert
  EXPECT_EQ(written, 0U);
  EXPECT_EQ(buffer.write_byte(static_cast<uint8_t>('x')), 0U);
  EXPECT_FALSE(buffer.flush());
}

TEST(OutputBuffer, RefusesBytesWhileDetached) {
  // Arrange
  Buffer buffer;

  // Act
  const size_t written = buffer.write_byte(static_cast<uint8_t>('x'));

  // Assert
  EXPECT_EQ(written, 0U);
  EXPECT_FALSE(buffer.flush());
}

TEST(OutputBuffer, DiscardsBytesInDiscardMode) {
  // Arrange
  FakeClient client("");
  Buffer buffer;
  buffer.attach(client);
  buffer.set_discard(true);

  // Act
  const size_t written = buffer.write_byte(static_cast<uint8_t>('x'));

  // Assert
  EXPECT_EQ(written, 1U);
  EXPECT_TRUE(buffer.flush());
  EXPECT_EQ(client.output(), "");
}

TEST(OutputBuffer, RejectsMissingDataWithALength) {
  // Arrange
  FakeClient client("");
  Buffer buffer;
  buffer.attach(client);

  // Act
  const size_t written = buffer.write(nullptr, 3U);

  // Assert
  EXPECT_EQ(written, 0U);
}

TEST(OutputBuffer, RejectsMissingFlashText) {
  // Arrange
  FakeClient client("");
  Buffer buffer;
  buffer.attach(client);

  // Act
  const size_t written = buffer.write_flash(nullptr);

  // Assert
  EXPECT_EQ(written, 0U);
}

TEST(OutputBuffer, KeepsItsBarrierIntact) {
  // Arrange
  FakeData data(13U);
  const std::string payload = data.word(500);
  FakeClient client("");
  Buffer buffer;
  buffer.attach(client);
  buffer.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

  // Act
  const bool intact = buffer.intact();

  // Assert
  EXPECT_TRUE(intact);
}

}
