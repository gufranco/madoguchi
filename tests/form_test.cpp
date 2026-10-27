#include <gtest/gtest.h>
#include <stdint.h>

#include <string>

#include "ExchangeHarness.h"
#include "FakeData.h"
#include "mado/FormStatus.h"
#include "mado/Method.h"
#include "mado/next_form_field.h"

namespace {

std::string read_all_fields(const std::string& body) {
  ExchangeHarness harness(body, static_cast<uint32_t>(body.size()), mado::Method::kPost);
  HarnessExchange exchange = harness.make();
  std::string fields;
  for (int i = 0; i < 16; ++i) {
    char name[16] = {};
    char value[16] = {};
    const mado::FormStatus status =
        mado::next_form_field(exchange, &name[0], sizeof(name), &value[0], sizeof(value));
    if (status != mado::FormStatus::kField) {
      fields += "#" + std::to_string(static_cast<int>(status));
      break;
    }
    fields += std::string(&name[0]) + "=" + std::string(&value[0]) + ";";
  }
  return fields;
}

struct FormCase {
  const char* body;
  const char* expected;
};

class NextFormFieldTest : public ::testing::TestWithParam<FormCase> {};

TEST_P(NextFormFieldTest, ReadsEveryFieldOfAnUrlencodedBody) {
  // Arrange
  const FormCase sample = GetParam();

  // Act
  const std::string fields = read_all_fields(sample.body);

  // Assert
  EXPECT_EQ(fields, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(Bodies, NextFormFieldTest,
                         ::testing::Values(FormCase{"a=1&b=two", "a=1;b=two;#1"},
                                           FormCase{"", "#1"}, FormCase{"flag&x=1", "flag=;x=1;#1"},
                                           FormCase{"a=", "a=;#1"},
                                           FormCase{"name=a%2Bb+c%26d", "name=a+b c&d;#1"},
                                           FormCase{"n=%4", "#3"}, FormCase{"n=%zz", "#3"},
                                           FormCase{"n=0123456789abcdef", "#2"},
                                           FormCase{"0123456789abcdefg=1", "#2"}));

TEST(NextFormField, RejectsMissingBuffers) {
  // Arrange
  ExchangeHarness harness("a=1", 3U, mado::Method::kPost);
  HarnessExchange exchange = harness.make();
  char value[4] = {};

  // Act
  const mado::FormStatus status = mado::next_form_field(exchange, nullptr, 4U, &value[0], 4U);

  // Assert
  EXPECT_EQ(status, mado::FormStatus::kBroken);
}

TEST(NextFormField, ReportsABodyThatStopsEarly) {
  // Arrange
  FakeData data(31U);
  ExchangeHarness harness("a=" + data.word(3), 40U, mado::Method::kPost);
  HarnessExchange exchange = harness.make();
  char name[8] = {};
  char value[8] = {};

  // Act
  const mado::FormStatus status =
      mado::next_form_field(exchange, &name[0], sizeof(name), &value[0], sizeof(value));

  // Assert
  EXPECT_EQ(status, mado::FormStatus::kBroken);
}

}
