#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/core.h>

namespace fzx {

struct TTY
{
  static constexpr int kInvalidFd = -1;

  TTY() = default;
  ~TTY() noexcept;
  TTY(TTY&& b) noexcept;
  TTY& operator=(TTY&& b) noexcept;
  TTY(const TTY&) = delete;
  TTY& operator=(const TTY&) = delete;

  bool open() noexcept;
  void close() noexcept;
  [[nodiscard]] int fd() const noexcept { return mFd; }
  [[nodiscard]] bool isOpen() const noexcept { return mFd != kInvalidFd; }

  void updateSize() noexcept;
  [[nodiscard]] uint16_t width() const noexcept { return mWidth; }
  [[nodiscard]] uint16_t height() const noexcept { return mHeight; }

  [[nodiscard]] std::optional<uint8_t> read() const noexcept;

  void put(char c);
  void put(const char* s);
  void put(std::string_view s);
  void put(const std::string& s);

  template <typename... Ts>
  void put(fmt::format_string<Ts...> fmt, Ts&&... ts)
  {
    fmt::format_to(std::back_inserter(mBuffer), fmt, std::forward<Ts>(ts)...);
  }

  void flush() noexcept;

private:
  int mFd { kInvalidFd };
  uint16_t mWidth { 0 };
  uint16_t mHeight { 0 };
  std::vector<char> mBuffer;
};

} // namespace fzx
