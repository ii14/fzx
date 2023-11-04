#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>

#include "fzx/match.hpp"

using namespace fzx;

TEST_CASE("fzx::Match")
{
  constexpr auto kMax = std::numeric_limits<float>::infinity();
  constexpr auto kMin = -std::numeric_limits<float>::infinity();

  SECTION("prefers lower index") {
    CHECK(Match{0,    0} < Match{1,    0});
    CHECK(Match{0,    1} < Match{1,    1});
    CHECK(Match{0,    2} < Match{1,    2});
    CHECK(Match{0,   -1} < Match{1,   -1});
    CHECK(Match{0,   -2} < Match{1,   -2});
    CHECK(Match{0, kMax} < Match{1, kMax});
    CHECK(Match{0, kMin} < Match{1, kMin});
  }

  SECTION("prefers higher score") {
    const std::vector<std::pair<float, float>> kTestCases {
      {   -2, kMin },
      {   -1,   -2 },
      {   -1, kMin },
      {    0,   -1 },
      {    0,   -2 },
      {    0, kMin },
      {    1,    0 },
      {    1,   -1 },
      {    1,   -2 },
      {    1, kMin },
      {    2,    1 },
      {    2,    0 },
      {    2,   -1 },
      {    2,   -2 },
      {    2, kMin },
      { kMax,    2 },
      { kMax,    1 },
      { kMax,    0 },
      { kMax,   -1 },
      { kMax,   -2 },
      { kMax, kMin },
    };

    for (const auto& t : kTestCases) {
      auto fst = t.first;
      auto snd = t.second;
      CAPTURE(fst);
      CAPTURE(snd);
      CHECK(Match{0, fst} < Match{0, snd});
      CHECK(Match{1, fst} < Match{0, snd});
      CHECK(Match{0, fst} < Match{1, snd});
    }
  }

  SECTION("decode") {
    CHECK(Match{0, 0}.index() == 0);
    CHECK(Match{1, 0}.index() == 1);

    CHECK(Match{0, 2}.score() == 2);
    CHECK(Match{0, 1}.score() == 1);
    CHECK(Match{0, 0}.score() == 0);
    CHECK(Match{0, -1}.score() == -1);
    CHECK(Match{0, -2}.score() == -2);
    CHECK(Match{0, kMin}.score() == kMin);
    CHECK(Match{0, kMax}.score() == kMax);
  }
}
