#include <catch2/catch_test_macros.hpp>
#include "fzx/util.hpp"

TEST_CASE("fzx::roundPow2")
{
  CHECK(fzx::roundPow2(0UL) == 0UL);

  CHECK(fzx::roundPow2(1UL) == 1UL);

  CHECK(fzx::roundPow2(2UL) == 2UL);

  for (size_t n = 3; n <= 4UL; ++n) {
    CAPTURE(n);
    CHECK(fzx::roundPow2(n) == 4UL);
  }

  for (size_t n = 5; n <= 8UL; ++n) {
    CAPTURE(n);
    CHECK(fzx::roundPow2(n) == 8UL);
  }

  for (size_t n = 9; n <= 16UL; ++n) {
    CAPTURE(n);
    CHECK(fzx::roundPow2(n) == 16UL);
  }

  for (size_t n = 17; n <= 32UL; ++n) {
    CAPTURE(n);
    CHECK(fzx::roundPow2(n) == 32UL);
  }

  for (size_t n = 33; n <= 64UL; ++n) {
    CAPTURE(n);
    CHECK(fzx::roundPow2(n) == 64UL);
  }
}
