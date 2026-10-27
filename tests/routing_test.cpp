#include <gtest/gtest.h>

#include "mado/Method.h"
#include "mado/RequestLine.h"
#include "mado/Route.h"
#include "mado/RouteLookup.h"
#include "mado/find_route.h"
#include "mado/parse_request_line.h"

namespace {

const mado::Route* routes() {
  static const mado::Route kRoutes[] = {
      {"/a", mado::Method::kPost, 1U},
      {"/a", mado::Method::kGet, 2U},
      {"/*", mado::Method::kAny, 3U},
      {nullptr, mado::Method::kAny, 0U},
  };
  return &kRoutes[0];
}

TEST(FindRoute, ServesHeadFromTheGetRoute) {
  // Arrange
  const mado::Route* table = routes();

  // Act
  const mado::RouteLookup lookup = mado::find_route<8U>(table, mado::Method::kHead, "/a");

  // Assert
  EXPECT_TRUE(lookup.found);
  EXPECT_EQ(lookup.id, 2U);
}

TEST(FindRoute, FallsThroughToAnAnyMethodWildcard) {
  // Arrange
  const mado::Route* table = routes();

  // Act
  const mado::RouteLookup lookup = mado::find_route<8U>(table, mado::Method::kDelete, "/zz");

  // Assert
  EXPECT_TRUE(lookup.found);
  EXPECT_EQ(lookup.id, 3U);
}

TEST(FindRoute, RejectsAMissingTable) {
  // Arrange
  const mado::Route* table = nullptr;

  // Act
  const mado::RouteLookup lookup = mado::find_route<8U>(table, mado::Method::kGet, "/a");

  // Assert
  EXPECT_FALSE(lookup.found);
}

TEST(FindRoute, StopsAtTheRouteLimit) {
  // Arrange
  const mado::Route* table = routes();

  // Act
  const mado::RouteLookup lookup = mado::find_route<1U>(table, mado::Method::kGet, "/a");

  // Assert
  EXPECT_FALSE(lookup.found);
  EXPECT_TRUE(lookup.path_matched);
}

TEST(ParseRequestLine, SplitsMethodPathAndQuery) {
  // Arrange
  char line[] = "GET  /a/b?x=1 HTTP/1.1";

  // Act
  const mado::RequestLine parsed = mado::parse_request_line(&line[0]);

  // Assert
  EXPECT_TRUE(parsed.valid);
  EXPECT_EQ(parsed.method, mado::Method::kGet);
  EXPECT_STREQ(parsed.path, "/a/b");
  EXPECT_STREQ(parsed.query, "x=1");
}

TEST(ParseRequestLine, RejectsALineWithoutAVersion) {
  // Arrange
  char line[] = "GET /";

  // Act
  const mado::RequestLine parsed = mado::parse_request_line(&line[0]);

  // Assert
  EXPECT_FALSE(parsed.valid);
  EXPECT_EQ(parsed.path, nullptr);
}

TEST(ParseRequestLine, RejectsAMethodWithoutATarget) {
  // Arrange
  char line[] = "GET";

  // Act
  const mado::RequestLine parsed = mado::parse_request_line(&line[0]);

  // Assert
  EXPECT_FALSE(parsed.valid);
}

TEST(ParseRequestLine, RejectsABlankTarget) {
  // Arrange
  char line[] = "GET  ";

  // Act
  const mado::RequestLine parsed = mado::parse_request_line(&line[0]);

  // Assert
  EXPECT_FALSE(parsed.valid);
}

TEST(ParseRequestLine, RejectsAMissingLine) {
  // Arrange
  char* line = nullptr;

  // Act
  const mado::RequestLine parsed = mado::parse_request_line(line);

  // Assert
  EXPECT_FALSE(parsed.valid);
}

}
