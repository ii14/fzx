#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "fzx/match/fzy.hpp"
#include "fzx/match/fzy_config.hpp"
#include "fzx/config.hpp"

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace fzx::fzy;
using Catch::Approx;

/// Overallocated std::string
static std::string operator""_s(const char* data, size_t size)
{
  std::string s { data, size };
  s.reserve(s.size() + fzx::kOveralloc);
  return s;
}

TEST_CASE("fzx::fzy::hasMatch", "[fzy]")
{
  SECTION("exact match should return true") {
    CHECK(hasMatch("a"_s, "a"_s));
    CHECK(hasMatch("abc"_s, "abc"_s));
  }

  SECTION("partial match should return true") {
    CHECK(hasMatch("a"_s, "ab"_s));
    CHECK(hasMatch("a"_s, "ba"_s));
    CHECK(hasMatch("ab"_s, "aba"_s));
    CHECK(hasMatch("ab"_s, "  aba"_s));
  }

  SECTION("match with delimiters in between") {
    CHECK(hasMatch("abc"_s, "a|b|c"_s));
  }

  SECTION("non match should return false") {
    CHECK(!hasMatch("a"_s, ""_s));
    CHECK(!hasMatch("a"_s, "b"_s));
    CHECK(!hasMatch("ass"_s, "tags"_s));
  }

  SECTION("empty query should always match") {
    CHECK(hasMatch(""_s, ""_s));
    CHECK(hasMatch(""_s, "a"_s));
  }

  SECTION("sse") {
    auto b1 =
      "abcdefghijklmnop"
      "qrstuvwx01234567"_s;

    CHECK(hasMatch("p"_s, { b1.data(), 16 }));
    CHECK(!hasMatch("q"_s, { b1.data(), 16 }));
    CHECK(hasMatch("q"_s, { b1.data(), 17 }));
    CHECK(!hasMatch("r"_s, { b1.data(), 17 }));

    CHECK(hasMatch("ep"_s, { b1.data(), 16 }));
    CHECK(!hasMatch("eq"_s, { b1.data(), 16 }));
    CHECK(hasMatch("eq"_s, { b1.data(), 17 }));
    CHECK(!hasMatch("er"_s, { b1.data(), 17 }));
  }

  SECTION("avx") {
    auto b1 =
      "abcdefghijklmnop"
      "qrstuvwx01234567yz"_s;

    CHECK(hasMatch("7"_s, { b1.data(), 32 }));
    CHECK(!hasMatch("y"_s, { b1.data(), 32 }));
    CHECK(hasMatch("y"_s, { b1.data(), 33 }));
    CHECK(!hasMatch("z"_s, { b1.data(), 33 }));

    CHECK(hasMatch("e7"_s, { b1.data(), 32 }));
    CHECK(!hasMatch("ey"_s, { b1.data(), 32 }));
    CHECK(hasMatch("ey"_s, { b1.data(), 33 }));
    CHECK(!hasMatch("ez"_s, { b1.data(), 33 }));
  }
}

TEST_CASE("fzx::fzy::match", "[fzy]")
{
  SECTION("should prefer starts of words") {
    // App/Models/Order is better than App/MOdels/zRder
    CHECK(match("amor"_s, "app/models/order"_s) > match("amor"_s, "app/models/zrder"_s));
  }

  SECTION("should prefer consecutive letters") {
    // App/MOdels/foo is better than App/M/fOo
    CHECK(match("amo"_s, "app/m/foo"_s) < match("amo"_s, "app/models/foo"_s));
  }

  SECTION("should prefer contiguous over letter following period") {
    // GEMFIle.Lock < GEMFILe
    CHECK(match("gemfil"_s, "Gemfile.lock"_s) < match("gemfil"_s, "Gemfile"_s));
  }

  SECTION("should prefer shorter matches") {
    CHECK(match("abce"_s, "abcdef"_s) > match("abce"_s, "abc de"_s));
    CHECK(match("abc"_s, "    a b c "_s) > match("abc"_s, " a  b  c "_s));
    CHECK(match("abc"_s, " a b c    "_s) > match("abc"_s, " a  b  c "_s));
  }

  SECTION("should prefer shorter candidates") {
    CHECK(match("test"_s, "tests"_s) > match("test"_s, "testing"_s));
  }

  SECTION("should prefer start of candidate") {
    // Scores first letter highly
    CHECK(match("test"_s, "testing"_s) > match("test"_s, "/testing"_s));
  }

  SECTION("score exact match") {
    // Exact match is kScoreMax
    CHECK(Approx(kScoreMax) == match("abc"_s, "abc"_s));
    CHECK(Approx(kScoreMax) == match("aBc"_s, "abC"_s));
  }

  SECTION("score empty query") {
    // Empty query always results in kScoreMin
    CHECK(Approx(kScoreMin) == match(""_s, ""_s));
    CHECK(Approx(kScoreMin) == match(""_s, "a"_s));
    CHECK(Approx(kScoreMin) == match(""_s, "bb"_s));
  }

  SECTION("score gaps") {
    CHECK(Approx(kScoreGapLeading) == match("a"_s, "*a"_s));
    CHECK(Approx(kScoreGapLeading*2) == match("a"_s, "*ba"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreGapTrailing) == match("a"_s, "**a*"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreGapTrailing*2) == match("a"_s, "**a**"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchConsecutive + kScoreGapTrailing*2) == match("aa"_s, "**aa**"_s));
    CHECK(Approx(kScoreGapLeading + kScoreGapLeading + kScoreGapInner + kScoreGapTrailing + kScoreGapTrailing) == match("aa"_s, "**a*a**"_s));
  }

  SECTION("score consecutive") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchConsecutive) == match("aa"_s, "*aa"_s));
    CHECK(Approx(kScoreGapLeading + kScoreMatchConsecutive*2) == match("aaa"_s, "*aaa"_s));
    CHECK(Approx(kScoreGapLeading + kScoreGapInner + kScoreMatchConsecutive) == match("aaa"_s, "*a*aa"_s));
  }

  SECTION("score slash") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchSlash) == match("a"_s, "/a"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchSlash) == match("a"_s, "*/a"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchSlash + kScoreMatchConsecutive) == match("aa"_s, "a/aa"_s));
  }

  SECTION("score capital") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchCapital) == match("a"_s, "bA"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchCapital) == match("a"_s, "baA"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchCapital + kScoreMatchConsecutive) == match("aa"_s, "baAa"_s));
  }

  SECTION("score dot") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchDot) == match("a"_s, ".a"_s));
    CHECK(Approx(kScoreGapLeading*3 + kScoreMatchDot) == match("a"_s, "*a.a"_s));
    CHECK(Approx(kScoreGapLeading + kScoreGapInner + kScoreMatchDot) == match("a"_s, "*a.a"_s));
  }

  SECTION("score long string") {
    char str[4096] {};
    memset(str, 'a', std::size(str) - 1);
    CHECK(Approx(kScoreMin) == match("aa"_s, str));
    CHECK(Approx(kScoreMin) == match(str, "aa"_s));
    CHECK(Approx(kScoreMin) == match(str, str));
  }

  SECTION("positions consecutive") {
    Positions positions {};
    matchPositions("amo"_s, "app/models/foo"_s, &positions);
    CHECK(positions[0] == 0);
    CHECK(positions[1] == 4);
    CHECK(positions[2] == 5);
  }

  SECTION("positions start of word") {
    // We should prefer matching the 'o' in order, since it's the beginning of a word.
    Positions positions {};
    matchPositions("amor"_s, "app/models/order"_s, &positions);
    CHECK(positions[0] == 0);
    CHECK(positions[1] == 4);
    CHECK(positions[2] == 11);
    CHECK(positions[3] == 12);
  }

  SECTION("positions no bonuses") {
    Positions positions {};
    matchPositions("as"_s, "tags"_s, &positions);
    CHECK(1 == positions[0]);
    CHECK(3 == positions[1]);

    matchPositions("as"_s, "examples.txt"_s, &positions);
    CHECK(2 == positions[0]);
    CHECK(7 == positions[1]);
  }

  SECTION("positions multiple candidates start of words") {
    Positions positions {};
    matchPositions("abc"_s, "a/a/b/c/c"_s, &positions);
    CHECK(2 == positions[0]);
    CHECK(4 == positions[1]);
    CHECK(6 == positions[2]);
  }

  SECTION("positions exact match") {
    Positions positions {};
    matchPositions("foo"_s, "foo"_s, &positions);
    CHECK(0 == positions[0]);
    CHECK(1 == positions[1]);
    CHECK(2 == positions[2]);
  }
}
