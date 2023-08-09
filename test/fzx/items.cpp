#include <catch2/catch_test_macros.hpp>
#include "fzx/items.hpp"

TEST_CASE("fzx::Items")
{
  using namespace std::string_view_literals;

  SECTION("pushing empty string doesn't do anything") {
    fzx::Items items;
    REQUIRE(items.size() == 0);
    items.push(""sv);
    REQUIRE(items.size() == 0);
  }

  SECTION("pushing item") {
    fzx::Items items;
    REQUIRE(items.size() == 0);

    items.push("foo"sv);
    REQUIRE(items.size() == 1);
    REQUIRE(items.at(0) == "foo"sv);

    items.push("bar"sv);
    REQUIRE(items.size() == 2);
    REQUIRE(items.at(0) == "foo"sv);
    REQUIRE(items.at(1) == "bar"sv);

    items.push("baz"sv);
    REQUIRE(items.size() == 3);
    REQUIRE(items.at(0) == "foo"sv);
    REQUIRE(items.at(1) == "bar"sv);
    REQUIRE(items.at(2) == "baz"sv);
  }

  SECTION("pushing a lot of items") {
    fzx::Items items;
    constexpr auto kSize = 0x10000;
    for (size_t i = 0; i < kSize; ++i)
      items.push("0123456789abcdef"sv);
    REQUIRE(items.size() == kSize);
    for (size_t i = 0; i < kSize; ++i)
      REQUIRE(items.at(i) == "0123456789abcdef"sv);
  }

  SECTION("clearing empty vector does nothing") {
    fzx::Items items;
    items.clear();
  }

  SECTION("clearing vector with items empties it") {
    fzx::Items items;
    items.push("foo"sv);
    items.push("bar"sv);
    items.push("baz"sv);
    REQUIRE(items.size() == 3);
    items.clear();
    REQUIRE(items.size() == 0);
  }

  SECTION("copy") {
    fzx::Items items;
    items.push("foo"sv);
    items.push("bar"sv);
    items.push("baz"sv);

    SECTION("constructor") {
      fzx::Items copy { items }; // NOLINT
      REQUIRE(copy.size() == 3);
      REQUIRE(copy.at(0) == "foo"sv);
      REQUIRE(copy.at(1) == "bar"sv);
      REQUIRE(copy.at(2) == "baz"sv);

      SECTION("then nothing") {}
      SECTION("then add items the copy") {
        copy.push("foo"sv);
        copy.push("bar"sv);
        copy.push("baz"sv);
        REQUIRE(copy.size() == 6);
      }
    }

    SECTION("assignment") {
      fzx::Items copy;
      copy = items;
      REQUIRE(copy.size() == 3);
      REQUIRE(copy.at(0) == "foo"sv);
      REQUIRE(copy.at(1) == "bar"sv);
      REQUIRE(copy.at(2) == "baz"sv);

      SECTION("then nothing") {}
      SECTION("then add more items to the copy") {
        copy.push("foo"sv);
        copy.push("bar"sv);
        copy.push("baz"sv);
        REQUIRE(copy.size() == 6);
      }
    }

    REQUIRE(items.size() == 3);
    REQUIRE(items.at(0) == "foo"sv);
    REQUIRE(items.at(1) == "bar"sv);
    REQUIRE(items.at(2) == "baz"sv);
  }

  SECTION("move") {
    fzx::Items items;
    items.push("foo"sv);
    items.push("bar"sv);
    items.push("baz"sv);

    SECTION("constructor") {
      fzx::Items copy { std::move(items) };
      REQUIRE(copy.size() == 3);
      REQUIRE(copy.at(0) == "foo"sv);
      REQUIRE(copy.at(1) == "bar"sv);
      REQUIRE(copy.at(2) == "baz"sv);
    }

    SECTION("assignment") {
      fzx::Items copy;
      copy = std::move(items);
      REQUIRE(copy.size() == 3);
      REQUIRE(copy.at(0) == "foo"sv);
      REQUIRE(copy.at(1) == "bar"sv);
      REQUIRE(copy.at(2) == "baz"sv);
    }

    REQUIRE(items.size() == 0);
  }

}
