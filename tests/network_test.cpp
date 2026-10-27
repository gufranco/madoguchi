#include <Arduino.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <cstddef>
#include <string>
#include <vector>

#include "FakeData.h"
#include "FakeEeprom.h"
#include "FakeEthernet.h"
#include "FakeUdp.h"
#include "mado/MdnsResponder.h"
#include "mado/NetworkAction.h"
#include "mado/NetworkWatch.h"
#include "mado/begin_network.h"
#include "mado/load_mac.h"

namespace {

using Responder = mado::MdnsResponder<FakeUdp, IPAddress>;

const char kBoardName[] PROGMEM = "madoguchi";

struct Question {
  uint16_t type;
  uint16_t klass;
};

uint8_t octet(const IPAddress& address, const uint8_t index) {
  return static_cast<uint8_t>((static_cast<uint32_t>(address) >> (8U * index)) & 0xFFU);
}

std::vector<uint8_t> query(const std::string& name, const Question& question) {
  std::vector<uint8_t> packet = {
      0x12, 0x34, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  size_t start = 0U;
  for (size_t dot = name.find('.'); start <= name.size(); dot = name.find('.', start)) {
    const size_t end = (dot == std::string::npos) ? name.size() : dot;
    packet.push_back(static_cast<uint8_t>(end - start));
    packet.insert(packet.end(), name.begin() + static_cast<std::ptrdiff_t>(start),
                  name.begin() + static_cast<std::ptrdiff_t>(end));
    start = end + 1U;
  }
  packet.push_back(0x00);
  packet.push_back(static_cast<uint8_t>(question.type >> 8U));
  packet.push_back(static_cast<uint8_t>(question.type & 0xFFU));
  packet.push_back(static_cast<uint8_t>(question.klass >> 8U));
  packet.push_back(static_cast<uint8_t>(question.klass & 0xFFU));
  return packet;
}

TEST(BeginNetwork, UsesTheLeaseWhenDhcpAnswers) {
  // Arrange
  FakeEthernet ethernet(true);
  uint8_t mac[6] = {0x02, 1, 2, 3, 4, 5};

  // Act
  const bool leased = mado::begin_network(ethernet, &mac[0], IPAddress(192, 168, 1, 177), 8000U);

  // Assert
  EXPECT_TRUE(leased);
  EXPECT_EQ(ethernet.dhcp_calls(), 1U);
  EXPECT_EQ(ethernet.static_calls(), 0U);
  EXPECT_EQ(ethernet.last_timeout(), 8000U);
  EXPECT_EQ(ethernet.last_response_timeout(), mado::kDhcpResponseTimeoutMs);
}

TEST(BeginNetwork, FallsBackToTheStaticAddress) {
  // Arrange
  FakeEthernet ethernet(false);
  uint8_t mac[6] = {0x02, 1, 2, 3, 4, 5};

  // Act
  const bool leased = mado::begin_network(ethernet, &mac[0], IPAddress(10, 0, 0, 9), 8000U);

  // Assert
  EXPECT_FALSE(leased);
  EXPECT_EQ(ethernet.static_calls(), 1U);
  EXPECT_EQ(octet(ethernet.address(), 0U), 10U);
  EXPECT_EQ(octet(ethernet.address(), 3U), 9U);
}

TEST(BeginNetwork, RefusesATimeoutTooShortForDhcp) {
  // Arrange
  FakeEthernet ethernet(true);
  uint8_t mac[6] = {0x02, 1, 2, 3, 4, 5};

  // Act
  const bool leased = mado::begin_network(ethernet, &mac[0], IPAddress(10, 0, 0, 9), 10U);

  // Assert
  EXPECT_FALSE(leased);
  EXPECT_EQ(ethernet.dhcp_calls(), 0U);
  EXPECT_EQ(ethernet.static_calls(), 0U);
}

TEST(LoadMac, CreatesALocallyAdministeredUnicastAddressOnce) {
  // Arrange
  FakeData data(20261112U);
  FakeEeprom store;
  uint8_t mac[mado::kMacSize] = {};

  // Act
  const bool stored = mado::load_mac(store, 8U, mac, data.number(1U, 0xFFFFFFFEU));

  // Assert
  EXPECT_FALSE(stored);
  EXPECT_EQ(mac[0] & 0x03U, 0x02U);
  EXPECT_EQ(store.read(8), mado::kMacMarker);
  EXPECT_EQ(store.read(9), mac[0]);
}

TEST(LoadMac, ReturnsTheSameAddressAfterARestart) {
  // Arrange
  FakeData data(20261113U);
  FakeEeprom store;
  uint8_t first[mado::kMacSize] = {};
  static_cast<void>(mado::load_mac(store, 8U, first, data.number(1U, 0xFFFFFFFEU)));
  uint8_t second[mado::kMacSize] = {};

  // Act
  const bool stored = mado::load_mac(store, 8U, second, data.number(1U, 0xFFFFFFFEU));

  // Assert
  EXPECT_TRUE(stored);
  EXPECT_EQ(std::vector<uint8_t>(&first[0], &first[6]),
            std::vector<uint8_t>(&second[0], &second[6]));
}

TEST(LoadMac, GivesTwoSeedsTwoAddresses) {
  // Arrange
  FakeEeprom first_store;
  FakeEeprom second_store;
  uint8_t first[mado::kMacSize] = {};
  uint8_t second[mado::kMacSize] = {};
  static_cast<void>(mado::load_mac(first_store, 0U, first, 1U));

  // Act
  static_cast<void>(mado::load_mac(second_store, 0U, second, 2U));

  // Assert
  EXPECT_NE(std::vector<uint8_t>(&first[0], &first[6]),
            std::vector<uint8_t>(&second[0], &second[6]));
}

TEST(LoadMac, RefusesAStoreTooSmall) {
  // Arrange
  FakeEeprom store(6U);
  uint8_t mac[mado::kMacSize] = {};

  // Act
  const bool stored = mado::load_mac(store, 0U, mac, 7U);

  // Assert
  EXPECT_FALSE(stored);
  EXPECT_EQ(store.writes(), 0U);
}

TEST(NetworkWatch, RestartsWhenTheLinkComesBack) {
  // Arrange
  mado::NetworkWatch watch;
  static_cast<void>(watch.update({true, true, true}));
  static_cast<void>(watch.update({true, true, false}));

  // Act
  const mado::NetworkAction action = watch.update({true, true, true});

  // Assert
  EXPECT_EQ(action, mado::NetworkAction::kRestart);
  EXPECT_EQ(watch.restarts(), 1U);
}

TEST(NetworkWatch, RestartsWhenTheChipAnswersAgain) {
  // Arrange
  mado::NetworkWatch watch;
  static_cast<void>(watch.update({false, false, false}));

  // Act
  const mado::NetworkAction action = watch.update({true, false, false});

  // Assert
  EXPECT_EQ(action, mado::NetworkAction::kRestart);
}

TEST(NetworkWatch, StaysQuietWhileNothingChanges) {
  // Arrange
  mado::NetworkWatch watch;
  static_cast<void>(watch.update({true, false, false}));

  // Act
  const mado::NetworkAction action = watch.update({true, false, false});

  // Assert
  EXPECT_EQ(action, mado::NetworkAction::kNone);
  EXPECT_EQ(watch.restarts(), 0U);
}

TEST(NetworkWatch, IgnoresALinkThatWasNeverKnown) {
  // Arrange
  mado::NetworkWatch watch;
  static_cast<void>(watch.update({true, false, false}));

  // Act
  const mado::NetworkAction action = watch.update({true, true, true});

  // Assert
  EXPECT_EQ(action, mado::NetworkAction::kNone);
}

TEST(MdnsResponder, AnswersAQueryForItsName) {
  // Arrange
  FakeUdp udp;
  udp.deliver(query("madoguchi.local", Question{1U, 1U}));
  const Responder responder(&kBoardName[0]);

  // Act
  const bool answered = responder.poll(udp, IPAddress(192, 168, 1, 50));

  // Assert
  const std::vector<uint8_t> expected = {
      0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 9,   'm',  'a',
      'd',  'o',  'g',  'u',  'c',  'h',  'i',  5,    'l',  'o',  'c',  'a',  'l', 0x00, 0x00,
      0x01, 0x80, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x04, 192,  168,  1,    50,
  };
  ASSERT_TRUE(answered);
  ASSERT_EQ(udp.sent().size(), 1U);
  EXPECT_EQ(udp.sent().at(0), expected);
  EXPECT_EQ(octet(udp.destination(), 0U), 224U);
  EXPECT_EQ(octet(udp.destination(), 3U), 251U);
  EXPECT_EQ(udp.port(), mado::kMdnsPort);
}

TEST(MdnsResponder, AnswersAnyTypeAndTheUnicastBitInAnyCase) {
  // Arrange
  FakeUdp udp;
  udp.deliver(query("MadoGuchi.LOCAL", Question{255U, 0x8001U}));
  const Responder responder(&kBoardName[0]);

  // Act
  const bool answered = responder.poll(udp, IPAddress(10, 0, 0, 2));

  // Assert
  EXPECT_TRUE(answered);
}

struct IgnoredCase {
  std::string name;
  uint16_t type = 0U;
};

class MdnsIgnoredTest : public ::testing::TestWithParam<IgnoredCase> {};

TEST_P(MdnsIgnoredTest, StaysSilentForOtherQuestions) {
  // Arrange
  const IgnoredCase& sample = GetParam();
  FakeUdp udp;
  udp.deliver(query(sample.name, Question{sample.type, 1U}));
  const Responder responder(&kBoardName[0]);

  // Act
  const bool answered = responder.poll(udp, IPAddress(10, 0, 0, 2));

  // Assert
  EXPECT_FALSE(answered);
  EXPECT_TRUE(udp.sent().empty());
}

INSTANTIATE_TEST_SUITE_P(
    Questions, MdnsIgnoredTest,
    ::testing::Values(IgnoredCase{"other.local", 1U}, IgnoredCase{"madoguchi.local", 28U},
                      IgnoredCase{"madoguchi.lan", 1U}, IgnoredCase{"madoguchi.local.x", 1U},
                      IgnoredCase{"madoguch.local", 1U}, IgnoredCase{"madoguchi", 1U}));

TEST(MdnsResponder, IgnoresResponses) {
  // Arrange
  FakeUdp udp;
  std::vector<uint8_t> packet = query("madoguchi.local", Question{1U, 1U});
  packet.at(2) = 0x84U;
  udp.deliver(packet);
  const Responder responder(&kBoardName[0]);

  // Act
  const bool answered = responder.poll(udp, IPAddress(10, 0, 0, 2));

  // Assert
  EXPECT_FALSE(answered);
}

TEST(MdnsResponder, IgnoresACompressedName) {
  // Arrange
  FakeUdp udp;
  std::vector<uint8_t> packet = query("madoguchi.local", Question{1U, 1U});
  packet.resize(12U);
  const std::vector<uint8_t> pointer = {0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01};
  packet.insert(packet.end(), pointer.begin(), pointer.end());
  udp.deliver(packet);
  const Responder responder(&kBoardName[0]);

  // Act
  const bool answered = responder.poll(udp, IPAddress(10, 0, 0, 2));

  // Assert
  EXPECT_FALSE(answered);
}

TEST(MdnsResponder, IgnoresAShortPacketAndAnEmptySocket) {
  // Arrange
  FakeUdp udp;
  udp.deliver({0x00, 0x01, 0x02});
  const Responder responder(&kBoardName[0]);
  const bool short_answered = responder.poll(udp, IPAddress(10, 0, 0, 2));

  // Act
  const bool empty_answered = responder.poll(udp, IPAddress(10, 0, 0, 2));

  // Assert
  EXPECT_FALSE(short_answered);
  EXPECT_FALSE(empty_answered);
}

TEST(MdnsResponder, ReportsASocketThatCannotSend) {
  // Arrange
  FakeUdp udp;
  udp.refuse_begin();
  udp.deliver(query("madoguchi.local", Question{1U, 1U}));
  const Responder responder(&kBoardName[0]);

  // Act
  const bool answered = responder.poll(udp, IPAddress(10, 0, 0, 2));

  // Assert
  EXPECT_FALSE(answered);
  EXPECT_TRUE(udp.sent().empty());
}

TEST(MdnsResponder, ReportsAWriteTheSocketRefused) {
  // Arrange
  FakeUdp udp;
  udp.limit_writes(20U);
  udp.deliver(query("madoguchi.local", Question{1U, 1U}));
  const Responder responder(&kBoardName[0]);

  // Act
  const bool answered = responder.poll(udp, IPAddress(10, 0, 0, 2));

  // Assert
  EXPECT_FALSE(answered);
}

TEST(MdnsResponder, ReportsAPacketTheSocketDidNotSend) {
  // Arrange
  FakeUdp udp;
  udp.refuse_end();
  udp.deliver(query("madoguchi.local", Question{1U, 1U}));
  const Responder responder(&kBoardName[0]);

  // Act
  const bool answered = responder.poll(udp, IPAddress(10, 0, 0, 2));

  // Assert
  EXPECT_FALSE(answered);
  EXPECT_TRUE(udp.sent().empty());
}

TEST(MdnsResponder, LooksAtNoMoreThanFourQuestions) {
  // Arrange
  FakeUdp udp;
  std::vector<uint8_t> packet = query("other.local", Question{1U, 1U});
  packet.at(5) = 5U;
  const std::vector<uint8_t> other = query("other.local", Question{1U, 1U});
  for (int i = 0; i < 3; ++i) {
    packet.insert(packet.end(), other.begin() + 12, other.end());
  }
  const std::vector<uint8_t> ours = query("madoguchi.local", Question{1U, 1U});
  packet.insert(packet.end(), ours.begin() + 12, ours.end());
  udp.deliver(packet);
  const Responder responder(&kBoardName[0]);

  // Act
  const bool answered = responder.poll(udp, IPAddress(10, 0, 0, 2));

  // Assert
  EXPECT_FALSE(answered);
}

}
