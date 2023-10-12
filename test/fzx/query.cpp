#include "fzx/query.hpp"
// std::ostream overloads have to be defined before catch
#include <catch2/catch_test_macros.hpp>

using namespace std::string_literals;

TEST_CASE("fzx::Query")
{
  using Builder = fzx::Query::Builder;
  using Type = fzx::Query::Type;
  using Item = Builder::Item;
  using Items = Builder::Items;

  // NOLINTBEGIN(readability-identifier-naming)
  auto Exact  = [](std::string s) -> Item { return { std::move(s), Type::kExact }; };
  auto Begin  = [](std::string s) -> Item { return { std::move(s), Type::kBegin }; };
  auto End    = [](std::string s) -> Item { return { std::move(s), Type::kEnd }; };
  auto Substr = [](std::string s) -> Item { return { std::move(s), Type::kSubstr }; };
  auto Fuzzy  = [](std::string s) -> Item { return { std::move(s), Type::kFuzzy }; };
  // NOLINTEND(readability-identifier-naming)

  const std::vector<Type> kTypes {
    Type::kExact,
    Type::kBegin,
    Type::kEnd,
    Type::kSubstr,
    Type::kFuzzy,
  };

  SECTION("empty text is ignored") {
    for (auto type : kTypes) {
      CAPTURE(type);
      Builder q;
      q.add(""s, type);
      CHECK(q.items().empty());
    }
  }

  SECTION("items are added") {
    for (auto type : kTypes) {
      CAPTURE(type);
      Builder q;
      q.add("123"s, type);
      CHECK(q.items() == Items { { "123"s, type } });
      CHECK(!q.isImpossible());
    }
  }

  SECTION("de-duplication") {
    for (auto type : kTypes) {
      CAPTURE(type);
      Builder q;
      q.add("123"s, type);
      q.add("123"s, type);
      CHECK(q.items() == Items { { "123"s, type } });
      CHECK(!q.isImpossible());
    }
  }

  SECTION("impossible combinations") {
    const std::vector<Items> kTestCases {
      { Exact("123"s), Exact("12"s) },
      { Exact("123"s), Exact("1234"s) },
      { Exact("123"s), Exact("456"s) },

      { Exact("123"s), Begin("4"s) },
      { Exact("123"s), Begin("456"s) },
      { Exact("123"s), Begin("1234"s) },

      { Exact("123"s), End("4"s) },
      { Exact("123"s), End("456"s) },
      { Exact("123"s), End("0123"s) },

      { Exact("123"s), Substr("4"s) },
      { Exact("123"s), Substr("456"s) },
      { Exact("123"s), Substr("1234"s) },

      { Exact("123"s), Fuzzy("4"s) },
      { Exact("123"s), Fuzzy("456"s) },
      { Exact("123"s), Fuzzy("1234"s) },

      { Begin("123"s), Begin("4"s) },
      { Begin("123"s), Begin("45"s) },
      { Begin("123"s), Begin("456"s) },
      { Begin("123"s), Begin("4567"s) },

      { End("123"s), End("4"s) },
      { End("123"s), End("45"s) },
      { End("123"s), End("456"s) },
      { End("123"s), End("4567"s) },
    };

    for (const auto& items : kTestCases) {
      REQUIRE(items.size() == 2);
      const auto& a = items.at(0);
      const auto& b = items.at(1);

      {
        INFO("items: " << a << ", " << b);
        Builder q;
        q.add(a.mText, a.mType);
        q.add(b.mText, b.mType);
        CHECK(q.isImpossible());
        CHECK(q.items().empty());
      }

      {
        INFO("items: " << b << ", " << a);
        Builder q;
        q.add(b.mText, b.mType);
        q.add(a.mText, a.mType);
        CHECK(q.isImpossible());
        CHECK(q.items().empty());
      }
    }
  }

  SECTION("merging") {
    struct TestCase
    {
      Item mA;
      Item mB;
      Items mResult;
    };

    const std::vector<TestCase> kTestCases {
      { Exact("123"s), Begin("123"s), { Exact("123"s) } },
      { Exact("123"s), Begin("12"s), { Exact("123"s) } },
      { Exact("123"s), Begin("1"s), { Exact("123"s) } },
      { Exact("123"s), End("123"s), { Exact("123"s) } },
      { Exact("123"s), End("23"s), { Exact("123"s) } },
      { Exact("123"s), End("3"s), { Exact("123"s) } },
      { Exact("123"s), Substr("123"s), { Exact("123"s) } },
      { Exact("123"s), Substr("12"s), { Exact("123"s) } },
      { Exact("123"s), Substr("23"s), { Exact("123"s) } },
      { Exact("123"s), Substr("2"s), { Exact("123"s) } },
      { Exact("123"s), Fuzzy("123"s), { Exact("123"s) } },
      { Exact("123"s), Fuzzy("13"s), { Exact("123"s) } },
      { Begin("123"s), Begin("12"s), { Begin("123"s) } },
      { Begin("123"s), Begin("1"s), { Begin("123"s) } },
      { Begin("123"s), Substr("12"s), { Begin("123"s) } },
      { End("123"s), End("23"s), { End("123"s) } },
      { End("123"s), End("3"s), { End("123"s) } },
      { End("123"s), Substr("23"s), { End("123"s) } },
      { Substr("123"s), Substr("23"s), { Substr("123"s) } },
    };

    for (const auto& t : kTestCases) {
      const auto& a = t.mA;
      const auto& b = t.mB;
      const auto& result = t.mResult;

      {
        INFO("items: " << a << ", " << b);
        Builder q;
        q.add(a.mText, a.mType);
        q.add(b.mText, b.mType);
        CHECK(q.items() == result);
        CHECK(!q.isImpossible());
      }

      {
        INFO("items: " << b << ", " << a);
        Builder q;
        q.add(b.mText, b.mType);
        q.add(a.mText, a.mType);
        CHECK(q.items() == result);
        CHECK(!q.isImpossible());
      }
    }
  }

  SECTION("appending") {
    const std::vector<std::pair<Type, Type>> kTestCases {
      { Type::kBegin, Type::kEnd },
      { Type::kBegin, Type::kSubstr },
      { Type::kBegin, Type::kFuzzy },
      { Type::kEnd, Type::kBegin },
      { Type::kEnd, Type::kSubstr },
      { Type::kEnd, Type::kFuzzy },
      { Type::kSubstr, Type::kBegin },
      { Type::kSubstr, Type::kEnd },
      { Type::kSubstr, Type::kSubstr },
      { Type::kSubstr, Type::kFuzzy },
      { Type::kFuzzy, Type::kBegin },
      { Type::kFuzzy, Type::kEnd },
      { Type::kFuzzy, Type::kSubstr },
      { Type::kFuzzy, Type::kFuzzy },
    };

    for (auto [a, b] : kTestCases) {
      CAPTURE(a);
      CAPTURE(b);
      Builder q;
      q.add("123"s, a);
      q.add("456"s, b);
      CHECK(q.items() == Items { { "123"s, a }, { "456"s, b } });
      CHECK(!q.isImpossible());
    }
  }
}
