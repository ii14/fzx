// Licensed under LGPLv3 - see LICENSE file for details.

#include "fzx/query.hpp"

#include <iomanip>
#include <stdexcept>

#include "fzx/match/match.hpp"

namespace fzx {

void Query::Builder::clear() noexcept
{
  mItems.clear();
  mImpossible = false;
}

void Query::Builder::add(std::string text, Type type)
{
  // Empty Type::Exact are ignored too, like in fzf. They could be made
  // impossible instead, but not sure if there is any point in doing that.
  if (text.empty())
    return;

  if (mImpossible)
    return;

  // TODO: refactor into something less bug prone?

  // TODO: all comparisons here are case sensitive, except matchFuzzyNative
  switch (type) {
  case Type::kExact:
    for (const auto& item : mItems) {
      std::string_view itext { item.mText };
      switch (item.mType) {
      case Type::kExact:
        if (itext != text)
          goto impossible;
        return;
      case Type::kBegin:
        if (itext.size() > text.size())
          goto impossible;
        if (std::string_view{text}.substr(0, itext.size()) != itext)
          goto impossible;
        break;
      case Type::kEnd:
        if (itext.size() > text.size())
          goto impossible;
        if (std::string_view{text}.substr(text.size() - itext.size(), itext.size()) != itext)
          goto impossible;
        break;
      case Type::kSubstr:
        if (itext.size() > text.size())
          goto impossible;
        if (text.find(itext) == std::string::npos)
          goto impossible;
        break;
      case Type::kFuzzy:
        if (itext.size() > text.size())
          goto impossible;
        if (!matchFuzzyNaive(itext, text))
          goto impossible;
        break;
      }
    }
    mItems.clear();
    break;

  case Type::kBegin:
    for (size_t i = 0; i < mItems.size(); /* increment in body */) {
      const auto& item = mItems[i];
      if (item.mType == Type::kExact) {
        std::string_view itext { item.mText };
        if (text.size() > itext.size())
          goto impossible;
        if (itext.substr(0, text.size()) != text)
          goto impossible;
        return;
      } else if (item.mType == Type::kBegin) {
        std::string_view itext { item.mText };
        if (text.size() > itext.size()) {
          if (std::string_view{text}.substr(0, itext.size()) != itext)
            goto impossible;
          mItems.erase(mItems.begin() + static_cast<ptrdiff_t>(i));
          continue;
        } else if (itext.size() > text.size()) {
          if (itext.substr(0, text.size()) != text)
            goto impossible;
          return;
        } else if (text != itext) {
          goto impossible;
        } else {
          return;
        }
      } else if (item.mType == Type::kSubstr) {
        if (text.find(item.mText) != std::string::npos) {
          mItems.erase(mItems.begin() + static_cast<ptrdiff_t>(i));
          continue;
        }
      }
      ++i;
    }
    break;

  case Type::kEnd:
    for (size_t i = 0; i < mItems.size(); /* increment in body */) {
      const auto& item = mItems[i];
      if (item.mType == Type::kExact) {
        std::string_view itext { item.mText };
        if (text.size() > itext.size())
          goto impossible;
        if (itext.substr(itext.size() - text.size(), text.size()) != text)
          goto impossible;
        return;
      } else if (item.mType == Type::kEnd) {
        std::string_view itext { item.mText };
        if (text.size() > itext.size()) {
          if (std::string_view{text}.substr(text.size() - itext.size(), itext.size()) != itext)
            goto impossible;
          mItems.erase(mItems.begin() + static_cast<ptrdiff_t>(i));
          continue;
        } else if (itext.size() > text.size()) {
          if (itext.substr(itext.size() - text.size(), text.size()) != text)
            goto impossible;
          return;
        } else if (text != itext) {
          goto impossible;
        } else {
          return;
        }
      } else if (item.mType == Type::kSubstr) {
        if (text.find(item.mText) != std::string::npos) {
          mItems.erase(mItems.begin() + static_cast<ptrdiff_t>(i));
          continue;
        }
      }
      ++i;
    }
    break;

  case Type::kSubstr:
    for (size_t i = 0; i < mItems.size(); /* increment in body */) {
      const auto& item = mItems[i];
      if (item.mType == Type::kExact) {
        std::string_view itext { item.mText };
        if (text.size() > itext.size())
          goto impossible;
        if (itext.find(text) == std::string::npos)
          goto impossible;
        return;
      } else if (item.mType == Type::kSubstr) {
        if (text.size() > item.mText.size()) {
          if (text.find(item.mText) != std::string::npos) {
            mItems.erase(mItems.begin() + static_cast<ptrdiff_t>(i));
            continue;
          }
        } else if (item.mText.size() > text.size()) {
          if (item.mText.find(text) != std::string::npos)
            return;
        } else if (text == item.mText) {
          return;
        }
      } else if (item.mType == Type::kBegin || item.mType == Type::kEnd) {
        if (item.mText.find(text) != std::string::npos)
          return;
      }
      ++i;
    }
    break;

  case Type::kFuzzy:
    for (const auto& item : mItems) {
      if (item.mType == Type::kExact) {
        std::string_view itext { item.mText };
        if (text.size() > itext.size())
          goto impossible;
        if (!matchFuzzyNaive(text, itext))
          goto impossible;
        return;
      } else if (item.mType == Type::kFuzzy) {
        if (text == item.mText)
          return;
      }
      // TODO: merge with begin, end, substr
    }
    break;
  }

  mItems.emplace_back(std::move(text), type);
  return;

impossible:
  mImpossible = true;
  mItems.clear();
}

std::shared_ptr<Query> Query::Builder::build() const
{
  if (mItems.empty())
    return {};
  auto q = std::make_shared<Query>();
  for (const auto& item : mItems)
    q->mItems.emplace_back(item.mText, item.mType);
  return q;
}

std::ostream& operator<<(std::ostream& os, Query::Type v)
{
  using Type = Query::Type;
  switch (v) {
  case Type::kExact:
    return os << "Exact";
  case Type::kBegin:
    return os << "Begin";
  case Type::kEnd:
    return os << "End";
  case Type::kSubstr:
    return os << "Substr";
  case Type::kFuzzy:
    return os << "Fuzzy";
  }
  return os << "Unknown";
}

std::ostream& operator<<(std::ostream& os, const Query::Builder::Item& v)
{
  return os << v.mType << '(' << std::quoted(v.mText) << ')';
}

std::ostream& operator<<(std::ostream& os, const Query::Builder::Items& v)
{
  if (v.empty())
    return os << "{}";
  os << "{ " << v.at(0);
  for (size_t i = 1; i < v.size(); ++i)
    os << ", " << v.at(1);
  return os << " }";
}

} // namespace fzx