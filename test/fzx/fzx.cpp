#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iostream>
#include <unistd.h>

#include "fzx/eventfd.hpp"
#include "fzx/fzx.hpp"
#include "fzx/macros.hpp"

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
  fzx::EventFd e;
  fzx::Fzx f;

  REQUIRE(e.open().empty());
  f.setCallback([](void* userData) {
    static_cast<fzx::EventFd*>(userData)->notify();
  }, &e);

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
      REQUIRE(wait(e.fd(), 100ms));
      e.consume();
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
