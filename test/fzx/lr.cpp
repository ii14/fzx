#include <catch2/catch_test_macros.hpp>
#include "fzx/lr.hpp"
#include "fzx/thread.hpp"
#include <thread>

TEST_CASE("fzx::LR")
{
  SECTION("can store and load values")
  {
    fzx::LR<size_t> lr;

    {
      size_t x; // NOLINT(cppcoreguidelines-init-variables)
      lr.load(x);
      REQUIRE(x == 0); // default initialized
    }

    {
      size_t x = 42;
      lr.store(x);
    }

    {
      size_t x; // NOLINT(cppcoreguidelines-init-variables)
      lr.load(x);
      REQUIRE(x == 42);
    }

    {
      size_t x = 12;
      lr.store(x);
    }

    {
      size_t x; // NOLINT(cppcoreguidelines-init-variables)
      lr.load(x);
      REQUIRE(x == 12);
      lr.load(x);
      REQUIRE(x == 12);
    }

    {
      size_t x = 6;
      lr.store(x);
      x = 5;
      lr.store(x);
    }

    {
      size_t x; // NOLINT(cppcoreguidelines-init-variables)
      lr.load(x);
      REQUIRE(x == 5);
    }
  }

  SECTION("has no data races")
  {
    fzx::LR<size_t> lr;
    constexpr auto kIterations = 100'000;

    auto reader = [&lr] {
      volatile size_t r = 0;
      size_t x = 0;
      for (size_t i = 0; i < kIterations; ++i) {
        lr.load(x);
        r = x; // make sure it's not optimized out
      }
      UNUSED(r);
    };

    fzx::Thread t1 { reader };
    fzx::Thread t2 { reader };

    for (size_t i = 0; i < kIterations; ++i) {
      size_t x = i;
      lr.store(x);
    }
  }
}
