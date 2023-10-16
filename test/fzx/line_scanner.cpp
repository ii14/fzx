#include <catch2/catch_test_macros.hpp>

#include "fzx/items.hpp"
#include "fzx/helper/line_scanner.hpp"

using namespace std::string_view_literals;

TEST_CASE("fzx::LineScanner")
{
  fzx::Items items;
  fzx::LineScanner scanner;
  auto push = [&](std::string_view s) { items.push(s); };
  auto feed = [&](std::string_view s) { return scanner.feed(s, push); };
  auto end = [&]() { return scanner.finalize(push); };

  SECTION("") {
    CHECK(feed(""sv) == 0);
    CHECK(feed(""sv) == 0);
    CHECK(end() == false);
    REQUIRE(items.size() == 0);
  }

  SECTION("") {
    CHECK(feed("\n"sv) == 0);
    CHECK(end() == false);
    REQUIRE(items.size() == 0);
  }

  SECTION("") {
    CHECK(feed("\n\n\n"sv) == 0);
    CHECK(end() == false);
    REQUIRE(items.size() == 0);
  }

  SECTION("") {
    CHECK(feed("foo"sv) == 0);
    CHECK(feed("bar"sv) == 0);
    CHECK(end() == true);
    REQUIRE(items.size() == 1);
    REQUIRE(items.at(0) == "foobar"sv);
  }

  SECTION("") {
    CHECK(feed("foo\n"sv) == 1);
    CHECK(feed("bar\n"sv) == 1);
    CHECK(end() == false);
    REQUIRE(items.size() == 2);
    REQUIRE(items.at(0) == "foo"sv);
    REQUIRE(items.at(1) == "bar"sv);
  }

  SECTION("") {
    CHECK(feed("\nfoo"sv) == 0);
    CHECK(feed("\nbar"sv) == 1);
    CHECK(end() == true);
    REQUIRE(items.size() == 2);
    REQUIRE(items.at(0) == "foo"sv);
    REQUIRE(items.at(1) == "bar"sv);
  }

  SECTION("") {
    CHECK(feed("fo"sv) == 0);
    CHECK(feed("o\nba"sv) == 1);
    CHECK(feed("r\n"sv) == 1);
    CHECK(end() == false);
    REQUIRE(items.size() == 2);
    REQUIRE(items.at(0) == "foo"sv);
    REQUIRE(items.at(1) == "bar"sv);
  }

  SECTION("") {
    CHECK(feed("foo\nbar\nbaz\n"sv) == 3);
    CHECK(end() == false);
    REQUIRE(items.size() == 3);
    REQUIRE(items.at(0) == "foo"sv);
    REQUIRE(items.at(1) == "bar"sv);
    REQUIRE(items.at(2) == "baz"sv);
  }
}
