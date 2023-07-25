#include <benchmark/benchmark.h>

#include <vector>
#include <string>
#include <string_view>

#include "fzx/match/fzy.hpp"
#include "fzx/line_scanner.ipp"

static std::vector<char> gStrings;
static std::vector<std::string_view> gItems;
static std::string gQuery { "/" };

static void BM_fzy(benchmark::State& s)
{
  const std::string_view query = gQuery;

  for ([[maybe_unused]] auto _ : s) {
    for (const auto& item : gItems) {
      if (fzx::fzy::hasMatch(query, item)) {
        auto res = fzx::fzy::match(query, item);
        benchmark::DoNotOptimize(res);
      }
    }
  }

  s.SetBytesProcessed(static_cast<int64_t>(gStrings.size()));
  s.SetItemsProcessed(static_cast<int64_t>(gItems.size()) * s.iterations());
}

BENCHMARK(BM_fzy);

/// Read dataset from stdin
///
/// Not providing the data, because it's ~100MB. I'm testing this on the
/// output of fd from chromium, gecko, llvm, gcc, linux and few others.
static void readStdin()
{
  struct Range { size_t mOffset, mLength; };
  std::vector<Range> ranges;

  fzx::LineScanner scanner;
  char buf[4096];

  auto push = [&](std::string_view s) {
    ranges.push_back({ gStrings.size(), s.size() });
    gStrings.insert(gStrings.end(), s.begin(), s.end());
  };

  for (size_t len = 0;;) {
    len = fread(buf, std::size(buf), 1, stdin);
    if (len > 0) {
      scanner.feed({ buf, len }, push);
    } else {
      scanner.finalize(push);
      break;
    }
  }

  // overallocate 64 bytes, so reading out of bounds with simd doesn't segfault
  gStrings.reserve(gStrings.size() + 64);

  const auto* data = gStrings.data();
  for (auto range : ranges) {
    gItems.emplace_back(data + range.mOffset, range.mLength);
  }
}

int main(int argc, char** argv)
{
  gQuery.reserve(gQuery.size() + 64);
  // TODO: add option for changing the query
  readStdin();
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
