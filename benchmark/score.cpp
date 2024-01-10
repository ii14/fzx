#include <benchmark/benchmark.h>

#include <vector>
#include <string>
#include <string_view>

#include <unistd.h>

#include "fzx/aligned_string.hpp"
#include "fzx/helper/line_scanner.hpp"
#include "fzx/match.hpp"
#include "fzx/score.hpp"
#include "fzx/items.hpp"

#include "common.hpp"

using namespace std::string_view_literals;

static fzx::AlignedString gQuery;
static fzx::Items gItems;

// NOLINTNEXTLINE(readability-identifier-naming)
static void BM_fzy(benchmark::State& s)
{
  const std::string_view query { gQuery.str() };

  for ([[maybe_unused]] auto _ : s) {
    for (size_t i = 0; i < gItems.size(); ++i) {
      auto item = gItems.at(i);
      if (fzx::matchFuzzy(query, item)) {
        auto res = fzx::score(query, item);
        benchmark::DoNotOptimize(res);
      }
    }
  }

  // s.SetBytesProcessed(static_cast<int64_t>(gStrings.size()) * s.iterations());
  s.SetItemsProcessed(static_cast<int64_t>(gItems.size()) * s.iterations());
}

BENCHMARK(BM_fzy);

/// Read dataset from stdin
static void readStdin()
{
  struct Range { size_t mOffset, mLength; };
  std::vector<Range> ranges;

  fzx::LineScanner scanner;
  char buf[4096];

  auto push = [&](std::string_view s) {
    gItems.push(s);
  };

  fprintf(stderr, "reading stdin... ");
  for (;;) {
    auto len = fread(buf, 1, std::size(buf), stdin);
    if (len > 0) {
      scanner.feed({ buf, len }, push);
    } else {
      scanner.finalize(push);
      break;
    }
  }
  fprintf(stderr, "done\n");
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
    noDataError();
    return 1;
  }
  readStdin();
  if (gItems.size() == 0) {
    noDataError();
    return 1;
  }

  fprintf(stderr, "Input items: %zu\n", gItems.size() - 64);
  // fprintf(stderr, "Input bytes: %zu\n", gStrings.size());

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
