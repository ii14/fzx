#include <benchmark/benchmark.h>

#include <vector>
#include <string>
#include <string_view>

#include <unistd.h>

#include "fzx/match/fzy/fzy.hpp"
#include "fzx/line_scanner.ipp"
#include "fzx/aligned_string.hpp"

using namespace std::string_view_literals;

static std::vector<char> gStrings;
static std::vector<std::string_view> gItems;
static fzx::AlignedString gQuery;

// NOLINTNEXTLINE(readability-identifier-naming)
static void BM_fzy(benchmark::State& s)
{
  const std::string_view query { gQuery.str() };

  for ([[maybe_unused]] auto _ : s) {
    for (const auto& item : gItems) {
      if (fzx::fzy::hasMatch(query, item)) {
        auto res = fzx::fzy::match(query, item);
        benchmark::DoNotOptimize(res);
      }
    }
  }

  s.SetBytesProcessed(static_cast<int64_t>(gStrings.size()) * s.iterations());
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

  for (;;) {
    auto len = fread(buf, 1, std::size(buf), stdin);
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
  std::string_view query { "chromium" };

  // TODO: could be more robust and remove parsed arguments
  //       before passing them to the benchmark library.
  for (int i = 1; i < argc; ++i) {
    std::string_view arg { argv[i] };
    if (arg == "--query"sv) {
      if (++i == argc) {
        fprintf(stderr, "Expected argument for --query\n");
        return 1;
      }
      query = std::string_view { argv[i] };
    }
  }

  gQuery = query;

  if (isatty(0)) {
    fprintf(stderr, "No data, aborting.\n");
    fprintf(stderr, "Provide the data set for the benchmark over stdin.\n");
    return 1;
  }
  readStdin();
  if (gItems.empty()) {
    fprintf(stderr, "No data, aborting.\n");
    fprintf(stderr, "Provide the data set for the benchmark over stdin.\n");
    return 1;
  }

  fprintf(stderr, "Input items: %zu\n", gItems.size() - 64);
  fprintf(stderr, "Input bytes: %zu\n", gStrings.size());

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
