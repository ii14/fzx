#include <benchmark/benchmark.h>

#include <shared_mutex>
#include <mutex>

#include "fzx/lr.hpp"
#include "fzx/thread.hpp"
#include "fzx/tx_value.hpp"
#include "fzx/events.hpp"

static void pinThread(int cpu)
{
  UNUSED(cpu);
#if 0
  if (cpu < 0)
    return;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);

  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
}

static constexpr auto kIterations = 1'000'000;

static void BM_spmc_full_baseline(benchmark::State& s)
{
  const size_t threads = s.range(0);
  std::shared_mutex mx;
  pinThread(0);

  for ([[maybe_unused]] auto _ : s) {
    size_t r = 0;

    auto reader = [&r, &mx](int cpu) {
      pinThread(cpu);
      size_t x = 0;
      for (size_t i = 0; i < kIterations; ++i) {
        std::shared_lock lk { mx };
        x = r;
        benchmark::DoNotOptimize(x);
      }
    };

    std::vector<fzx::Thread> ts;
    for (size_t i = 0; i < threads; ++i)
      ts.emplace_back(reader, i + 1);

    for (size_t i = 0; i < kIterations; ++i) {
      std::unique_lock lk { mx };
      r = i;
    }
  }

  s.SetItemsProcessed(s.iterations() * kIterations);
}

static void BM_spmc_full_lr(benchmark::State& s)
{
  const size_t threads = s.range(0);
  pinThread(0);

  for ([[maybe_unused]] auto _ : s) {
    fzx::LR<size_t> lr;

    auto reader = [&lr](int cpu) {
      pinThread(cpu);
      size_t x = 0;
      for (size_t i = 0; i < kIterations; ++i) {
        lr.load(x);
        benchmark::DoNotOptimize(x);
      }
    };

    std::vector<fzx::Thread> ts;
    for (size_t i = 0; i < threads; ++i)
      ts.emplace_back(reader, i + 1);

    for (size_t i = 0; i < kIterations; ++i) {
      size_t x = i;
      lr.store(x);
    }
  }

  s.SetItemsProcessed(s.iterations() * kIterations);
}

static void BM_spmc_reader_baseline(benchmark::State& s)
{
  const size_t threads = s.range(0);
  std::shared_mutex mx;
  pinThread(0);

  for ([[maybe_unused]] auto _ : s) {
    size_t r = 0;
    std::atomic<size_t> running { threads };

    auto reader = [&r, &mx, &running](int cpu) {
      pinThread(cpu);
      size_t x = 0;
      for (size_t i = 0; i < kIterations; ++i) {
        std::shared_lock lk { mx };
        x = r;
        benchmark::DoNotOptimize(x);
      }
      running.fetch_sub(1);
    };

    std::vector<fzx::Thread> ts;
    for (size_t i = 0; i < threads; ++i)
      ts.emplace_back(reader, i + 1);

    for (size_t i = 0; running.load() > 0; ++i) {
      std::unique_lock lk { mx };
      r = i;
    }
  }

  s.SetItemsProcessed(s.iterations() * kIterations);
}

static void BM_spmc_reader_lr(benchmark::State& s)
{
  const size_t threads = s.range(0);
  pinThread(0);

  for ([[maybe_unused]] auto _ : s) {
    fzx::LR<size_t> lr;
    std::atomic<size_t> running { threads };

    auto reader = [&lr, &running](int cpu) {
      pinThread(cpu);
      size_t x = 0;
      for (size_t i = 0; i < kIterations; ++i) {
        lr.load(x);
        benchmark::DoNotOptimize(x);
      }
      running.fetch_sub(1);
    };

    std::vector<fzx::Thread> ts;
    for (size_t i = 0; i < threads; ++i)
      ts.emplace_back(reader, i + 1);

    for (size_t i = 0; running.load() > 0; ++i) {
      size_t x = i;
      lr.store(x);
    }
  }

  s.SetItemsProcessed(s.iterations() * kIterations);
}

#define ARGS ->Arg(1)->Arg(2)->Arg(3)->Arg(4)//->Arg(6)->Arg(8)->Arg(12)->Arg(16)
BENCHMARK(BM_spmc_full_baseline) ARGS;
BENCHMARK(BM_spmc_full_lr) ARGS;
BENCHMARK(BM_spmc_reader_baseline) ARGS;
BENCHMARK(BM_spmc_reader_lr) ARGS;

static void BM_spsc_baseline(benchmark::State& s)
{
  std::mutex mx;
  pinThread(0);
  for ([[maybe_unused]] auto _ : s) {
    size_t x = 0;
    fzx::Thread reader { [&]{
      pinThread(1);
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
  pinThread(0);
  for ([[maybe_unused]] auto _ : s) {
    fzx::TxValue<size_t> tx;
    fzx::Thread reader { [&]{
      for (size_t i = 0; i < kIterations; ++i) {
        pinThread(1);
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
  pinThread(0);
  for ([[maybe_unused]] auto _ : s) {
    fzx::Events w;

    fzx::Thread t { [&]{
      pinThread(1);
      while (true)
        if (w.wait() & fzx::Events::kStopEvent)
          return;
    } };

    for (size_t i = 0; i < kIterations; ++i)
      w.post(fzx::Events::kItemsEvent);
    w.post(fzx::Events::kStopEvent);
  }
  s.SetItemsProcessed(s.iterations() * kIterations);
}
BENCHMARK(BM_events);

BENCHMARK_MAIN();
