#include <catch2/catch_test_macros.hpp>
#include "fzx/events.hpp"
#include "fzx/thread.hpp"
#include <cstdio>

TEST_CASE("fzx::Events")
{
  fzx::Events ev;

  size_t items = 0;
  size_t query = 0;

  fzx::Thread t0 { [&] {
    while (true) {
      uint32_t evs = ev.wait();
      if (evs & fzx::Events::kStopEvent)
        return;
      if (evs & fzx::Events::kItemsEvent)
        ++items;
      if (evs & fzx::Events::kQueryEvent)
        ++query;
    }
  } };

  fzx::Thread t1 { [&] {
    for (size_t i = 0; i < 100'000; ++i)
      ev.post(fzx::Events::kItemsEvent);
  } };
  fzx::Thread t2 { [&] {
    for (size_t i = 0; i < 100'000; ++i)
      ev.post(fzx::Events::kQueryEvent);
  } };

  t1.join();
  t2.join();
  ev.post(fzx::Events::kStopEvent);
  t0.join();

  REQUIRE(items > 0);
  REQUIRE(query > 0);
}
