#include <gtest/gtest.h>
#include <mado/FileLength.h>
#include <mado/Method.h>
#include <mado/RequestLine.h>
#include <mado/ResponseSummary.h>
#include <stdint.h>

#include <string>

#include "../src/audit/application/request_journal.service.h"
#include "../src/audit/domain/response_time.policy.h"
#include "FakeData.h"
#include "FakeSd.h"

namespace {

using Journal = audit::RequestJournal<FakeSd, FILE_READ, FILE_WRITE>;

class AuditJournalTest : public ::testing::Test {
 protected:
  FakeData& data() {
    return data_;
  }

  FakeSd& card() {
    return card_;
  }

  Journal& journal() {
    return journal_;
  }

 private:
  FakeData data_{20261113U};
  FakeSd card_;
  Journal journal_;
};

TEST_F(AuditJournalTest, MeasuresAResponseAcrossTheMillisecondWrap) {
  // Arrange
  const uint32_t started = 0xFFFFFFF0UL;

  // Act
  const uint32_t elapsed = audit::response_ms(started, 0x10UL);

  // Assert
  EXPECT_EQ(elapsed, 0x20UL);
}

TEST_F(AuditJournalTest, CountsAnImpossibleDurationAsZero) {
  // Arrange
  const uint32_t started = 0x90000000UL;

  // Act
  const uint32_t elapsed = audit::response_ms(started, 0x10000000UL);

  // Assert
  EXPECT_EQ(elapsed, 0U);
}

TEST_F(AuditJournalTest, AppendsOneLinePerResponse) {
  // Arrange
  journal().attach(&card(), 3U);
  const std::string path = "/" + data().word(6U);
  mado::RequestLine line = {};
  line.method = mado::Method::kGet;
  line.path = path.c_str();
  const mado::ResponseSummary response = {404U, 10U};

  // Act
  const bool logged = journal().request(line, response, 25U);

  // Assert
  EXPECT_TRUE(logged);
  EXPECT_NE(card().files()["/LOG.TXT"].find("\"url\":\"" + path + "\""), std::string::npos);
  EXPECT_NE(card().files()["/LOG.TXT"].find("\"statusCode\":404"), std::string::npos);
}

TEST_F(AuditJournalTest, ClearsTheLogAndItsRotatedCopy) {
  // Arrange
  journal().attach(&card(), 3U);
  card().files()["/LOG.TXT"] = "{}\n";
  card().files()["/LOG.OLD"] = "{}\n";

  // Act
  const bool cleared = journal().clear();

  // Assert
  EXPECT_TRUE(cleared);
  EXPECT_TRUE(card().files().empty());
}

TEST_F(AuditJournalTest, OpensNothingWithoutACard) {
  // Arrange
  journal().attach(nullptr, 3U);
  Journal::FileT file;

  // Act
  const mado::FileLength length = journal().open(file);

  // Assert
  EXPECT_FALSE(length.fits);
}

}
