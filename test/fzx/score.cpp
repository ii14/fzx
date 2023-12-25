#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "fzx/config.hpp"
#include "fzx/score.hpp"

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace fzx;
using Catch::Approx;

/// Overallocated std::string
static inline std::string operator""_s(const char* data, size_t size)
{
  std::string s { data, size };
  s.reserve(s.size() + fzx::kOveralloc);
  return s;
}

TEST_CASE("fzx::score", "[score]")
{
  SECTION("should prefer starts of words") {
    // App/Models/Order is better than App/MOdels/zRder
    CHECK(score("amor"sv, "app/models/order"_s) > score("amor"sv, "app/models/zrder"_s));
  }

  SECTION("should prefer consecutive letters") {
    // App/MOdels/foo is better than App/M/fOo
    CHECK(score("amo"sv, "app/m/foo"_s) < score("amo"sv, "app/models/foo"_s));
  }

  SECTION("should prefer contiguous over letter following period") {
    // GEMFIle.Lock < GEMFILe
    CHECK(score("gemfil"sv, "Gemfile.lock"_s) < score("gemfil"sv, "Gemfile"_s));
  }

  SECTION("should prefer shorter matches") {
    CHECK(score("abce"sv, "abcdef"_s) > score("abce"sv, "abc de"_s));
    CHECK(score("abc"sv, "    a b c "_s) > score("abc"sv, " a  b  c "_s));
    CHECK(score("abc"sv, " a b c    "_s) > score("abc"sv, " a  b  c "_s));
  }

  SECTION("should prefer shorter candidates") {
    CHECK(score("test"sv, "tests"_s) > score("test"sv, "testing"_s));
  }

  SECTION("should prefer start of candidate") {
    // Scores first letter highly
    CHECK(score("test"sv, "testing"_s) > score("test"sv, "/testing"_s));
  }

  SECTION("score exact match") {
    // Exact match is kScoreMax
    CHECK(Approx(kScoreMax) == score("abc"sv, "abc"_s));
    CHECK(Approx(kScoreMax) == score("aBc"sv, "abC"_s));
  }

  SECTION("score empty query") {
    // Empty query always results in kScoreMin
    CHECK(Approx(kScoreMin) == score(""sv, ""_s));
    CHECK(Approx(kScoreMin) == score(""sv, "a"_s));
    CHECK(Approx(kScoreMin) == score(""sv, "bb"_s));
  }

  SECTION("score gaps") {
    CHECK(Approx(kScoreGapLeading) == score("a"sv, "*a"_s));
    CHECK(Approx(kScoreGapLeading * 2) == score("a"sv, "*ba"_s));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreGapTrailing) == score("a"sv, "**a*"_s));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreGapTrailing * 2) == score("a"sv, "**a**"_s));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreMatchConsecutive + kScoreGapTrailing * 2)
          == score("aa"sv, "**aa**"_s));
    CHECK(Approx(kScoreGapLeading + kScoreGapLeading + kScoreGapInner + kScoreGapTrailing
                 + kScoreGapTrailing)
          == score("aa"sv, "**a*a**"_s));
  }

  SECTION("score consecutive") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchConsecutive) == score("aa"sv, "*aa"_s));
    CHECK(Approx(kScoreGapLeading + kScoreMatchConsecutive * 2) == score("aaa"sv, "*aaa"_s));
    CHECK(Approx(kScoreGapLeading + kScoreGapInner + kScoreMatchConsecutive)
          == score("aaa"sv, "*a*aa"_s));
  }

  SECTION("score slash") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchSlash) == score("a"sv, "/a"_s));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreMatchSlash) == score("a"sv, "*/a"_s));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreMatchSlash + kScoreMatchConsecutive)
          == score("aa"sv, "a/aa"_s));
  }

  SECTION("score capital") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchCapital) == score("a"sv, "bA"_s));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreMatchCapital) == score("a"sv, "baA"_s));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreMatchCapital + kScoreMatchConsecutive)
          == score("aa"sv, "baAa"_s));
  }

  SECTION("score dot") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchDot) == score("a"sv, ".a"_s));
    CHECK(Approx(kScoreGapLeading * 3 + kScoreMatchDot) == score("a"sv, "*a.a"_s));
    CHECK(Approx(kScoreGapLeading + kScoreGapInner + kScoreMatchDot) == score("a"sv, "*a.a"_s));
  }

  SECTION("score long string") {
    char buf[4096] {};
    memset(buf, 'a', std::size(buf) - 1);
    std::string_view str { buf, std::size(buf) - 1 };
    CHECK(Approx(kScoreMin) == score("aa"sv, str));
    CHECK(Approx(kScoreMin) == score(str, "aa"_s));
    CHECK(Approx(kScoreMin) == score(str, str));
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

#if defined(FZX_SSE2)
TEST_CASE("fzx::scoreSse", "[score]")
{
  auto h = "/Lorem/ipsum/dolor/sit/amet/consectetur/adipiscing/elit/"
           "Maecenas/mollis/odio/semper/nunc/convallis/accumsan/"_s;

  const std::vector<std::string_view> kTestCases {
    "/"sv,
    "//"sv,
    "///"sv,
    "////"sv,
    "/////"sv,
    "//////"sv,
    "///////"sv,
    "////////"sv,
    "/////////"sv,
    "//////////"sv,
    "///////////"sv,
    "////////////"sv,
    "/////////////"sv,
    "//////////////"sv,
    "///////////////"sv,
    "////////////////"sv,
    "l"sv,
    "li"sv,
    "lid"sv,
    "lids"sv,
    "lidsa"sv,
    "lidsac"sv,
    "lidsaca"sv,
    "lidsacae"sv,
    "lidsacaem"sv,
    "lidsacaemm"sv,
    "lidsacaemmo"sv,
    "lidsacaemmos"sv,
    "lidsacaemmosn"sv,
    "lidsacaemmosnc"sv,
    "lidsacaemmosnca"sv,
    "co"sv,
    "con"sv,
    "cons"sv,
    "conse"sv,
    "consec"sv,
    "consect"sv,
    "consecte"sv,
    "consectet"sv,
    "consectetu"sv,
    "consectetur"sv,
  };

  for (auto t : kTestCases) {
    CAPTURE(t);
    fzx::AlignedString n { t };
    if (!t.empty() && t.size() <= 4) {
      CHECK(Approx(score(n, h)) == scoreSSE<4>(n, h));
    } else if (t.size() >= 5 && t.size() <= 8) {
      CHECK(Approx(score(n, h)) == scoreSSE<8>(n, h));
    } else if (t.size() >= 9 && t.size() <= 12) {
      CHECK(Approx(score(n, h)) == scoreSSE<12>(n, h));
    } else if (t.size() >= 13 && t.size() <= 16) {
      CHECK(Approx(score(n, h)) == scoreSSE<16>(n, h));
    } else {
      FAIL("invalid needle size");
    }
  }
}
#endif

#if defined(FZX_NEON)
TEST_CASE("fzx::scoreNeon", "[score]")
{
  auto h = "/Lorem/ipsum/dolor/sit/amet/consectetur/adipiscing/elit/"
           "Maecenas/mollis/odio/semper/nunc/convallis/accumsan/"_s;

  const std::vector<std::string_view> kTestCases {
    "/"sv,
    "//"sv,
    "///"sv,
    "////"sv,
    "/////"sv,
    "//////"sv,
    "///////"sv,
    "////////"sv,
    "/////////"sv,
    "//////////"sv,
    "///////////"sv,
    "////////////"sv,
    "/////////////"sv,
    "//////////////"sv,
    "///////////////"sv,
    "////////////////"sv,
    "l"sv,
    "li"sv,
    "lid"sv,
    "lids"sv,
    "lidsa"sv,
    "lidsac"sv,
    "lidsaca"sv,
    "lidsacae"sv,
    "lidsacaem"sv,
    "lidsacaemm"sv,
    "lidsacaemmo"sv,
    "lidsacaemmos"sv,
    "lidsacaemmosn"sv,
    "lidsacaemmosnc"sv,
    "lidsacaemmosnca"sv,
    "co"sv,
    "con"sv,
    "cons"sv,
    "conse"sv,
    "consec"sv,
    "consect"sv,
    "consecte"sv,
    "consectet"sv,
    "consectetu"sv,
    "consectetur"sv,
  };

  for (auto t : kTestCases) {
    CAPTURE(t);
    fzx::AlignedString n { t };
    if (!t.empty() && t.size() <= 4) {
      CHECK(Approx(score(n, h)) == scoreNeon<4>(n, h));
    } else if (t.size() >= 5 && t.size() <= 8) {
      CHECK(Approx(score(n, h)) == scoreNeon<8>(n, h));
    } else if (t.size() >= 9 && t.size() <= 12) {
      CHECK(Approx(score(n, h)) == scoreNeon<12>(n, h));
    } else if (t.size() >= 13 && t.size() <= 16) {
      CHECK(Approx(score(n, h)) == scoreNeon<16>(n, h));
    } else {
      FAIL("invalid needle size");
    }
  }
}
#endif
