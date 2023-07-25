#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "fzx/match/fzy.hpp"
#include "fzx/match/fzy_config.hpp"

using namespace std::string_view_literals;
using namespace fzx::fzy;
using Catch::Approx;

TEST_CASE("fzx::fzy::hasMatch", "[fzy]")
{
  SECTION("exact match should return true") {
    CHECK(hasMatch("a"sv, "a"sv));
  }

  SECTION("partial match should return true") {
    CHECK(hasMatch("a"sv, "ab"sv));
    CHECK(hasMatch("a"sv, "ba"sv));
  }

  SECTION("match with delimiters in between") {
    CHECK(hasMatch("abc"sv, "a|b|c"sv));
  }

  SECTION("non match should return false") {
    CHECK(!hasMatch("a"sv, ""sv));
    CHECK(!hasMatch("a"sv, "b"sv));
    CHECK(!hasMatch("ass"sv, "tags"sv));
  }

  SECTION("empty query should always match") {
    // match when query is empty
    CHECK(hasMatch(""sv, ""sv));
    CHECK(hasMatch(""sv, "a"sv));
  }
}

TEST_CASE("fzx::fzy::match", "[fzy]")
{
  SECTION("should prefer starts of words") {
    // App/Models/Order is better than App/MOdels/zRder
    CHECK(match("amor"sv, "app/models/order"sv) > match("amor"sv, "app/models/zrder"sv));
  }

  SECTION("should prefer consecutive letters") {
    // App/MOdels/foo is better than App/M/fOo
    CHECK(match("amo"sv, "app/m/foo"sv) < match("amo"sv, "app/models/foo"sv));
  }

  SECTION("should prefer contiguous over letter following period") {
    // GEMFIle.Lock < GEMFILe
    CHECK(match("gemfil"sv, "Gemfile.lock"sv) < match("gemfil"sv, "Gemfile"sv));
  }

  SECTION("should prefer shorter matches") {
    CHECK(match("abce"sv, "abcdef"sv) > match("abce"sv, "abc de"sv));
    CHECK(match("abc"sv, "    a b c "sv) > match("abc"sv, " a  b  c "sv));
    CHECK(match("abc"sv, " a b c    "sv) > match("abc"sv, " a  b  c "sv));
  }

  SECTION("should prefer shorter candidates") {
    CHECK(match("test"sv, "tests"sv) > match("test"sv, "testing"sv));
  }

  SECTION("should prefer start of candidate") {
    // Scores first letter highly
    CHECK(match("test"sv, "testing"sv) > match("test"sv, "/testing"sv));
  }

  SECTION("score exact match") {
    // Exact match is kScoreMax
    CHECK(Approx(kScoreMax) == match("abc"sv, "abc"sv));
    CHECK(Approx(kScoreMax) == match("aBc"sv, "abC"sv));
  }

  SECTION("score empty query") {
    // Empty query always results in kScoreMin
    CHECK(Approx(kScoreMin) == match(""sv, ""sv));
    CHECK(Approx(kScoreMin) == match(""sv, "a"sv));
    CHECK(Approx(kScoreMin) == match(""sv, "bb"sv));
  }

  SECTION("score gaps") {
    CHECK(Approx(kScoreGapLeading) == match("a"sv, "*a"sv));
    CHECK(Approx(kScoreGapLeading*2) == match("a"sv, "*ba"sv));
    CHECK(Approx(kScoreGapLeading*2 + kScoreGapTrailing) == match("a"sv, "**a*"sv));
    CHECK(Approx(kScoreGapLeading*2 + kScoreGapTrailing*2) == match("a"sv, "**a**"sv));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchConsecutive + kScoreGapTrailing*2) == match("aa"sv, "**aa**"sv));
    CHECK(Approx(kScoreGapLeading + kScoreGapLeading + kScoreGapInner + kScoreGapTrailing + kScoreGapTrailing) == match("aa"sv, "**a*a**"sv));
  }

  SECTION("score consecutive") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchConsecutive) == match("aa"sv, "*aa"sv));
    CHECK(Approx(kScoreGapLeading + kScoreMatchConsecutive*2) == match("aaa"sv, "*aaa"sv));
    CHECK(Approx(kScoreGapLeading + kScoreGapInner + kScoreMatchConsecutive) == match("aaa"sv, "*a*aa"sv));
  }

  SECTION("score slash") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchSlash) == match("a"sv, "/a"sv));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchSlash) == match("a"sv, "*/a"sv));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchSlash + kScoreMatchConsecutive) == match("aa"sv, "a/aa"sv));
  }

  SECTION("score capital") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchCapital) == match("a"sv, "bA"sv));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchCapital) == match("a"sv, "baA"sv));
    CHECK(Approx(kScoreGapLeading*2 + kScoreMatchCapital + kScoreMatchConsecutive) == match("aa"sv, "baAa"sv));
  }

  SECTION("score dot") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchDot) == match("a"sv, ".a"sv));
    CHECK(Approx(kScoreGapLeading*3 + kScoreMatchDot) == match("a"sv, "*a.a"sv));
    CHECK(Approx(kScoreGapLeading + kScoreGapInner + kScoreMatchDot) == match("a"sv, "*a.a"sv));
  }

  SECTION("score long string") {
    char str[4096] {};
    memset(str, 'a', std::size(str) - 1);
    CHECK(Approx(kScoreMin) == match("aa"sv, str));
    CHECK(Approx(kScoreMin) == match(str, "aa"sv));
    CHECK(Approx(kScoreMin) == match(str, str));
  }

  SECTION("positions consecutive") {
    Positions positions {};
    matchPositions("amo"sv, "app/models/foo"sv, &positions);
    CHECK(positions[0] == 0);
    CHECK(positions[1] == 4);
    CHECK(positions[2] == 5);
  }

  SECTION("positions start of word") {
    // We should prefer matching the 'o' in order, since it's the beginning of a word.
    Positions positions {};
    matchPositions("amor"sv, "app/models/order"sv, &positions);
    CHECK(positions[0] == 0);
    CHECK(positions[1] == 4);
    CHECK(positions[2] == 11);
    CHECK(positions[3] == 12);
  }

  SECTION("positions no bonuses") {
    Positions positions {};
    matchPositions("as"sv, "tags"sv, &positions);
    CHECK(1 == positions[0]);
    CHECK(3 == positions[1]);

    matchPositions("as"sv, "examples.txt"sv, &positions);
    CHECK(2 == positions[0]);
    CHECK(7 == positions[1]);
  }

  SECTION("positions multiple candidates start of words") {
    Positions positions {};
    matchPositions("abc"sv, "a/a/b/c/c"sv, &positions);
    CHECK(2 == positions[0]);
    CHECK(4 == positions[1]);
    CHECK(6 == positions[2]);
  }

  SECTION("positions exact match") {
    Positions positions {};
    matchPositions("foo"sv, "foo"sv, &positions);
    CHECK(0 == positions[0]);
    CHECK(1 == positions[1]);
    CHECK(2 == positions[2]);
  }
}
