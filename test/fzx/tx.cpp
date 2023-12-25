#include <catch2/catch_test_macros.hpp>
#include "fzx/tx.hpp"
#include "thread.hpp"

TEST_CASE("fzx::Tx")
{
  SECTION("can store and load values") {
    fzx::Tx<size_t> tx;

    tx.writeBuffer() = 42;
    tx.commit();

    REQUIRE(tx.load());
    REQUIRE(tx.readBuffer() == 42);

    REQUIRE(!tx.load());
    REQUIRE(tx.readBuffer() == 42);

    tx.writeBuffer() = 12;
    tx.commit();

    REQUIRE(tx.load());
    REQUIRE(tx.readBuffer() == 12);
  }

  SECTION("has no data races") {
    fzx::Tx<size_t> tx;
    constexpr auto kIterations = 100'000;

    fzx::Thread reader { [&] {
      size_t last = 0;
      for (size_t i = 0; i < kIterations; ++i) {
        bool res = tx.load();
        const size_t& cur = tx.readBuffer();
        if (res) {
          REQUIRE(cur > last);
        } else {
          REQUIRE(cur == last);
        }
        last = cur;
      }
    } };

    for (size_t i = 0; i < kIterations; ++i) {
      tx.writeBuffer() = i + 1;
      tx.commit();
    }
  }
}
