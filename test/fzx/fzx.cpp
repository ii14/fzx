#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>

#include "fzx/fzx.hpp"
#include "fzx/macros.hpp"

namespace chrono = std::chrono;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace {

struct Notify
{
  std::mutex mMutex;
  std::condition_variable mCv;
  bool mActive { false };

  void notify()
  {
    std::unique_lock lock { mMutex };
    mActive = true;
    mCv.notify_one();
  }

  template <typename Rep, typename Period>
  bool wait(const chrono::duration<Rep, Period>& reltime)
  {
    std::unique_lock lock { mMutex };
    bool res = mCv.wait_for(lock, reltime, [this] { return mActive; });
    mActive = false;
    return res;
  }
};

} // namespace

TEST_CASE("fzx::Fzx")
{
  fzx::Fzx f;
  Notify notify;

  f.setCallback([](void* userData) { static_cast<Notify*>(userData)->notify(); }, &notify);

  SECTION("starts and stops") {
    f.start();
    f.stop();
  }

  SECTION("") {
    f.start();

    f.pushItem("foo"sv);
    f.pushItem("bar"sv);
    f.pushItem("baz"sv);
    f.commit();
    REQUIRE(f.itemsSize() == 3);
    REQUIRE(f.getItem(0) == "foo"sv);
    REQUIRE(f.getItem(1) == "bar"sv);
    REQUIRE(f.getItem(2) == "baz"sv);

    f.setQuery("b"s);

    for (unsigned i = 0; i < 2; ++i) {
      REQUIRE(notify.wait(100ms));
      REQUIRE(f.loadResults());
      if (!f.processing())
        break;
    }
    REQUIRE(!f.processing());

    REQUIRE(f.resultsSize() == 2);
    REQUIRE(f.getResult(0).mLine == "bar"sv);
    REQUIRE(f.getResult(1).mLine == "baz"sv);

    f.stop();
  }
}
