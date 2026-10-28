#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>

#include "../src/card/application/format.service.h"
#include "../src/card/domain/file_system.policy.h"
#include "../src/card/domain/format_phase.entity.h"
#include "../src/http/api.generated.h"
#include "../src/http/api.router.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "FakeSectorCard.h"

namespace {

using card::CardFileSystem;
using card::file_system_for;
using card::kFormatBatch;

constexpr uint8_t kSdsc = 2U;
constexpr uint8_t kSdhc = 3U;
constexpr uint32_t kSmallCard = 4100U;
constexpr uint32_t kMaxSteps = 100000UL;

class DriverControls {
 public:
  void refuse_format() {
    refuse_format_ = true;
  }

  void mount_nothing() {
    mount_nothing_ = true;
  }

  void record(const CardFileSystem kind) {
    formatted_ = kind;
  }

  bool refuses_format() const {
    return refuse_format_;
  }

  bool mounts_nothing() const {
    return mount_nothing_;
  }

  CardFileSystem formatted() const {
    return formatted_;
  }

 private:
  bool refuse_format_ = false;
  bool mount_nothing_ = false;
  CardFileSystem formatted_ = CardFileSystem::kNone;
};

struct FakeDriver {
  using Device = FakeSectorCard;

  static FakeSectorCard*& card() {
    static FakeSectorCard* instance = nullptr;
    return instance;
  }

  static DriverControls*& controls() {
    static DriverControls* instance = nullptr;
    return instance;
  }

  static Device* open() {
    return card();
  }

  static bool format(Device& device, const CardFileSystem kind, uint8_t* sector) {
    DriverControls* const set = controls();
    if (set == nullptr) {
      return false;
    }
    set->record(kind);
    sector[0] = 0xEBU;
    return (!set->refuses_format()) && device.writeSectors(0U, sector, 1U);
  }

  static CardFileSystem mounted(const Device& device) {
    const DriverControls* const set = controls();
    const bool marked = device.read(0U).at(0U) == 0xEBU;
    const bool answers = (set != nullptr) && marked && (!set->mounts_nothing());
    return answers ? set->formatted() : CardFileSystem::kNone;
  }
};

using Service = card::FormatService<FakeDriver>;
using App = api::Router<FakeClient, Service>;
using AppServer = mado::Server<FakeClient, App, rest_format::Limits>;

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

class RestFormatTest : public ::testing::Test {
 public:
  std::string get(const std::string& path) {
    FakeClient client("GET " + path + " HTTP/1.1\r\nHost: mado.test\r\n\r\n");
    server_.process(client);
    return client.output();
  }

  std::string start(const std::string& body) {
    FakeClient client(
        "POST /api/card/format HTTP/1.1\r\nHost: mado.test\r\nContent-Type: application/json\r\n"
        "Content-Length: " +
        std::to_string(body.size()) + "\r\n\r\n" + body);
    server_.process(client);
    return client.output();
  }

  void use(FakeSectorCard& card) {
    FakeDriver::card() = &card;
    static_cast<void>(service().begin());
  }

  bool running() {
    const card::FormatProgress progress = service_.status();
    const bool active = (progress.phase != card::FormatPhase::kIdle) &&
                        (progress.phase != card::FormatPhase::kDone);
    return active && (progress.failure == card::FormatFailure::kNone);
  }

  uint32_t run_to_end() {
    uint32_t steps = 0U;
    for (; running() && (steps < kMaxSteps); ++steps) {
      service_.step();
    }
    return steps;
  }

  FakeData& data() {
    return data_;
  }

  DriverControls& controls() {
    return controls_;
  }

 protected:
  void SetUp() override {
    fake_clock::set(0U);
    FakeDriver::card() = nullptr;
    controls_ = DriverControls{};
    FakeDriver::controls() = &controls_;
    static_cast<void>(service_.begin());
    App::attach(service_);
  }

  Service& service() {
    return service_;
  }

 private:
  AppServer server_{&rest_format::kRoutes[0], &rest_format::kHeaderNames[0]};
  FakeData data_{20261102U};
  DriverControls controls_;
  Service service_;
};

struct SizeCase {
  uint32_t sectors;
  CardFileSystem kind;
};

class FileSystemForTest : public ::testing::TestWithParam<SizeCase> {};

TEST_P(FileSystemForTest, PicksTheFileSystemTheCardSizeCalledFor) {
  // Arrange
  const SizeCase sample = GetParam();

  // Act
  const CardFileSystem kind = file_system_for(sample.sectors);

  // Assert
  EXPECT_EQ(kind, sample.kind);
}

INSTANTIATE_TEST_SUITE_P(Boundaries, FileSystemForTest,
                         ::testing::Values(SizeCase{0U, CardFileSystem::kNone},
                                           SizeCase{1U, CardFileSystem::kFat16},
                                           SizeCase{0x3FFFFFUL, CardFileSystem::kFat16},
                                           SizeCase{0x400000UL, CardFileSystem::kFat32},
                                           SizeCase{0x4000000UL, CardFileSystem::kFat32},
                                           SizeCase{0x4000001UL, CardFileSystem::kExFat},
                                           SizeCase{0xFFFFFFFFUL, CardFileSystem::kExFat}));

TEST_F(RestFormatTest, DescribesAnSdscCard) {
  // Arrange
  FakeSectorCard card(0x200000UL, kSdsc);
  use(card);

  // Act
  const std::string response = get("/api/card");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), R"({"capacity_mib":1024,"kind":"SDSC","file_system":"FAT16"})");
}

TEST_F(RestFormatTest, DescribesSdhcAndSdxcCards) {
  // Arrange
  FakeSectorCard sdhc(0x3B72400UL, kSdhc);
  FakeSectorCard sdxc(0x7720000UL, kSdhc);
  use(sdhc);

  // Act
  const std::string response = get("/api/card");

  // Assert
  EXPECT_EQ(body_of(response), R"({"capacity_mib":30436,"kind":"SDHC","file_system":"FAT32"})");
  use(sdxc);
  EXPECT_EQ(body_of(get("/api/card")),
            R"({"capacity_mib":60992,"kind":"SDXC","file_system":"exFAT"})");
}

TEST_F(RestFormatTest, AnswersUnavailableWithoutACard) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  card.set_present(false);
  use(card);

  // Act
  const std::string response = get("/api/card");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 503 Service Unavailable");
  EXPECT_EQ(status_line(start(R"({"capacity_mib":2})")), "HTTP/1.1 503 Service Unavailable");
  EXPECT_EQ(status_line(get("/api/card/format")), "HTTP/1.1 200 OK");
}

TEST_F(RestFormatTest, FindsACardInsertedAfterBoot) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  const std::string missing = get("/api/card");
  FakeDriver::card() = &card;

  // Act
  const std::string response = get("/api/card");

  // Assert
  EXPECT_EQ(status_line(missing), "HTTP/1.1 503 Service Unavailable");
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
}

TEST_F(RestFormatTest, ReportsAnIdleJobBeforeAnyWipe) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  use(card);

  // Act
  const std::string response = get("/api/card/format");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response),
            R"({"phase":"idle","percent":0,"capacity_mib":2,"file_system":"FAT16","detail":""})");
}

TEST_F(RestFormatTest, RefusesACapacityThatIsNotThisCards) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  use(card);

  // Act
  const std::string response = start(R"({"capacity_mib":3})");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
  EXPECT_NE(body_of(response).find("the capacity does not match this card"), std::string::npos);
  EXPECT_EQ(card.writes(), 0U);
}

TEST_F(RestFormatTest, RefusesABodyWithoutTheCapacity) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  use(card);

  // Act
  const std::string response = start("{}");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
  EXPECT_EQ(card.writes(), 0U);
}

TEST_F(RestFormatTest, StartsTheWipeAndRefusesASecondOne) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  use(card);

  // Act
  const std::string response = start(R"({"capacity_mib":2})");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 202 Accepted");
  EXPECT_NE(response.find("Location: /api/card/format\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), R"({"phase":"fill-zero","percent":0,"capacity_mib":2,)"
                               R"("file_system":"FAT16","detail":""})");
  EXPECT_EQ(status_line(start(R"({"capacity_mib":2})")), "HTTP/1.1 409 Conflict");
}

TEST_F(RestFormatTest, FillsVerifiesAndFormatsTheWholeCard) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  use(card);
  const std::string started = start(R"({"capacity_mib":2})");

  // Act
  const uint32_t steps = run_to_end();

  // Assert
  EXPECT_EQ(status_line(started), "HTTP/1.1 202 Accepted");
  EXPECT_LT(steps, kMaxSteps);
  EXPECT_EQ(card.writes(), (2U * kSmallCard) + 1U);
  EXPECT_EQ(card.reads(), 2U * kSmallCard);
  EXPECT_EQ(card.read(kSmallCard - 1U).at(kFakeSectorSize - 1U), 0xFFU);
  EXPECT_EQ(controls().formatted(), CardFileSystem::kFat16);
  EXPECT_EQ(body_of(get("/api/card/format")),
            R"({"phase":"done","percent":100,"capacity_mib":2,"file_system":"FAT16",)"
            R"("detail":"the card holds a fresh FAT16 volume"})");
}

TEST_F(RestFormatTest, AsksTheCardForItsSizeOnEveryVerifyBatch) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  use(card);
  const std::string started = start(R"({"capacity_mib":2})");
  for (uint32_t i = 0U; (i < kMaxSteps) && (card.reads() == 0U); ++i) {
    service().step();
  }
  const size_t queries = card.count_queries();
  const size_t reads = card.reads();

  // Act
  service().step();

  // Assert
  EXPECT_EQ(status_line(started), "HTTP/1.1 202 Accepted");
  EXPECT_EQ(card.count_queries(), queries + 1U);
  EXPECT_EQ(card.reads(), reads + kFormatBatch);
}

TEST_F(RestFormatTest, ReportsProgressHalfwayThroughTheFirstPass) {
  // Arrange
  FakeSectorCard card(0x2000UL, kSdsc);
  use(card);
  const std::string started = start(R"({"capacity_mib":4})");
  const uint32_t half = 0x1000UL / kFormatBatch;

  // Act
  for (uint32_t i = 0U; i < half; ++i) {
    service().step();
  }

  // Assert
  EXPECT_EQ(status_line(started), "HTTP/1.1 202 Accepted");
  EXPECT_EQ(
      body_of(get("/api/card/format")),
      R"({"phase":"fill-zero","percent":12,"capacity_mib":4,"file_system":"FAT16","detail":""})");
}

TEST_F(RestFormatTest, FailsWhenASectorDoesNotKeepItsZeros) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  card.stick(17U, data().number(0U, static_cast<uint32_t>(kFakeSectorSize - 1U)), 0x01U);
  use(card);
  const std::string started = start(R"({"capacity_mib":2})");

  // Act
  run_to_end();

  // Assert
  EXPECT_EQ(status_line(started), "HTTP/1.1 202 Accepted");
  EXPECT_EQ(controls().formatted(), CardFileSystem::kNone);
  const std::string status = body_of(get("/api/card/format"));
  EXPECT_NE(status.find(R"("phase":"failed")"), std::string::npos);
  EXPECT_NE(status.find("the card read back a byte it was not given"), std::string::npos);
}

TEST_F(RestFormatTest, FailsWhenASectorDoesNotKeepItsOnes) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  card.stick(4099U, data().number(0U, static_cast<uint32_t>(kFakeSectorSize - 1U)), 0x00U);
  use(card);
  const std::string started = start(R"({"capacity_mib":2})");

  // Act
  run_to_end();

  // Assert
  const std::string status = body_of(get("/api/card/format"));
  EXPECT_EQ(status_line(started), "HTTP/1.1 202 Accepted");
  EXPECT_NE(status.find(R"("phase":"failed")"), std::string::npos);
  EXPECT_EQ(card.writes(), 2U * kSmallCard);
}

TEST_F(RestFormatTest, FailsWhenTheCardRefusesAWrite) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  card.refuse_writes_from(2048U);
  use(card);
  const std::string started = start(R"({"capacity_mib":2})");

  // Act
  run_to_end();

  // Assert
  EXPECT_EQ(status_line(started), "HTTP/1.1 202 Accepted");
  EXPECT_NE(body_of(get("/api/card/format")).find("the card refused a write"), std::string::npos);
  EXPECT_EQ(status_line(start(R"({"capacity_mib":2})")), "HTTP/1.1 202 Accepted");
}

TEST_F(RestFormatTest, FailsWhenTheCardRefusesARead) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  use(card);
  const std::string started = start(R"({"capacity_mib":2})");
  for (uint32_t i = 0U; i < ((kSmallCard / kFormatBatch) + 1U); ++i) {
    service().step();
  }
  card.set_present(false);

  // Act
  service().step();

  // Assert
  EXPECT_EQ(status_line(started), "HTTP/1.1 202 Accepted");
  EXPECT_NE(body_of(get("/api/card/format")).find("the card refused a read"), std::string::npos);
}

TEST_F(RestFormatTest, FailsWhenTheCardIsSwappedDuringTheWipe) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  use(card);
  const std::string started = start(R"({"capacity_mib":2})");
  for (uint32_t i = 0U; i < ((kSmallCard / kFormatBatch) + 1U); ++i) {
    service().step();
  }
  card.resize(kSmallCard * 2U);

  // Act
  service().step();

  // Assert
  EXPECT_EQ(status_line(started), "HTTP/1.1 202 Accepted");
  EXPECT_NE(body_of(get("/api/card/format")).find("the card changed during the wipe"),
            std::string::npos);
}

TEST_F(RestFormatTest, FailsWhenTheFormatterFails) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  controls().refuse_format();
  use(card);
  const std::string started = start(R"({"capacity_mib":2})");

  // Act
  run_to_end();

  // Assert
  EXPECT_EQ(status_line(started), "HTTP/1.1 202 Accepted");
  EXPECT_NE(body_of(get("/api/card/format")).find("the format did not complete"),
            std::string::npos);
}

TEST_F(RestFormatTest, FailsWhenTheNewVolumeDoesNotMount) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  controls().mount_nothing();
  use(card);
  const std::string started = start(R"({"capacity_mib":2})");

  // Act
  run_to_end();

  // Assert
  EXPECT_EQ(status_line(started), "HTTP/1.1 202 Accepted");
  EXPECT_NE(body_of(get("/api/card/format")).find("the new volume did not mount"),
            std::string::npos);
}

TEST_F(RestFormatTest, StepsDoNothingWhileIdle) {
  // Arrange
  FakeSectorCard card(kSmallCard, kSdsc);
  use(card);

  // Act
  service().step();

  // Assert
  EXPECT_EQ(card.writes(), 0U);
  EXPECT_EQ(card.reads(), 0U);
}

}
