#include <benchmark/benchmark.h>

#include <shared_mutex>
#include <mutex>

#include "fzx/lr.hpp"
#include "fzx/thread.hpp"
#include "fzx/tx_value.hpp"

static constexpr auto kIterations = 10'000;

static void BM_spmc_full_baseline(benchmark::State& s)
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

static void BM_spmc_full_lr(benchmark::State& s)
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

static void BM_spmc_reader_baseline(benchmark::State& s)
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

static void BM_spmc_reader_lr(benchmark::State& s)
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
BENCHMARK(BM_spmc_full_baseline) ARGS;
BENCHMARK(BM_spmc_full_lr) ARGS;
BENCHMARK(BM_spmc_reader_baseline) ARGS;
BENCHMARK(BM_spmc_reader_lr) ARGS;

static void BM_spsc_baseline(benchmark::State& s)
{
  std::mutex mx;
  for ([[maybe_unused]] auto _ : s) {
    size_t x = 0;
    fzx::Thread reader { [&]{
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
  s.SetBytesProcessed(s.iterations() * kIterations);
}

static void BM_spsc_tx(benchmark::State& s)
{
  for ([[maybe_unused]] auto _ : s) {
    fzx::TxValue<size_t> tx;
    fzx::Thread reader { [&]{
      for (size_t i = 0; i < kIterations; ++i) {
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
  s.SetBytesProcessed(s.iterations() * kIterations);
}

// TODO: different results on different machines:
//
// Intel(R) Core(TM) i5-2520M CPU @ 2.50GHz
// BM_spsc_baseline    1853533 ns      1795442 ns          396 bytes_per_second=5.31164M/s
// BM_spsc_tx           387814 ns       317646 ns         2238 bytes_per_second=30.0232M/s
//
// AMD Ryzen 7 3700X 8-Core Processor
// BM_spsc_baseline     672020 ns       527124 ns         1083 bytes_per_second=18.092M/s
// BM_spsc_tx           944793 ns       901578 ns          781 bytes_per_second=10.5778M/s
BENCHMARK(BM_spsc_baseline);
BENCHMARK(BM_spsc_tx);

BENCHMARK_MAIN();
