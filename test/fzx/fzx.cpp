#include <catch2/catch_test_macros.hpp>
#include "fzx/fzx.hpp"
#include <unistd.h>
#include <chrono>
#include <iostream>

namespace chrono = std::chrono;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

static timespec toTimeval(chrono::nanoseconds duration)
{
  timespec ts {};
  auto sec = chrono::duration_cast<chrono::seconds>(duration);
  ts.tv_sec = sec.count();
  ts.tv_nsec = chrono::duration_cast<chrono::nanoseconds>(duration - sec).count();
  return ts;
}

static bool wait(int fd, chrono::nanoseconds timeout)
{
  fd_set fds {};
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  auto ts = toTimeval(timeout);
  int res = pselect(fd + 1, &fds, nullptr, nullptr, &ts, nullptr);
  if (res <= 0)
    return false;
  ASSERT(FD_ISSET(fd, &fds));
  return true;
}

TEST_CASE("fzx::Fzx")
{
  fzx::Fzx f;

  SECTION("starts and stops") {
    f.start();
    f.stop();
  }

  SECTION("") {
    f.start();

    f.pushItem("foo"sv);
    f.pushItem("bar"sv);
    f.pushItem("baz"sv);
    f.commitItems();
    REQUIRE(f.itemsSize() == 3);
    REQUIRE(f.getItem(0) == "foo"sv);
    REQUIRE(f.getItem(1) == "bar"sv);
    REQUIRE(f.getItem(2) == "baz"sv);

    f.setQuery("b"s);

    // TODO: some way of waiting until everything is processed
    std::this_thread::sleep_for(100ms);

    REQUIRE(wait(f.notifyFd(), 500ms));
    REQUIRE(f.loadResults());
    REQUIRE(f.resultsSize() == 2);
    REQUIRE(f.getResult(0).mLine == "bar"sv);
    REQUIRE(f.getResult(1).mLine == "baz"sv);

    f.stop();
  }
}
