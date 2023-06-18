#include "fzx/util.hpp"

#include <cstdio>
#include <cstdlib>

namespace fzx::detail {

// NOLINTNEXTLINE(google-runtime-int)
[[noreturn]] void assertFail(const char* expr, const char* file, unsigned long line)
{
  std::fprintf(stderr, "fzx: Assertion failed: %s:%lu: %s\n", file, line, expr);
  std::abort();
}

// NOLINTNEXTLINE(google-runtime-int)
[[noreturn]] void assumeFail(const char* expr, const char* file, unsigned long line)
{
  std::fprintf(stderr, "fzx: Assumption failed: %s:%lu: %s\n", file, line, expr);
  std::abort();
}

} // namespace fzx::detail
