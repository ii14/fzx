#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "fzx/match/fzy/fzy.hpp"
#include "fzx/match/fzy/config.hpp"
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
    CHECK(hasMatch("a"sv, "a"_s));
    CHECK(hasMatch("abc"sv, "abc"_s));
  }

  SECTION("partial match should return true") {
    CHECK(hasMatch("a"sv, "ab"_s));
    CHECK(hasMatch("a"sv, "ba"_s));
    CHECK(hasMatch("ab"sv, "aba"_s));
    CHECK(hasMatch("ab"sv, "  aba"_s));
  }

  SECTION("match with delimiters in between") {
    CHECK(hasMatch("abc"sv, "a|b|c"_s));
  }

  SECTION("non match should return false") {
    CHECK(!hasMatch("a"sv, ""_s));
    CHECK(!hasMatch("a"sv, "b"_s));
    CHECK(!hasMatch("ass"sv, "tags"_s));
  }

  SECTION("empty query should always match") {
    CHECK(hasMatch(""sv, ""_s));
    CHECK(hasMatch(""sv, "a"_s));
  }

  SECTION("sse") {
    auto b1 =
      "abcdefghijklmnop"
      "qrstuvwx01234567"_s;

    CHECK(hasMatch("p"sv, { b1.data(), 16 }));
    CHECK(!hasMatch("q"sv, { b1.data(), 16 }));
    CHECK(hasMatch("q"sv, { b1.data(), 17 }));
    CHECK(!hasMatch("r"sv, { b1.data(), 17 }));

    CHECK(hasMatch("ep"sv, { b1.data(), 16 }));
    CHECK(!hasMatch("eq"sv, { b1.data(), 16 }));
    CHECK(hasMatch("eq"sv, { b1.data(), 17 }));
    CHECK(!hasMatch("er"sv, { b1.data(), 17 }));
  }

  SECTION("avx") {
    auto b1 =
      "abcdefghijklmnop"
      "qrstuvwx01234567yz"_s;

    CHECK(hasMatch("7"sv, { b1.data(), 32 }));
    CHECK(!hasMatch("y"sv, { b1.data(), 32 }));
    CHECK(hasMatch("y"sv, { b1.data(), 33 }));
    CHECK(!hasMatch("z"sv, { b1.data(), 33 }));

    CHECK(hasMatch("e7"sv, { b1.data(), 32 }));
    CHECK(!hasMatch("ey"sv, { b1.data(), 32 }));
    CHECK(hasMatch("ey"sv, { b1.data(), 33 }));
    CHECK(!hasMatch("ez"sv, { b1.data(), 33 }));
  }
}

TEST_CASE("fzx::fzy::match", "[fzy]")
{
  SECTION("should prefer starts of words") {
    // App/Models/Order is better than App/MOdels/zRder
    CHECK(match("amor"sv, "app/models/order"_s) > match("amor"sv, "app/models/zrder"_s));
  }

  SECTION("should prefer consecutive letters") {
    // App/MOdels/foo is better than App/M/fOo
    CHECK(match("amo"sv, "app/m/foo"_s) < match("amo"sv, "app/models/foo"_s));
  }

  SECTION("should prefer contiguous over letter following period") {
    // GEMFIle.Lock < GEMFILe
    CHECK(match("gemfil"sv, "Gemfile.lock"_s) < match("gemfil"sv, "Gemfile"_s));
  }

  SECTION("should prefer shorter matches") {
    CHECK(match("abce"sv, "abcdef"_s) > match("abce"sv, "abc de"_s));
    CHECK(match("abc"sv, "    a b c "_s) > match("abc"sv, " a  b  c "_s));
    CHECK(match("abc"sv, " a b c    "_s) > match("abc"sv, " a  b  c "_s));
  }

  SECTION("should prefer shorter candidates") {
    CHECK(match("test"sv, "tests"_s) > match("test"sv, "testing"_s));
  }

  SECTION("should prefer start of candidate") {
    // Scores first letter highly
    CHECK(match("test"sv, "testing"_s) > match("test"sv, "/testing"_s));
  }

  SECTION("score exact match") {
    // Exact match is kScoreMax
    CHECK(Approx(kScoreMax) == match("abc"sv, "abc"_s));
    CHECK(Approx(kScoreMax) == match("aBc"sv, "abC"_s));
  }

  SECTION("score empty query") {
    // Empty query always results in kScoreMin
    CHECK(Approx(kScoreMin) == match(""sv, ""_s));
    CHECK(Approx(kScoreMin) == match(""sv, "a"_s));
    CHECK(Approx(kScoreMin) == match(""sv, "bb"_s));
  }

  SECTION("score gaps") {
    CHECK(Approx(kScoreGapLeading) == match("a"sv, "*a"_s));
    CHECK(Approx(kScoreGapLeading*2) == match("a"sv, "*ba"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreGapTrailing) == match("a"sv, "**a*"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreGapTrailing*2) == match("a"sv, "**a**"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchConsecutive + kScoreGapTrailing*2) == match("aa"sv, "**aa**"_s));
    CHECK(Approx(kScoreGapLeading + kScoreGapLeading + kScoreGapInner + kScoreGapTrailing + kScoreGapTrailing) == match("aa"sv, "**a*a**"_s));
  }

  SECTION("score consecutive") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchConsecutive) == match("aa"sv, "*aa"_s));
    CHECK(Approx(kScoreGapLeading + kScoreMatchConsecutive*2) == match("aaa"sv, "*aaa"_s));
    CHECK(Approx(kScoreGapLeading + kScoreGapInner + kScoreMatchConsecutive) == match("aaa"sv, "*a*aa"_s));
  }

  SECTION("score slash") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchSlash) == match("a"sv, "/a"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchSlash) == match("a"sv, "*/a"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchSlash + kScoreMatchConsecutive) == match("aa"sv, "a/aa"_s));
  }

  SECTION("score capital") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchCapital) == match("a"sv, "bA"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchCapital) == match("a"sv, "baA"_s));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchCapital + kScoreMatchConsecutive) == match("aa"sv, "baAa"_s));
  }

  SECTION("score dot") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchDot) == match("a"sv, ".a"_s));
    CHECK(Approx(kScoreGapLeading*3 + kScoreMatchDot) == match("a"sv, "*a.a"_s));
    CHECK(Approx(kScoreGapLeading + kScoreGapInner + kScoreMatchDot) == match("a"sv, "*a.a"_s));
  }

  SECTION("score long string") {
    char buf[4096] {};
    memset(buf, 'a', std::size(buf) - 1);
    std::string_view str { buf, std::size(buf) - 1 };
    CHECK(Approx(kScoreMin) == match("aa"sv, str));
    CHECK(Approx(kScoreMin) == match(str, "aa"_s));
    CHECK(Approx(kScoreMin) == match(str, str));
  }

  SECTION("positions consecutive") {
    Positions positions {};
    matchPositions("amo"sv, "app/models/foo"_s, &positions);
    CHECK(positions[0] == 0);
    CHECK(positions[1] == 4);
    CHECK(positions[2] == 5);
  }

  SECTION("positions start of word") {
    // We should prefer matching the 'o' in order, since it's the beginning of a word.
    Positions positions {};
    matchPositions("amor"sv, "app/models/order"_s, &positions);
    CHECK(positions[0] == 0);
    CHECK(positions[1] == 4);
    CHECK(positions[2] == 11);
    CHECK(positions[3] == 12);
  }

  SECTION("positions no bonuses") {
    Positions positions {};
    matchPositions("as"sv, "tags"_s, &positions);
    CHECK(1 == positions[0]);
    CHECK(3 == positions[1]);

    matchPositions("as"sv, "examples.txt"_s, &positions);
    CHECK(2 == positions[0]);
    CHECK(7 == positions[1]);
  }

  SECTION("positions multiple candidates start of words") {
    Positions positions {};
    matchPositions("abc"sv, "a/a/b/c/c"_s, &positions);
    CHECK(2 == positions[0]);
    CHECK(4 == positions[1]);
    CHECK(6 == positions[2]);
  }

  SECTION("positions exact match") {
    Positions positions {};
    matchPositions("foo"sv, "foo"_s, &positions);
    CHECK(0 == positions[0]);
    CHECK(1 == positions[1]);
    CHECK(2 == positions[2]);
  }
}
