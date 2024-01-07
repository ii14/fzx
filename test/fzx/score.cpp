#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

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
  auto f = [](const AlignedString& needle, const AlignedString& haystack) -> Score {
    switch (needle.size()) {
    case 1:
      return score1(needle, haystack);
#if defined(FZX_SSE2)
      // clang-format off
    case 2: case 3: case 4:
      return scoreSSE<4>(needle, haystack);
    case 5: case 6: case 7: case 8:
      return scoreSSE<8>(needle, haystack);
    case 9: case 10: case 11: case 12:
      return scoreSSE<12>(needle, haystack);
    case 13: case 14: case 15: case 16:
      return scoreSSE<16>(needle, haystack);
      // clang-format on
#elif defined(FZX_NEON)
      // clang-format off
    case 2: case 3: case 4:
      return scoreNeon<4>(needle, haystack);
    case 5: case 6: case 7: case 8:
      return scoreNeon<8>(needle, haystack);
    case 9: case 10: case 11: case 12:
      return scoreNeon<12>(needle, haystack);
    case 13: case 14: case 15: case 16:
      return scoreNeon<16>(needle, haystack);
      // clang-format on
#endif
    default:
      return score(needle, haystack);
    }
  };

  SECTION("should prefer starts of words") {
    // App/Models/Order is better than App/MOdels/zRder
    CHECK(f("amor"sv, "app/models/order"sv) > f("amor"sv, "app/models/zrder"sv));
  }

  SECTION("should prefer consecutive letters") {
    // App/MOdels/foo is better than App/M/fOo
    CHECK(f("amo"sv, "app/m/foo"sv) < f("amo"sv, "app/models/foo"sv));
  }

  SECTION("should prefer contiguous over letter following period") {
    // GEMFIle.Lock < GEMFILe
    CHECK(f("gemfil"sv, "Gemfile.lock"sv) < f("gemfil"sv, "Gemfile"sv));
  }

  SECTION("should prefer shorter matches") {
    CHECK(f("abce"sv, "abcdef"sv) > f("abce"sv, "abc de"sv));
    CHECK(f("abc"sv, "    a b c "sv) > f("abc"sv, " a  b  c "sv));
    CHECK(f("abc"sv, " a b c    "sv) > f("abc"sv, " a  b  c "sv));
  }

  SECTION("should prefer shorter candidates") {
    CHECK(f("test"sv, "tests"sv) > f("test"sv, "testing"sv));
  }

  SECTION("should prefer start of candidate") {
    // Scores first letter highly
    CHECK(f("test"sv, "testing"sv) > f("test"sv, "/testing"sv));
  }

  SECTION("score exact match") {
    // Exact match is kScoreMax
    CHECK(Approx(kScoreMax) == f("abc"sv, "abc"sv));
    CHECK(Approx(kScoreMax) == f("aBc"sv, "abC"sv));
  }

  SECTION("score empty query") {
    // Empty query always results in kScoreMin
    CHECK(Approx(kScoreMin) == f(""sv, ""sv));
    CHECK(Approx(kScoreMin) == f(""sv, "a"sv));
    CHECK(Approx(kScoreMin) == f(""sv, "bb"sv));
  }

  SECTION("score gaps") {
    CHECK(Approx(kScoreGapLeading) == f("a"sv, "*a"sv));
    CHECK(Approx(kScoreGapLeading * 2) == f("a"sv, "*ba"sv));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreGapTrailing) == f("a"sv, "**a*"sv));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreGapTrailing * 2) == f("a"sv, "**a**"sv));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreMatchConsecutive + kScoreGapTrailing * 2)
          == f("aa"sv, "**aa**"sv));
    CHECK(Approx(kScoreGapLeading + kScoreGapLeading + kScoreGapInner + kScoreGapTrailing
                 + kScoreGapTrailing)
          == f("aa"sv, "**a*a**"sv));
  }

  SECTION("score consecutive") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchConsecutive) == f("aa"sv, "*aa"sv));
    CHECK(Approx(kScoreGapLeading + kScoreMatchConsecutive * 2) == f("aaa"sv, "*aaa"sv));
    CHECK(Approx(kScoreGapLeading + kScoreGapInner + kScoreMatchConsecutive)
          == f("aaa"sv, "*a*aa"sv));
  }

  SECTION("score slash") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchSlash) == f("a"sv, "/a"sv));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreMatchSlash) == f("a"sv, "*/a"sv));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreMatchSlash + kScoreMatchConsecutive)
          == f("aa"sv, "a/aa"sv));
  }

  SECTION("score capital") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchCapital) == f("a"sv, "bA"sv));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreMatchCapital) == f("a"sv, "baA"sv));
    CHECK(Approx(kScoreGapLeading * 2 + kScoreMatchCapital + kScoreMatchConsecutive)
          == f("aa"sv, "baAa"sv));
  }

  SECTION("score dot") {
    CHECK(Approx(kScoreGapLeading + kScoreMatchDot) == f("a"sv, ".a"sv));
    CHECK(Approx(kScoreGapLeading * 3 + kScoreMatchDot) == f("a"sv, "*a.a"sv));
    CHECK(Approx(kScoreGapLeading + kScoreGapInner + kScoreMatchDot) == f("a"sv, "*a.a"sv));
  }

  SECTION("score long string") {
    char buf[4096] {};
    memset(buf, 'a', std::size(buf) - 1);
    std::string_view str { buf, std::size(buf) - 1 };
    CHECK(Approx(kScoreMin) == f("aa"sv, str));
    CHECK(Approx(kScoreMin) == f(str, "aa"sv));
    CHECK(Approx(kScoreMin) == f(str, str));
  }

  SECTION("positions consecutive") {
    std::vector<bool> positions {};
    positions.resize(14);
    matchPositions("amo"sv, "app/models/foo"_s, &positions);
    CHECK(positions
          == std::vector<bool> {
              true,
              false,
              false,
              false,
              true,
              true,
              false,
              false,
              false,
              false,
              false,
              false,
              false,
              false,
          });
  }

  SECTION("positions start of word") {
    // We should prefer matching the 'o' in order, since it's the beginning of a word.
    std::vector<bool> positions {};
    positions.resize(16);
    matchPositions("amor"sv, "app/models/order"_s, &positions);
    CHECK(positions
          == std::vector<bool> {
              true,
              false,
              false,
              false,
              true,
              false,
              false,
              false,
              false,
              false,
              false,
              true,
              true,
              false,
              false,
              false,
          });
  }

  SECTION("positions no bonuses") {
    {
      std::vector<bool> positions {};
      positions.resize(4);
      matchPositions("as"sv, "tags"_s, &positions);
      CHECK(positions
            == std::vector<bool> {
                false,
                true,
                false,
                true,
            });
    }

    {
      std::vector<bool> positions {};
      positions.resize(12);
      matchPositions("as"sv, "examples.txt"_s, &positions);
      CHECK(positions
            == std::vector<bool> {
                false,
                false,
                true,
                false,
                false,
                false,
                false,
                true,
                false,
                false,
                false,
                false,
            });
    }
  }

  SECTION("positions multiple candidates start of words") {
    std::vector<bool> positions {};
    positions.resize(9);
    matchPositions("abc"sv, "a/a/b/c/c"_s, &positions);
    CHECK(positions
          == std::vector<bool> {
              false,
              false,
              true,
              false,
              true,
              false,
              true,
              false,
              false,
          });
  }

  SECTION("positions exact match") {
    std::vector<bool> positions {};
    positions.resize(3);
    matchPositions("foo"sv, "foo"_s, &positions);
    CHECK(positions
          == std::vector<bool> {
              true,
              true,
              true,
          });
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
