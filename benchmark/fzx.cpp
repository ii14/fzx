#include <algorithm>
#include <chrono>
#include <limits>
#include <mutex>
#include <vector>

#include <unistd.h>

#include "fzx/fzx.hpp"
#include "fzx/helper/line_scanner.hpp"
#include "fzx/macros.hpp"
#include "common.hpp"

namespace chrono = std::chrono;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

struct Sample
{
  chrono::system_clock::time_point mStart;
  chrono::system_clock::time_point mEnd;

  [[nodiscard]] int64_t diff() const
  {
    auto diff = mEnd - mStart;
    return chrono::duration_cast<chrono::microseconds>(diff).count();
  }
};

static fzx::Fzx gFzx;
static std::mutex gMutex;
static std::condition_variable gCv;
static std::vector<Sample> gSamples;
static bool gGatherSample = false;

static void callback(void*) // NOLINT(readability-named-parameter)
{
  std::unique_lock lock { gMutex };
  gFzx.loadResults();
  // Stop benchmark once results are available
  if (gGatherSample) {
    auto ts = chrono::system_clock::now();
    ASSERT(!gSamples.empty());
    gSamples.back().mEnd = ts;
  }
  gCv.notify_all();
}

int main(int argc, char** argv)
{
  // Parse arguments
  std::string_view query = "chromium"sv;
  int samples = 16;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg { argv[i] };
    if (arg == "-q"sv || arg == "--query"sv) {
      if (++i == argc) {
        fprintf(stderr, "Expected argument for --query\n");
        return 1;
      }
      query = std::string_view { argv[i] };
    } else if (arg == "-s"sv || arg == "--samples"sv) {
      if (++i == argc) {
        fprintf(stderr, "Expected argument for --samples\n");
        return 1;
      }
      samples = std::clamp(std::stoi(argv[i]), 1, std::numeric_limits<int>::max());
    } else if (arg == "-t"sv || arg == "--threads"sv) {
      if (++i == argc) {
        fprintf(stderr, "Expected argument for --threads\n");
        return 1;
      }
      gFzx.setThreads(std::stoi(argv[i]));
    } else {
      fprintf(stderr, "Unknown argument: %s\n", argv[i]);
      return 1;
    }
  }

  gFzx.setCallback(callback);
  gFzx.start();

  // Load data from stdin
  {
    if (isatty(0)) {
      noDataError();
      return 1;
    }

    fzx::LineScanner scanner;
    char buf[4096];

    size_t count = 0;
    size_t bytes = 0;
    auto push = [&](std::string_view s) {
      count += 1;
      bytes += s.size();
      gFzx.pushItem(s);
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

    if (gFzx.itemsSize() == 0) {
      noDataError();
      return 1;
    }

    fprintf(stderr, "  items: %zu\n", count);
    fprintf(stderr, "  bytes: %zu\n", bytes);
  }

  size_t matched = -1;

  auto benchmark = [&] {
    std::unique_lock lock { gMutex };

    // Reset query
    if (gFzx.setQuery({}))
      while (!gFzx.synchronized())
        gCv.wait(lock);

    gSamples.push_back({ chrono::system_clock::now(), {} });
    gGatherSample = true;
    ASSERT(gFzx.setQuery(query));
    while (!gFzx.synchronized())
      gCv.wait(lock);
    if (matched != static_cast<size_t>(-1)) {
      ASSERT(matched == gFzx.resultsSize());
    }
    matched = gFzx.resultsSize();
    gGatherSample = false;
  };

  fprintf(stderr, "samples: %d\n", samples);
  for (int i = 0; i < samples; ++i) {
    fprintf(stderr, "\r%d/%d", i, samples);
    benchmark();
  }
  fprintf(stderr, "\r");

  {
    int64_t min = std::numeric_limits<int64_t>::max();
    int64_t max = 0;
    double acc = 0;
    for (const auto& sample : gSamples) {
      auto diff = sample.diff();
      if (min > diff)
        min = diff;
      if (max < diff)
        max = diff;
      acc += static_cast<double>(diff);
    }
    if (min == std::numeric_limits<int64_t>::max())
      min = 0;

    auto print = [](const char* what, size_t time) {
      size_t ms = static_cast<size_t>(time) / 1000;
      size_t us = static_cast<size_t>(time) % 1000;
      fprintf(stderr, "%7s: %zu.%03zu ms\n", what, ms, us);
    };

    fprintf(stderr, "matched: %zu\n", matched);
    print("min", min);
    print("max", max);
    print("mean", static_cast<size_t>(acc / static_cast<double>(gSamples.size())));
    ASSERT(!gSamples.empty());
    if (gSamples.size() == 1) {
      print("median", gSamples.at(0).diff());
    } else if (gSamples.size() % 2 == 1) {
      auto mid = gSamples.size() / 2;
      print("median", gSamples.at(mid).diff());
    } else {
      auto mid = gSamples.size() / 2;
      auto median = static_cast<size_t>((gSamples.at(mid-1).diff() + gSamples.at(mid).diff()) / 2);
      print("median", median);
    }
  }
}
