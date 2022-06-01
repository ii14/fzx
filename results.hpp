#pragma once

#include <vector>
#include <mutex>
#include <atomic>

struct output_buffer_t
{
  size_t pos {0};
  size_t len {0};
  size_t max {0};
  const char** buf {nullptr};
};

struct results_t
{
  static constexpr uint32_t MASKSIZE = 0x3FFFFFFF;
  static constexpr uint32_t MASKSTOP = 0x80000000;

  mutable std::mutex mut;
  std::atomic<uint32_t> written;
  std::vector<const char*> res;

  void fetch(output_buffer_t& output, uint32_t pos, uint16_t len) const
  {
    std::unique_lock<std::mutex> lock{mut};
    const size_t size = written;

    if (pos + len > size)
      pos = len > size ? size - len : 0;

    for (size_t i = pos; i < pos + len; ++i)
      output.buf[i] = i < size ? res[i] : nullptr;
    output.max = size;
    output.pos = pos;
    output.len = len;
  }
};

// vim: sts=2 sw=2 et
