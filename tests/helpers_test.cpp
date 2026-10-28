#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "FakeData.h"
#include "mado/DecimalText.h"
#include "mado/DecodeMode.h"
#include "mado/HeaderStatus.h"
#include "mado/LineStatus.h"
#include "mado/Method.h"
#include "mado/equals_ignore_case.h"
#include "mado/equals_ignore_case_P.h"
#include "mado/error_status.h"
#include "mado/find_query_param.h"
#include "mado/hex_digit_value.h"
#include "mado/match_route.h"
#include "mado/parse_method.h"
#include "mado/reason_phrase.h"
#include "mado/to_lower_ascii.h"
#include "mado/url_decode.h"

namespace {

struct CharCase {
  char input;
  int expected;
};

class HexDigitValueTest : public ::testing::TestWithParam<CharCase> {};

TEST_P(HexDigitValueTest, MapsHexDigitsAndRejectsEverythingElse) {
  // Arrange
  const CharCase sample = GetParam();

  // Act
  const int8_t value = mado::hex_digit_value(sample.input);

  // Assert
  EXPECT_EQ(value, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(Digits, HexDigitValueTest,
                         ::testing::Values(CharCase{'0', 0}, CharCase{'9', 9}, CharCase{'a', 10},
                                           CharCase{'f', 15}, CharCase{'A', 10}, CharCase{'F', 15},
                                           CharCase{'g', -1}, CharCase{'G', -1}, CharCase{'/', -1},
                                           CharCase{':', -1}, CharCase{'@', -1},
                                           CharCase{'`', -1}));

class ToLowerAsciiTest : public ::testing::TestWithParam<CharCase> {};

TEST_P(ToLowerAsciiTest, LowersOnlyAsciiCapitals) {
  // Arrange
  const CharCase sample = GetParam();

  // Act
  const char lowered = mado::to_lower_ascii(sample.input);

  // Assert
  EXPECT_EQ(lowered, static_cast<char>(sample.expected));
}

INSTANTIATE_TEST_SUITE_P(Letters, ToLowerAsciiTest,
                         ::testing::Values(CharCase{'A', 'a'}, CharCase{'Z', 'z'},
                                           CharCase{'a', 'a'}, CharCase{'0', '0'},
                                           CharCase{'@', '@'}, CharCase{'[', '['}));

struct PairCase {
  const char* left;
  const char* right;
  bool expected;
};

class EqualsIgnoreCaseTest : public ::testing::TestWithParam<PairCase> {};

TEST_P(EqualsIgnoreCaseTest, ComparesWholeStringsWithoutCase) {
  // Arrange
  const PairCase sample = GetParam();

  // Act
  const bool equal = mado::equals_ignore_case(sample.left, sample.right);

  // Assert
  EXPECT_EQ(equal, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(Pairs, EqualsIgnoreCaseTest,
                         ::testing::Values(PairCase{"Host", "hOST", true},
                                           PairCase{"Content", "Content-Length", false},
                                           PairCase{"", "", true}, PairCase{nullptr, "a", false},
                                           PairCase{"a", nullptr, false}));

class EqualsIgnoreCasePTest : public ::testing::TestWithParam<PairCase> {};

TEST_P(EqualsIgnoreCasePTest, ComparesRamAgainstFlashWithoutCase) {
  // Arrange
  const PairCase sample = GetParam();

  // Act
  const bool equal = mado::equals_ignore_case_P(sample.left, sample.right);

  // Assert
  EXPECT_EQ(equal, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(Pairs, EqualsIgnoreCasePTest,
                         ::testing::Values(PairCase{"eXPECT", "Expect", true},
                                           PairCase{"Expected", "Expect", false},
                                           PairCase{"Expec", "Expect", false},
                                           PairCase{nullptr, "Expect", false},
                                           PairCase{"Expect", nullptr, false}));

struct DecodeCase {
  const char* input;
  const char* expected;
};

std::string decode(const char* text, mado::DecodeMode mode, uint16_t capacity) {
  char out[64] = {};
  const int16_t length =
      mado::url_decode(text, static_cast<uint16_t>(strlen(text)), &out[0], capacity, mode);
  const std::string text_out(&out[0]);
  return length < 0 ? std::string("<overflow:") + text_out + ">" : text_out;
}

class UrlDecodePathTest : public ::testing::TestWithParam<DecodeCase> {};

TEST_P(UrlDecodePathTest, DecodesTheUpstreamWorkedExamples) {
  // Arrange
  const DecodeCase sample = GetParam();

  // Act
  const std::string decoded = decode(sample.input, mado::DecodeMode::kPath, 64U);

  // Assert
  EXPECT_EQ(decoded, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Upstream, UrlDecodePathTest,
    ::testing::Values(DecodeCase{"index%2Ehtm", "index.htm"}, DecodeCase{"index%2E", "index."},
                      DecodeCase{"index%2", "index%2"}, DecodeCase{"%2Eindex", ".index"},
                      DecodeCase{"", ""}, DecodeCase{"%GEhtm", "%GEhtm"},
                      DecodeCase{"%%Ehtm", "%%Ehtm"}, DecodeCase{"a%2fb", "a/b"},
                      DecodeCase{"a%2Fb", "a/b"}, DecodeCase{"a+b", "a+b"}));

TEST(UrlDecode, TurnsPlusIntoSpaceInForms) {
  // Arrange
  const char* text = "a+b";

  // Act
  const std::string decoded = decode(text, mado::DecodeMode::kForm, 64U);

  // Assert
  EXPECT_EQ(decoded, "a b");
}

TEST(UrlDecode, ReportsOverflowAndKeepsTheTargetTerminated) {
  // Arrange
  const char* text = "abcdef";

  // Act
  const std::string decoded = decode(text, mado::DecodeMode::kPath, 4U);

  // Assert
  EXPECT_EQ(decoded, "<overflow:abc>");
}

TEST(UrlDecode, RejectsAMissingSource) {
  // Arrange
  char out[4] = {};

  // Act
  const int16_t length = mado::url_decode(nullptr, 1U, &out[0], 4U, mado::DecodeMode::kPath);

  // Assert
  EXPECT_EQ(length, -1);
}

TEST(UrlDecode, RejectsAZeroSizedTarget) {
  // Arrange
  char out[4] = {};

  // Act
  const int16_t length = mado::url_decode("a", 1U, &out[0], 0U, mado::DecodeMode::kPath);

  // Assert
  EXPECT_EQ(length, -1);
}

struct QueryCase {
  const char* query;
  const char* name;
  const char* expected;
};

class FindQueryParamTest : public ::testing::TestWithParam<QueryCase> {};

TEST_P(FindQueryParamTest, FindsAndDecodesTheNamedValue) {
  // Arrange
  const QueryCase sample = GetParam();
  char out[16] = {};

  // Act
  const bool found = mado::find_query_param(sample.query, sample.name, &out[0], sizeof(out));

  // Assert
  EXPECT_EQ(found ? std::string(&out[0]) : std::string("<absent>"), sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Queries, FindQueryParamTest,
    ::testing::Values(QueryCase{"a=1&flag&name=x%20y&b=two", "name", "x y"},
                      QueryCase{"a=1&flag&name=x%20y&b=two", "flag", ""},
                      QueryCase{"a=1&flag&name=x%20y&b=two", "nam", "<absent>"},
                      QueryCase{"a=1&flag&name=x%20y&b=two", "b", "two"},
                      QueryCase{"k=0123456789abcdefghij", "k", "<absent>"},
                      QueryCase{nullptr, "a", "<absent>"}, QueryCase{"a=1", nullptr, "<absent>"},
                      QueryCase{"", "a", "<absent>"}));

TEST(FindQueryParam, RejectsAMissingOutputBuffer) {
  // Arrange
  const char* query = "a=1";

  // Act
  const bool found = mado::find_query_param(query, "a", nullptr, 4U);

  // Assert
  EXPECT_FALSE(found);
}

class MatchRouteTest : public ::testing::TestWithParam<PairCase> {};

TEST_P(MatchRouteTest, MatchesExactPathsAndTrailingWildcards) {
  // Arrange
  const PairCase sample = GetParam();

  // Act
  const bool matched = mado::match_route(sample.left, sample.right);

  // Assert
  EXPECT_EQ(matched, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Routes, MatchRouteTest,
    ::testing::Values(PairCase{"/led", "/led", true}, PairCase{"/led", "/leds", false},
                      PairCase{"/files/*", "/files/a/b", true},
                      PairCase{"/files/*", "/files", false}, PairCase{"/a*b", "/a*b", true},
                      PairCase{"/led", nullptr, false}, PairCase{nullptr, "/led", false}));

struct MethodCase {
  const char* name;
  mado::Method expected;
};

class ParseMethodTest : public ::testing::TestWithParam<MethodCase> {};

TEST_P(ParseMethodTest, RecognizesEveryMethodCaseSensitively) {
  // Arrange
  const MethodCase sample = GetParam();

  // Act
  const mado::Method method =
      mado::parse_method(sample.name, static_cast<uint8_t>(strlen(sample.name)));

  // Assert
  EXPECT_EQ(method, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(Methods, ParseMethodTest,
                         ::testing::Values(MethodCase{"GET", mado::Method::kGet},
                                           MethodCase{"HEAD", mado::Method::kHead},
                                           MethodCase{"POST", mado::Method::kPost},
                                           MethodCase{"PUT", mado::Method::kPut},
                                           MethodCase{"DELETE", mado::Method::kDelete},
                                           MethodCase{"PATCH", mado::Method::kPatch},
                                           MethodCase{"OPTIONS", mado::Method::kOptions},
                                           MethodCase{"get", mado::Method::kUnknown},
                                           MethodCase{"GETX", mado::Method::kUnknown},
                                           MethodCase{"", mado::Method::kUnknown}));

struct PhraseCase {
  uint16_t code;
  const char* expected;
};

class ReasonPhraseTest : public ::testing::TestWithParam<PhraseCase> {};

TEST_P(ReasonPhraseTest, NamesKnownCodesAndLeavesOthersEmpty) {
  // Arrange
  const PhraseCase sample = GetParam();

  // Act
  const char* phrase = mado::reason_phrase(sample.code);

  // Assert
  EXPECT_STREQ(phrase, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Codes, ReasonPhraseTest,
    ::testing::Values(
        PhraseCase{100U, "Continue"}, PhraseCase{200U, "OK"}, PhraseCase{201U, "Created"},
        PhraseCase{202U, "Accepted"}, PhraseCase{409U, "Conflict"}, PhraseCase{204U, "No Content"},
        PhraseCase{301U, "Moved Permanently"}, PhraseCase{302U, "Found"},
        PhraseCase{303U, "See Other"}, PhraseCase{304U, "Not Modified"},
        PhraseCase{400U, "Bad Request"}, PhraseCase{401U, "Unauthorized"},
        PhraseCase{403U, "Forbidden"}, PhraseCase{404U, "Not Found"},
        PhraseCase{405U, "Method Not Allowed"}, PhraseCase{408U, "Request Timeout"},
        PhraseCase{411U, "Length Required"}, PhraseCase{413U, "Content Too Large"},
        PhraseCase{414U, "URI Too Long"}, PhraseCase{406U, "Not Acceptable"},
        PhraseCase{415U, "Unsupported Media Type"}, PhraseCase{422U, "Unprocessable Content"},
        PhraseCase{429U, "Too Many Requests"}, PhraseCase{431U, "Request Header Fields Too Large"},
        PhraseCase{500U, "Internal Server Error"}, PhraseCase{501U, "Not Implemented"},
        PhraseCase{503U, "Service Unavailable"}, PhraseCase{507U, "Insufficient Storage"},
        PhraseCase{299U, ""}, PhraseCase{99U, ""}, PhraseCase{600U, ""}));

struct NumberCase {
  uint32_t value;
  const char* expected;
};

class DecimalTextTest : public ::testing::TestWithParam<NumberCase> {};

TEST_P(DecimalTextTest, FormatsUnsignedValues) {
  // Arrange
  const NumberCase sample = GetParam();

  // Act
  const mado::DecimalText text(sample.value);

  // Assert
  EXPECT_EQ(std::string(text.begin(), text.length()), sample.expected);
}

INSTANTIATE_TEST_SUITE_P(Values, DecimalTextTest,
                         ::testing::Values(NumberCase{0U, "0"}, NumberCase{7U, "7"},
                                           NumberCase{1730000000U, "1730000000"},
                                           NumberCase{4294967295U, "4294967295"}));

TEST(DecimalText, MatchesTheStandardLibraryOnARandomValue) {
  // Arrange
  FakeData data(7U);
  const uint32_t value = data.number(1U, 4000000000U);

  // Act
  const mado::DecimalText text(value);

  // Assert
  EXPECT_EQ(std::string(text.begin(), text.length()), std::to_string(value));
}

struct LineStatusCase {
  mado::LineStatus status;
  uint16_t expected;
};

class LineErrorStatusTest : public ::testing::TestWithParam<LineStatusCase> {};

TEST_P(LineErrorStatusTest, MapsRequestLineFailuresToResponses) {
  // Arrange
  const LineStatusCase sample = GetParam();

  // Act
  const uint16_t code = mado::error_status(sample.status);

  // Assert
  EXPECT_EQ(code, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(Statuses, LineErrorStatusTest,
                         ::testing::Values(LineStatusCase{mado::LineStatus::kOk, 0U},
                                           LineStatusCase{mado::LineStatus::kTooLong, 414U},
                                           LineStatusCase{mado::LineStatus::kTimeout, 408U},
                                           LineStatusCase{mado::LineStatus::kClosed, 0U}));

struct HeaderStatusCase {
  mado::HeaderStatus status;
  uint16_t expected;
};

class HeaderErrorStatusTest : public ::testing::TestWithParam<HeaderStatusCase> {};

TEST_P(HeaderErrorStatusTest, MapsHeaderFailuresToResponses) {
  // Arrange
  const HeaderStatusCase sample = GetParam();

  // Act
  const uint16_t code = mado::error_status(sample.status);

  // Assert
  EXPECT_EQ(code, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(Statuses, HeaderErrorStatusTest,
                         ::testing::Values(HeaderStatusCase{mado::HeaderStatus::kOk, 0U},
                                           HeaderStatusCase{mado::HeaderStatus::kTimeout, 408U},
                                           HeaderStatusCase{mado::HeaderStatus::kClosed, 0U},
                                           HeaderStatusCase{mado::HeaderStatus::kMalformed, 400U},
                                           HeaderStatusCase{mado::HeaderStatus::kTooMany, 431U}));

}
