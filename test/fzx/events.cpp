#include <catch2/catch_test_macros.hpp>
#include "fzx/events.hpp"
#include "thread.hpp"
#include <cstdio>

TEST_CASE("fzx::Events")
{
  fzx::Events ev;

  size_t items = 0;
  size_t query = 0;

  enum Event {
    kStopEvent = 0x1,
    kItemsEvent = 0x2,
    kQueryEvent = 0x4,
  };

  fzx::Thread t0 { [&] {
    while (true) {
      uint32_t evs = ev.wait();
      if (evs & Event::kItemsEvent)
        ++items;
      if (evs & Event::kQueryEvent)
        ++query;

      // in the actual application this happens first to stop as soon as possible.
      // here we want to guarantee that other events are parsed too, and if we get
      // unlucky, this thread might not get a chance to run before all events came in.
      if (evs & Event::kStopEvent)
        return;
    }
  } };

  fzx::Thread t1 { [&] {
    for (size_t i = 0; i < 100'000; ++i)
      ev.post(Event::kItemsEvent);
  } };
  fzx::Thread t2 { [&] {
    for (size_t i = 0; i < 100'000; ++i)
      ev.post(Event::kQueryEvent);
  } };

  t1.join();
  t2.join();
  ev.post(Event::kStopEvent);
  t0.join();

  REQUIRE(items > 0);
  REQUIRE(query > 0);
}
