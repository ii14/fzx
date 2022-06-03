#pragma once

#include <cstddef>
#include <cassert>
#include <vector>

#include "choice.hpp"

class allocator_t
{
  /// max string size
  static constexpr size_t MAX_STR_SIZE = 1024 - sizeof(choice_t::index_type) - 1;

  static constexpr size_t PAGE_SIZE = 1024 * 32;
  struct page_t { char data[PAGE_SIZE]; };

  std::vector<page_t*> pages;
  size_t idx {0}; /// next index / total amount of stored items
  size_t pos {0}; /// byte position in the current page

public:
  explicit allocator_t() {}
  ~allocator_t();

  allocator_t(const allocator_t&) = delete;
  allocator_t& operator=(const allocator_t&) = delete;
  allocator_t(allocator_t&&) = delete;
  allocator_t& operator=(allocator_t&&) = delete;

  /// insert a new item
  choice_t insert(const char* s);
  /// free all allocated memory
  void clear();

  size_t size() const { return idx; }
};

// vim: sts=2 sw=2 et
