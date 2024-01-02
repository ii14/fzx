#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "fzx/config.hpp"
#include "fzx/match.hpp"
#include "fzx/aligned_string.hpp"

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace fzx;

static inline AlignedString operator""_s(const char* data, size_t size)
{
  return { std::string_view { data, size }};
}

TEST_CASE("fzx::matchFuzzy")
{
  SECTION("exact match should return true") {
    CHECK(matchFuzzy("a"sv, "a"_s));
    CHECK(matchFuzzy("abc"sv, "abc"_s));
  }

  SECTION("partial match should return true") {
    CHECK(matchFuzzy("a"sv, "ab"_s));
    CHECK(matchFuzzy("a"sv, "ba"_s));
    CHECK(matchFuzzy("ab"sv, "aba"_s));
    CHECK(matchFuzzy("ab"sv, "  aba"_s));
  }

  SECTION("match with delimiters in between") {
    CHECK(matchFuzzy("abc"sv, "a|b|c"_s));
  }

  SECTION("non match should return false") {
    CHECK(!matchFuzzy("a"sv, ""_s));
    CHECK(!matchFuzzy("a"sv, "b"_s));
    CHECK(!matchFuzzy("ass"sv, "tags"_s));
  }

  SECTION("empty query should always match") {
    CHECK(matchFuzzy(""sv, ""_s));
    CHECK(matchFuzzy(""sv, "a"_s));
  }

  SECTION("simd") {
    CHECK(matchFuzzy("p"sv, "abcdefghijklmnop"_s));
    CHECK(!matchFuzzy("q"sv, "abcdefghijklmnop"_s));
    CHECK(matchFuzzy("q"sv, "abcdefghijklmnopq"_s));
    CHECK(!matchFuzzy("r"sv, "abcdefghijklmnopq"_s));

    CHECK(matchFuzzy("ep"sv, "abcdefghijklmnop"_s));
    CHECK(!matchFuzzy("eq"sv, "abcdefghijklmnop"_s));
    CHECK(matchFuzzy("eq"sv, "abcdefghijklmnopq"_s));
    CHECK(!matchFuzzy("er"sv, "abcdefghijklmnopq"_s));

    CHECK(matchFuzzy("7"sv, "abcdefghijklmnopqrstuvwx01234567"_s));
    CHECK(!matchFuzzy("y"sv, "abcdefghijklmnopqrstuvwx01234567"_s));
    CHECK(matchFuzzy("y"sv, "abcdefghijklmnopqrstuvwx01234567y"_s));
    CHECK(!matchFuzzy("z"sv, "abcdefghijklmnopqrstuvwx01234567y"_s));

    CHECK(matchFuzzy("e7"sv, "abcdefghijklmnopqrstuvwx01234567"_s));
    CHECK(!matchFuzzy("ey"sv, "abcdefghijklmnopqrstuvwx01234567"_s));
    CHECK(matchFuzzy("ey"sv, "abcdefghijklmnopqrstuvwx01234567y"_s));
    CHECK(!matchFuzzy("ez"sv, "abcdefghijklmnopqrstuvwx01234567y"_s));
  }
}

TEST_CASE("fzx::matchBegin")
{
  CHECK(matchBegin("a"sv, "a"_s));
  CHECK(matchBegin("abc"sv, "abc"_s));
  CHECK(matchBegin("a"sv, "abc"_s));
  CHECK(matchBegin("abc"sv, "abcdef"_s));
  CHECK(matchBegin(""sv, ""_s));
  CHECK(matchBegin(""sv, "a"_s));
  CHECK(!matchBegin("a"sv, ""_s));
  CHECK(!matchBegin("abc"sv, "def"_s));
  CHECK(!matchBegin("abc"sv, "a"_s));
  CHECK(!matchBegin("abc"sv, "ab"_s));
}

TEST_CASE("fzx::matchEnd")
{
  CHECK(matchEnd("a"sv, "a"_s));
  CHECK(matchEnd("abc"sv, "abc"_s));
  CHECK(matchEnd("c"sv, "abc"_s));
  CHECK(matchEnd("def"sv, "abcdef"_s));
  CHECK(matchEnd(""sv, ""_s));
  CHECK(matchEnd(""sv, "a"_s));
  CHECK(!matchEnd("a"sv, ""_s));
  CHECK(!matchEnd("abc"sv, "def"_s));
  CHECK(!matchEnd("abc"sv, "a"_s));
  CHECK(!matchEnd("abc"sv, "bc"_s));
}

TEST_CASE("fzx::matchExact")
{
  CHECK(matchExact(""sv, ""_s));
  CHECK(matchExact("a"sv, "a"_s));
  CHECK(matchExact("abc"sv, "abc"_s));
  CHECK(!matchExact("a"sv, ""_s));
  CHECK(!matchExact(""sv, "a"_s));
}

TEST_CASE("fzx::matchSubstr")
{
  CHECK(matchSubstr(""sv, ""_s));
  CHECK(matchSubstr(""sv, "abc"_s));
  CHECK(matchSubstr("a"sv, "abc"_s));
  CHECK(matchSubstr("b"sv, "abc"_s));
  CHECK(matchSubstr("c"sv, "abc"_s));
  CHECK(matchSubstr("ab"sv, "abc"_s));
  CHECK(matchSubstr("bc"sv, "abc"_s));
  CHECK(matchSubstr("abc"sv, "abc"_s));
  CHECK(!matchSubstr("a"sv, ""_s));
  CHECK(!matchSubstr("ac"sv, "abc"_s));
  CHECK(!matchSubstr("d"sv, "abc"_s));

  CHECK(matchSubstrIndex(""sv, ""_s) == 0);
  CHECK(matchSubstrIndex(""sv, "abc"_s) == 0);
  CHECK(matchSubstrIndex("a"sv, "abc"_s) == 0);
  CHECK(matchSubstrIndex("b"sv, "abc"_s) == 1);
  CHECK(matchSubstrIndex("c"sv, "abc"_s) == 2);
  CHECK(matchSubstrIndex("ab"sv, "abc"_s) == 0);
  CHECK(matchSubstrIndex("bc"sv, "abc"_s) == 1);
  CHECK(matchSubstrIndex("abc"sv, "abc"_s) == 0);
  CHECK(matchSubstrIndex("a"sv, ""_s) == -1);
  CHECK(matchSubstrIndex("ac"sv, "abc"_s) == -1);
  CHECK(matchSubstrIndex("d"sv, "abc"_s) == -1);
}
