#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cassert>

class choice_t
{
  const char* data;

public:
  using index_type = uint32_t;

  choice_t() noexcept
    : data{nullptr}
  {
  }

  choice_t(const char* data) noexcept
    : data{data}
  {
  }

  inline index_type index() const noexcept
  {
    assert(data != nullptr);
    index_type x;
    memcpy(&x, data, sizeof(index_type));
    return x;
  }

  inline const char* value() const noexcept
  {
    assert(data != nullptr);
    return data + sizeof(index_type);
  }

  inline bool empty() const noexcept
  {
    return data == nullptr;
  }

  inline operator bool() const noexcept
  {
    return data != nullptr;
  }
};

// vim: sts=2 sw=2 et
