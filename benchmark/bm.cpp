#include <benchmark/benchmark.h>

#include <shared_mutex>
#include <mutex>

#include "fzx/thread.hpp"
#include "fzx/tx.hpp"
#include "fzx/events.hpp"
#include "fzx/match/fzy.hpp"

static constexpr auto kIterations = 1'000'000;

static void BM_spsc_baseline(benchmark::State& s)
{
  std::mutex mx;
  fzx::Thread::pin(0);
  for ([[maybe_unused]] auto _ : s) {
    size_t x = 0;
    fzx::Thread reader { [&]{
      fzx::Thread::pin(1);
      for (size_t i = 0; i < kIterations; ++i) {
        size_t c = 0;
        {
          std::unique_lock lk { mx };
          c = x;
        }
        benchmark::DoNotOptimize(c);
      }
    } };
    for (size_t i = 0; i < kIterations; ++i) {
      std::unique_lock lk { mx };
      x = i;
    }
  }
  s.SetItemsProcessed(s.iterations() * kIterations);
}

static void BM_spsc_tx(benchmark::State& s)
{
  fzx::Thread::pin(0);
  for ([[maybe_unused]] auto _ : s) {
    fzx::Tx<size_t> tx;
    fzx::Thread reader { [&]{
      for (size_t i = 0; i < kIterations; ++i) {
        fzx::Thread::pin(1);
        tx.load();
        auto res = tx.readBuffer();
        benchmark::DoNotOptimize(res);
      }
    } };
    for (size_t i = 0; i < kIterations; ++i) {
      tx.writeBuffer() = i;
      tx.commit();
    }
  }
  s.SetItemsProcessed(s.iterations() * kIterations);
}

// Different results on different machines:
//
// Intel(R) Core(TM) i5-2520M CPU @ 2.50GHz
// BM_spsc_baseline    1853533 ns      1795442 ns          396 bytes_per_second=5.31164M/s
// BM_spsc_tx           387814 ns       317646 ns         2238 bytes_per_second=30.0232M/s
//
// AMD Ryzen 7 3700X 8-Core Processor
// BM_spsc_baseline     672020 ns       527124 ns         1083 bytes_per_second=18.092M/s
// BM_spsc_tx           944793 ns       901578 ns          781 bytes_per_second=10.5778M/s
//
// TODO: Pinning threads fixes the problem and makes atomics faster than mutexes again,
// but it seems to make SPMC slower on lower amounts of data. I think this probably just
// indicates that this setup sucks dick though, and we get the overhead from spawning and
// joining threads, so that has to be fixed. Do this properly and redo everything again.
//
// Also it's going to be really fun when I actually benchmark everything in the full context
// against mutexes and it will turn out that none of this lockless shit that is 5-30x faster
// on microbenchmarks will make any actual difference in the context of the full application,
// and I'll remove everything to make the code simpler lol. It's fun to write it though.
BENCHMARK(BM_spsc_baseline);
BENCHMARK(BM_spsc_tx);

static void BM_events(benchmark::State& s)
{
  fzx::Thread::pin(0);
  for ([[maybe_unused]] auto _ : s) {
    fzx::Events w;

    fzx::Thread t { [&]{
      fzx::Thread::pin(1);
      while (true)
        if (w.wait() & 0x1)
          return;
    } };

    for (size_t i = 0; i < kIterations; ++i)
      w.post(0x2);
    w.post(0x1);
  }
  s.SetItemsProcessed(s.iterations() * kIterations);
}
BENCHMARK(BM_events);

BENCHMARK_MAIN();
