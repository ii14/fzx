#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <cassert>

#include "queue.hpp"
#include "match.hpp"
#include "allocator.hpp"
#include "choice.hpp"
#include "results.hpp"

using std::string;
using std::vector;

vector<string> example {
#include "example.h"
};


static constexpr size_t WORKERS = 3;
const string PATTERN = "quid";


struct thread_match_t
{
  queue_producer_t p;
  queue_consumer_t c;
  std::thread t;

  static thread_match_t* create(int i)
  {
    return new thread_match_t(i, new_queue_buffer(), new_queue_buffer());
  }

  explicit thread_match_t(int i, queue_buffer_t* ibuf, queue_buffer_t* obuf)
    : p{ibuf}
    , c{obuf}
    , t{std::thread{run, i, ibuf, obuf}}
  {
  }

  static void run(int id, queue_buffer_t* ibuf, queue_buffer_t* obuf)
  {
    printf("start #%d\n", id);
    queue_consumer_t c{ibuf};
    queue_producer_t p{obuf};
    size_t matches = 0;

    do {
      for (c.fetch(); c.pos < c.size(); ++c.pos) {
        if (has_match(PATTERN.c_str(), c.get().value())) {
          match(PATTERN.c_str(), c.get().value());
          p.push(c.get());
          ++matches;
        }
      }
    } while (c.next());

    p.stop();
    printf("exit #%d matches=%ld\n", id, matches);
  }

  void push(choice_t s) { p.push(s); }
  void stop() { p.stop(); }
};

static void merge_thread(vector<thread_match_t*>* threads, results_t* results)
{
  vector<thread_match_t*> ts {*threads};
  size_t selected = 0; // TODO: implement mpsc queue

  while (!ts.empty()) {
    auto& c = ts[selected]->c;
    for (c.fetch(); c.pos < c.size(); ++c.pos)
      results->insert(c.get());
    if (!c.next())
      ts.erase(ts.begin() + selected);
    else
      ++selected;
    if (selected >= ts.size())
      selected = 0;
  }

  printf("done = %ld\n", results->res.size());
}

int main()
{
  allocator_t mem;
  results_t results;

  vector<thread_match_t*> threads {};
  for (size_t i = 0; i < WORKERS; ++i)
    threads.push_back(thread_match_t::create(i));
  std::thread merge{merge_thread, &threads, &results};

  size_t selected = 0;
  for (size_t i = 0; i < 40000; ++i) {
    for (const auto& choice : example) {
      auto s = mem.insert(choice.c_str());
      auto w = threads[selected++];
      // TODO: implement spmc queue
      if (selected >= threads.size())
        selected = 0;
      w->push(s);
    }
  }

  printf("%ld items\n", mem.size());

  // for (size_t i = 0; i < 6; ++i) {
  //   std::this_thread::sleep_for(std::chrono::milliseconds(15));
  //   results_buffer_t res;
  //   results.fetch(res, 0, 10);
  //   printf("items = %ld\n", res.max);
  //   // for (auto item : res.buf)
  //   //   printf("#%u %s\n", item.index(), item.value());
  // }

  for (auto w : threads)
    w->stop();
  for (auto w : threads)
    w->t.join();
  merge.join();
  for (auto w : threads)
    delete w;
}

// vim: sts=2 sw=2 et
