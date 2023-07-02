#include <benchmark/benchmark.h>

#include "fzx/lr.hpp"
#include "fzx/thread.hpp"

#include <shared_mutex>
#include <mutex>

static constexpr auto kIterations = 10'000;

static void BM_full_baseline(benchmark::State& s)
{
  const size_t threads = s.range(0);
  std::shared_mutex mx;

  for ([[maybe_unused]] auto _ : s) {
    size_t r = 0;

    auto reader = [&r, &mx] {
      size_t x = 0;
      for (size_t i = 0; i < kIterations; ++i) {
        std::shared_lock lk { mx };
        x = r;
        benchmark::DoNotOptimize(x);
      }
    };

    std::vector<fzx::Thread> ts;
    for (size_t i = 0; i < threads; ++i)
      ts.emplace_back(reader);

    for (size_t i = 0; i < kIterations; ++i) {
      std::unique_lock lk { mx };
      r = i;
    }
  }

  s.SetBytesProcessed(s.iterations() * kIterations);
}

static void BM_full_lr(benchmark::State& s)
{
  const size_t threads = s.range(0);

  for ([[maybe_unused]] auto _ : s) {
    fzx::LR<size_t> lr;

    auto reader = [&lr] {
      size_t x = 0;
      for (size_t i = 0; i < kIterations; ++i) {
        lr.load(x);
        benchmark::DoNotOptimize(x);
      }
    };

    std::vector<fzx::Thread> ts;
    for (size_t i = 0; i < threads; ++i)
      ts.emplace_back(reader);

    for (size_t i = 0; i < kIterations; ++i) {
      size_t x = i;
      lr.store(x);
    }
  }

  s.SetBytesProcessed(s.iterations() * kIterations);
}

static void BM_reader_baseline(benchmark::State& s)
{
  const size_t threads = s.range(0);
  std::shared_mutex mx;

  for ([[maybe_unused]] auto _ : s) {
    size_t r = 0;
    std::atomic<size_t> running { threads };

    auto reader = [&r, &mx, &running] {
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
      ts.emplace_back(reader);

    for (size_t i = 0; running.load() > 0; ++i) {
      std::unique_lock lk { mx };
      r = i;
    }
  }

  s.SetBytesProcessed(s.iterations() * kIterations);
}

static void BM_reader_lr(benchmark::State& s)
{
  const size_t threads = s.range(0);

  for ([[maybe_unused]] auto _ : s) {
    fzx::LR<size_t> lr;
    std::atomic<size_t> running { threads };

    auto reader = [&lr, &running] {
      size_t x = 0;
      for (size_t i = 0; i < kIterations; ++i) {
        lr.load(x);
        benchmark::DoNotOptimize(x);
      }
      running.fetch_sub(1);
    };

    std::vector<fzx::Thread> ts;
    for (size_t i = 0; i < threads; ++i)
      ts.emplace_back(reader);

    for (size_t i = 0; running.load() > 0; ++i) {
      size_t x = i;
      lr.store(x);
    }
  }

  s.SetBytesProcessed(s.iterations() * kIterations);
}

#define ARGS ->Arg(1)->Arg(2)->Arg(3)->Arg(4)
BENCHMARK(BM_full_baseline) ARGS;
BENCHMARK(BM_full_lr) ARGS;
BENCHMARK(BM_reader_baseline) ARGS;
BENCHMARK(BM_reader_lr) ARGS;

BENCHMARK_MAIN();
