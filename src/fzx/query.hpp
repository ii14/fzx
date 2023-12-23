#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "fzx/aligned_string.hpp"
#include "fzx/match.hpp"
#include "fzx/score.hpp"

namespace fzx {

enum class MatchType : uint8_t {
  kFuzzy, ///< `foo`
  kSubstr, ///< `'foo`
  kBegin, ///< `^foo`
  kEnd, ///< `foo$`
  kExact, ///< `^foo$`
};

struct Query
{
  struct Item
  {
    MatchType mType;
    AlignedString mText; // TODO: arena allocator

    Item(MatchType type, AlignedString text) : mType(type), mText(std::move(text)) { }

    friend bool operator==(const Item& a, const Item& b) noexcept
    {
      return a.mType == b.mType && a.mText == b.mText;
    }

    friend bool operator!=(const Item& a, const Item& b) noexcept
    {
      return a.mType != b.mType || a.mText != b.mText;
    }
  };

  void clear() noexcept { mItems.clear(); }

  void add(AlignedString text, MatchType type = MatchType::kFuzzy)
  {
    mItems.emplace_back(type, std::move(text));
  }

  [[nodiscard]] static Query parse(std::string_view s);

  [[nodiscard]] bool empty() const noexcept { return mItems.empty(); }
  [[nodiscard]] const std::vector<Item>& items() const noexcept { return mItems; }

  [[nodiscard]] bool match(std::string_view s) const;
  [[nodiscard]] Score score(std::string_view s) const;
  /// Precondition: match(s) == true
  void matchPositions(std::string_view s, std::vector<bool>& positions) const;

  friend bool operator==(const Query& a, const Query& b) noexcept { return a.mItems == b.mItems; }
  friend bool operator!=(const Query& a, const Query& b) noexcept { return a.mItems != b.mItems; }

private:
  std::vector<Item> mItems;
};

} // namespace fzx
