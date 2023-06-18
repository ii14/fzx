#pragma once

namespace fzx {

struct Input
{
  bool open() noexcept;
  void close() noexcept;
  [[nodiscard]] int fd() const noexcept { return mFd; }
  [[nodiscard]] bool isOpen() const noexcept { return mFd != -1; }

  Input() = default;

  ~Input() noexcept
  {
    close();
  }

  Input(Input&& b) noexcept
    : mFd(b.mFd)
  {
    b.mFd = -1;
  }

  Input& operator=(Input&& b) noexcept
  {
    close();
    mFd = b.mFd;
    b.mFd = -1;
    return *this;
  }

  Input(const Input&) = delete;
  Input& operator=(const Input&) = delete;

private:
  int mFd { -1 };
};

} // namespace fzx
