#pragma once

#include <vector>
#include <mutex>
#include <atomic>

#include "choice.hpp"

struct results_buffer_t
{
  size_t pos {0}; // current position
  size_t max {0}; // total amout of items
  std::vector<choice_t> buf; // output buffer
};

struct results_t
{
  std::vector<choice_t> res; // TODO: btree
  mutable std::mutex mut; // TODO: spinlock?

  void fetch(results_buffer_t& output, uint32_t pos, uint16_t len) const
  {
    output.buf.clear();
    output.buf.reserve(len);

    mut.lock();
    const size_t size = res.size();
    if (pos + len > size)
      pos = len > size ? size - len : 0;
    for (size_t i = 0; i < len && pos + i < size; ++i)
      output.buf.push_back(res[pos+i]);
    mut.unlock();

    output.max = size;
    output.pos = pos;
  }

  void insert(choice_t item)
  {
    mut.lock();
    res.push_back(item);
    mut.unlock();
  }
};

// vim: sts=2 sw=2 et
