#include "fzx/item_list.hpp"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace fzx {

ItemList::~ItemList() noexcept
{
  for (size_t i = 0; i < mBuffers.size(); ++i) {
    if (mBuffers[i].mStrData != nullptr) {
      for (size_t j = i + 1; j < mBuffers.size(); ++j)
        if (mBuffers[j].mStrData == mBuffers[i].mStrData)
          mBuffers[j].mStrData = nullptr;
      std::free(mBuffers[i].mStrData);
    }
    if (mBuffers[i].mItemData != nullptr) {
      for (size_t j = i + 1; j < mBuffers.size(); ++j)
        if (mBuffers[j].mItemData == mBuffers[i].mItemData)
          mBuffers[j].mItemData = nullptr;
      std::free(mBuffers[i].mItemData);
    }
    mBuffers[i] = {};
  }
}

void ItemList::push(std::string_view s)
{
  using Item = ItemList::Item;

  if (s.empty())
    return;

  auto& buf = mBuffers[mWrite];

  if (buf.mStrSize + s.size() > buf.mStrCap) {
    auto strCap = buf.mStrCap == 0 ? 4096 : buf.mStrCap * 2;
    auto* oldData = buf.mStrData;
    auto* newData = static_cast<char*>(std::aligned_alloc(64, strCap));
    if (newData == nullptr)
      throw std::bad_alloc {};

    buf.mStrData = newData;
    buf.mStrCap = strCap;

    if (oldData != nullptr) {
      std::memcpy(newData, oldData, buf.mStrSize);
      strRelease(oldData);
    }
  }

  if (buf.mItemSize + 1 > buf.mItemCap) {
    auto itemCap = buf.mItemCap == 0 ? 512 : buf.mItemCap * 2;
    auto* oldData = buf.mItemData;
    auto* newData = static_cast<Item*>(std::aligned_alloc(64, itemCap * sizeof(Item)));
    if (newData == nullptr)
      throw std::bad_alloc {};

    buf.mItemData = newData;
    buf.mItemCap = itemCap;

    if (oldData != nullptr) {
      std::memcpy(newData, oldData, buf.mItemSize * sizeof(Item));
      itemRelease(oldData);
    }
  }

  std::memcpy(buf.mStrData + buf.mStrSize, s.data(), s.size());
  new (buf.mItemData + buf.mItemSize) Item { uint32_t(buf.mStrSize), uint32_t(s.size()) };
  buf.mStrSize += s.size();
  buf.mItemSize += 1;
}

void ItemList::commit(std::memory_order memoryOrder) noexcept
{
  auto prevIdx = mWrite;
  mWrite = mUnused.exchange(mWrite, memoryOrder);

  auto* oldStrData = mBuffers[mWrite].mStrData;
  auto* oldItemData = mBuffers[mWrite].mItemData;
  mBuffers[mWrite] = mBuffers[prevIdx];
  if (oldStrData != nullptr)
    strRelease(oldStrData);
  if (oldItemData != nullptr)
    itemRelease(oldItemData);
}

void ItemList::strRelease(char* ptr) const noexcept
{
  DEBUG_ASSERT(ptr != nullptr);
  for (const auto& buf : mBuffers)
    if (buf.mStrData == ptr)
      return;
  std::free(ptr);
}

void ItemList::itemRelease(ItemList::Item* ptr) const noexcept
{
  DEBUG_ASSERT(ptr != nullptr);
  for (const auto& buf : mBuffers)
    if (buf.mItemData == ptr)
      return;
  std::free(ptr);
}

} // namespace fzx
