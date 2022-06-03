#include "allocator.hpp"

#include <cstring>
#include <limits>
#include <new>

using index_type = choice_t::index_type;

static constexpr index_type INDEX_TYPE_MAX = std::numeric_limits<index_type>::max();

allocator_t::~allocator_t()
{
  clear();
}

choice_t allocator_t::insert(const char* s)
{
  // TODO: return meaningful errors
  // TODO: optionally insert in bulk

  // max index value reached, can't add any more items
  if (idx >= INDEX_TYPE_MAX - 1)
    return choice_t{};

  // increment index even if the item is invalid, because
  // indexes can be used to associate user data with items.
  const index_type index = idx++;

  // TODO: maybe it should be passed as argument instead
  const size_t len = strlen(s);
  if (len == 0 || len > MAX_STR_SIZE)
    return choice_t{};

  // no page yet or there is no space for this item -> allocate a new page
  if (pages.empty() || pos + len + sizeof(uint32_t) + 1 > PAGE_SIZE) {
    pages.push_back(new page_t); // TODO: catch exceptions
    pos = 0;
  }

  auto &page = pages.back()->data;
  choice_t res{&page[pos]};

  memcpy(&page[pos], &index, sizeof(index_type));
  pos += sizeof(index_type);
  memcpy(&page[pos], s, len);
  pos += len;
  page[pos++] = '\0';

  return res;
}

void allocator_t::clear()
{
  for (auto page : pages)
    delete page;
  pages.clear();
}

// vim: sts=2 sw=2 et
