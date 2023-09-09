// Licensed under LGPLv3 - see LICENSE file for details.

#include "fzx/macros.hpp"

#include <cstdio>
#include <cstdlib>

namespace fzx::detail {

// NOLINTNEXTLINE(google-runtime-int)
[[noreturn]] NOINLINE void assertFail(const char* expr, const char* file, unsigned long line)
{
  std::fprintf(stderr, "fzx: Assertion failed: %s:%lu: %s\n", file, line, expr);
  std::abort();
}

// NOLINTNEXTLINE(google-runtime-int)
[[noreturn]] NOINLINE void assumeFail(const char* expr, const char* file, unsigned long line)
{
  std::fprintf(stderr, "fzx: Assumption failed: %s:%lu: %s\n", file, line, expr);
  std::abort();
}

// NOLINTNEXTLINE(google-runtime-int)
[[noreturn]] NOINLINE void unreachableFail(const char* file, unsigned long line)
{
  std::fprintf(stderr, "fzx: Reached unreachable code: %s:%lu\n", file, line);
  std::abort();
}

} // namespace fzx::detail
