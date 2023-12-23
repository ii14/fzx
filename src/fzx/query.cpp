#include "fzx/query.hpp"

#include "fzx/macros.hpp"

namespace fzx {

Query Query::parse(std::string_view s)
{
  if (s.empty())
    return {};

  Query q;

  constexpr char kSeparator = ' ';
  constexpr size_t kInvalid = -1;
  size_t pos = kInvalid;

  auto parseSubstr = [&](std::string_view ss) {
    DEBUG_ASSERT(!ss.empty());
    bool sub = ss.front() == '\'';
    bool begin = ss.front() == '^';
    bool end = ss.back() == '$';
    if (sub) {
      if (s.size() > 1)
        q.add(ss.substr(1, ss.size() - 1), MatchType::kSubstr);
    } else if (begin && end) {
      if (s.size() > 2)
        q.add(ss.substr(1, ss.size() - 2), MatchType::kExact);
    } else if (begin) {
      if (s.size() > 1)
        q.add(ss.substr(1, ss.size() - 1), MatchType::kBegin);
    } else if (end) {
      if (s.size() > 1)
        q.add(ss.substr(0, ss.size() - 1), MatchType::kEnd);
    } else {
      q.add(ss, MatchType::kFuzzy);
    }
  };

  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] != kSeparator && pos == kInvalid) {
      pos = i;
    } else if (s[i] == kSeparator && pos != kInvalid) {
      parseSubstr(s.substr(pos, i - pos));
      pos = kInvalid;
    }
  }

  if (pos != kInvalid) {
    parseSubstr(s.substr(pos));
  }

  return q;
}

bool Query::match(std::string_view s) const
{
  for (const auto& item : mItems) {
    switch (item.mType) {
    case MatchType::kFuzzy:
      if (!matchFuzzy(item.mText, s))
        return false;
      break;
    case MatchType::kSubstr:
      if (!matchSubstr(item.mText, s))
        return false;
      break;
    case MatchType::kBegin:
      if (!matchBegin(item.mText, s))
        return false;
      break;
    case MatchType::kEnd:
      if (!matchEnd(item.mText, s))
        return false;
      break;
    case MatchType::kExact:
      if (!matchExact(item.mText, s))
        return false;
      break;
    }
  }
  return true;
}

Score Query::score(std::string_view s) const
{
  Score sum = 0;
  uint32_t div = 0;

  for (const auto& item : mItems) {
    switch (item.mType) {
    case MatchType::kFuzzy:
      switch (item.mText.size()) {
      default:
        sum += fzx::score(item.mText, s);
        break;
      case 1:
        sum += fzx::score1(item.mText, s);
        break;
#if defined(FZX_SSE2)
      case 2:
      case 3:
      case 4:
        sum += fzx::scoreSSE<4>(item.mText, s);
        break;
      case 5:
      case 6:
      case 7:
      case 8:
        sum += fzx::scoreSSE<8>(item.mText, s);
        break;
      case 9:
      case 10:
      case 11:
      case 12:
        sum += fzx::scoreSSE<12>(item.mText, s);
        break;
      case 13:
      case 14:
      case 15:
      case 16:
        sum += fzx::scoreSSE<16>(item.mText, s);
        break;
#elif defined(FZX_NEON)
      case 2:
      case 3:
      case 4:
        sum += fzx::scoreNeon<4>(item.mText, s);
        break;
      case 5:
      case 6:
      case 7:
      case 8:
        sum += fzx::scoreNeon<8>(item.mText, s);
        break;
      case 9:
      case 10:
      case 11:
      case 12:
        sum += fzx::scoreNeon<12>(item.mText, s);
        break;
      case 13:
      case 14:
      case 15:
      case 16:
        sum += fzx::scoreNeon<16>(item.mText, s);
        break;
#endif
      }
      ++div;
      break;
    case MatchType::kSubstr:
    case MatchType::kBegin:
    case MatchType::kEnd:
    case MatchType::kExact:
      break;
    }
  }

  if (div == 0)
    return 0;
  return sum / static_cast<Score>(div);
}

void Query::matchPositions(std::string_view s, std::vector<bool>& positions) const
{
  DEBUG_ASSERT(match(s));
  positions.clear();
  positions.resize(s.size());
  for (const auto& item : mItems) {
    switch (item.mType) {
    case MatchType::kFuzzy:
      fzx::matchPositions(item.mText.str(), s, &positions);
      break;
    case MatchType::kSubstr: {
      DEBUG_ASSERT(s.size() >= item.mText.size());
      int start = matchSubstrIndex(item.mText, s);
      DEBUG_ASSERT(start != -1);
      auto it = positions.begin() + start;
      for (size_t i = 0; i < item.mText.size(); ++i)
        *it++ = true;
      break;
    }
    case MatchType::kBegin: {
      DEBUG_ASSERT(s.size() >= item.mText.size());
      auto it = positions.begin();
      for (size_t i = 0; i < item.mText.size(); ++i)
        *it++ = true;
      break;
    }
    case MatchType::kEnd: {
      DEBUG_ASSERT(s.size() >= item.mText.size());
      auto it = positions.rbegin();
      for (size_t i = 0; i < item.mText.size(); ++i)
        *it++ = true;
      break;
    }
    case MatchType::kExact:
      for (auto&& p : positions)
        p = true;
      break;
    }
  }
}

} // namespace fzx
