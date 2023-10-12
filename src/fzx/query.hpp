// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "fzx/aligned_string.hpp"

namespace fzx {

struct Query
{
  struct Builder;

  enum class Type {
    kExact,  ///< ^foo$
    kBegin,  ///< ^foo
    kEnd,    ///< foo$
    kSubstr, ///< 'foo
    kFuzzy,  ///< foo
    // TODO: negated
  };

  struct Item
  {
    Item(std::string_view text, Query::Type type) noexcept
      : mText(text), mType(type) { }

    AlignedString mText;
    Query::Type mType;
  };

  std::vector<Item> mItems;
  // TODO: bump allocator for strings
};

struct Query::Builder
{
  struct Item
  {
    Item(std::string text, Query::Type type) noexcept
      : mText(std::move(text)), mType(type) { }

    std::string mText;
    Query::Type mType;

    friend bool operator==(const Item& a, const Item& b) noexcept
    {
      return a.mType == b.mType && a.mText == b.mText;
    }

    friend bool operator!=(const Item& a, const Item& b) noexcept
    {
      return !(a == b);
    }
  };

  using Items = std::vector<Item>;

  void clear() noexcept;
  void add(std::string text, Query::Type type = Query::Type::kFuzzy);
  [[nodiscard]] bool isImpossible() const noexcept { return mImpossible; }
  [[nodiscard]] const Items& items() const noexcept { return mItems; }

  [[nodiscard]] std::shared_ptr<Query> build() const;

private:
  Items mItems;
  bool mImpossible { false };
};

std::ostream& operator<<(std::ostream& os, Query::Type v);
std::ostream& operator<<(std::ostream& os, const Query::Builder::Item& v);
std::ostream& operator<<(std::ostream& os, const Query::Builder::Items& v);

} // namespace fzx
