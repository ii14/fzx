#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>

#include "fzx/util.hpp"

namespace fzx {

struct ItemReader;

/// Single-producer, single-consumer, push-only item vector.
struct ItemList
{
  ItemList() noexcept = default;
  ~ItemList() noexcept;

  [[nodiscard]] ItemReader reader() noexcept;

  ItemList(ItemList&&) = delete;
  ItemList& operator=(ItemList&&) = delete;
  ItemList(const ItemList&) = delete;
  ItemList& operator=(const ItemList&) = delete;

  struct Item
  {
    uint32_t mOffset { 0 };
    uint32_t mLength { 0 };
  };

  struct Data
  {
    char* mStrData { nullptr };
    size_t mStrSize { 0 };
    size_t mStrCap { 0 };

    Item* mItemData { nullptr };
    size_t mItemSize { 0 };
    size_t mItemCap { 0 };
  };

  void push(std::string_view s);
  void commit(std::memory_order memoryOrder = std::memory_order_release) noexcept;

  [[nodiscard]] size_t size() const noexcept
  {
    return mBuffers[mWrite].mItemSize;
  }

  [[nodiscard]] std::string_view at(size_t i) const noexcept
  {
    const auto& buf = mBuffers[mWrite];
    DEBUG_ASSERT(i < buf.mItemSize);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto& item = *std::launder(reinterpret_cast<const ItemList::Item*>(buf.mItemData) + i);
    return std::string_view { buf.mStrData + item.mOffset, item.mLength };
  }

  [[nodiscard]] std::string_view operator[](size_t i) const noexcept
  {
    return at(i);
  }

private:
  void strRelease(char* ptr) const noexcept;
  void itemRelease(ItemList::Item* ptr) const noexcept;

private:
  std::array<Data, 3> mBuffers {};
  uint8_t mWrite { 0 };
  uint8_t mRead { 1 };
  std::atomic<uint8_t> mUnused { 2 };

  friend struct ItemReader;
};

struct ItemReader
{
  explicit ItemReader(ItemList& list) noexcept : mPtr(&list) { }

  void load(std::memory_order memoryOrder = std::memory_order_acquire) const noexcept
  {
    mPtr->mRead = mPtr->mUnused.exchange(mPtr->mRead, memoryOrder);
  }

  [[nodiscard]] size_t size() const noexcept
  {
    return mPtr->mBuffers[mPtr->mRead].mItemSize;
  }

  [[nodiscard]] std::string_view at(size_t i) const noexcept
  {
    auto& buf = mPtr->mBuffers[mPtr->mRead];
    DEBUG_ASSERT(i < buf.mItemSize);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto& item = *std::launder(reinterpret_cast<const ItemList::Item*>(buf.mItemData) + i);
    return std::string_view { buf.mStrData + item.mOffset, item.mLength };
  }

  [[nodiscard]] std::string_view operator[](size_t i) const noexcept { return at(i); }

private:
  ItemList* mPtr { nullptr };
};

[[nodiscard]] inline ItemReader ItemList::reader() noexcept
{
  return ItemReader { *this };
}

} // namespace fzx
