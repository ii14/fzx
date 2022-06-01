#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cassert>

using std::atomic;
using std::string;
using std::vector;

vector<string> example {
#include "example.h"
};

static inline bool match(
        const string& pattern,
        const char* choice)
{
  auto compare = [&](const char *c){
    for (size_t i = 0; i < pattern.size(); ++i)
      if (tolower(c[i]) != tolower(pattern[i]))
        return false;
    return true;
  };

  for (const char *c = choice; *c != '\0'; ++c)
    if (compare(c))
      return true;
  return false;
}

static constexpr size_t WORKERS = 3;
static constexpr size_t BUFSIZE = 4096 - 2;
static constexpr uint32_t MASKSIZE = 0x3FFFFFFF;
static constexpr uint32_t MASKSTOP = 0x80000000;
static constexpr uint32_t MASKNEXT = 0x40000000;

struct queue_buffer_t
{
  const char* data[BUFSIZE] {0};
  queue_buffer_t* next {nullptr};
  atomic<uint32_t> meta {0};

  inline queue_buffer_t* get_next()
  {
    auto buf = next;
    delete this;
    return buf;
  }
};

struct queue_consumer_t
{
  queue_buffer_t* buf;
  uint32_t pos;
  uint32_t meta;

  explicit queue_consumer_t(queue_buffer_t* buffer)
    : buf{buffer}
    , pos{0}
    , meta{0}
  {
    assert(buf != nullptr);
  }

  queue_consumer_t(const queue_consumer_t&) = delete;
  queue_consumer_t& operator=(const queue_consumer_t&) = delete;

  inline uint32_t size() const
  {
    return meta & MASKSIZE;
  }

  inline void load()
  {
    meta = buf->meta;
  }

  inline const char* get() const
  {
    return buf->data[pos];
  }

  inline bool next()
  {
    if (meta & MASKNEXT) {
      auto nbuf = buf->next;
      delete buf;
      buf = nbuf;
      pos = 0;
    } else if (meta & MASKSTOP) {
      delete buf;
      buf = nullptr;
      pos = 0;
      meta = 0;
      return false;
    }
    return true;
  }
};

struct queue_producer_t
{
  queue_buffer_t* buf;
  uint32_t pos;

  explicit queue_producer_t(queue_buffer_t* buffer)
    : buf{buffer}
    , pos{0}
  {
    assert(buf != nullptr);
  }

  queue_producer_t(const queue_producer_t&) = delete;
  queue_producer_t& operator=(const queue_producer_t&) = delete;

  inline void push(const char* s)
  {
    if (pos < BUFSIZE) {
      buf->data[pos++] = s;
      buf->meta = pos;
    } else {
      auto nbuf = new queue_buffer_t;
      nbuf->data[0] = s;
      nbuf->meta = 1;
      buf->next = nbuf;
      buf->meta = BUFSIZE | MASKNEXT;
      buf = nbuf;
      pos = 1;
    }
  }

  inline void stop()
  {
    buf->meta |= MASKSTOP;
  }
};

const string pattern = "quid";

static void thread_match(int id, queue_buffer_t* ibuf, queue_buffer_t* obuf)
{
  printf("start #%d\n", id);
  queue_consumer_t c{ibuf};
  queue_producer_t p{obuf};
  size_t matches = 0;

  do {
    c.load();
    for (; c.pos < c.size(); ++c.pos) {
      if (match(pattern, c.get())) {
        p.push(c.get());
        ++matches;
      }
    }
  } while (c.next());

  p.stop();
  printf("exit #%d matches=%ld\n", id, matches);
}

struct thread_match_t
{
  queue_producer_t p;
  queue_consumer_t c;
  std::thread      t;

  static thread_match_t* create(int i)
  {
    auto ibuf = new queue_buffer_t;
    auto obuf = new queue_buffer_t;
    return new thread_match_t(i, ibuf, obuf);
  }

  explicit thread_match_t(int i, queue_buffer_t* ibuf, queue_buffer_t* obuf)
    : p{ibuf}
    , c{obuf}
    , t{std::thread{thread_match, i, ibuf, obuf}}
  {
  }

  inline void push(const char* s)
  {
    p.push(s);
  }

  inline void stop()
  {
    p.stop();
  }
};

struct results_buffer_t
{
  // std::mutex mut;
  size_t refs;
  results_buffer_t* next;
  size_t len;
  size_t cap;
  const char *res;

  explicit results_buffer_t()
    : refs{0}
    , next{nullptr}
    , len{0}
    , cap{0}
    , res{nullptr}
  {
  }
};

struct results_cosumer_t
{
  results_buffer_t* buf {nullptr};

  explicit results_cosumer_t()
    : buf{new results_buffer_t}
  {
  }

  void next()
  {
    results_buffer_t* it = buf;
    while (true) {
      if (it == nullptr)
        break;
      it = it->next;
    }
    buf = it;
  }
};

static void merge_thread(vector<thread_match_t*>* threads)
{
  vector<const char*> output;
  output.reserve(512);
  vector<thread_match_t*> results {*threads};
  size_t selected = 0;

  while (!results.empty()) {
    auto&& c = results[selected]->c;
    c.load();
    for (; c.pos < c.size(); ++c.pos)
      output.emplace_back(c.get());
    if (!c.next())
      results.erase(results.begin() + selected);
    if (++selected >= results.size())
      selected = 0;
  }

  printf("results = %ld\n", output.size());
}

int main()
{
  vector<const char*> input;
  input.reserve(40000 * example.size());
  for (size_t i = 0; i < 40000; ++i)
    for (const auto& choice : example)
      input.emplace_back(choice.c_str());
  printf("%ld items\n", input.size());

  size_t selected = 0;
  vector<thread_match_t*> threads {};
  for (size_t i = 0; i < WORKERS; ++i)
    threads.emplace_back(thread_match_t::create(i));
  std::thread merge{merge_thread, &threads};

  for (auto s : input) {
    auto w = threads[selected++];
    if (selected >= threads.size())
      selected = 0;
    w->push(s);
  }

  for (auto w : threads)
    w->stop();
  for (auto w : threads)
    w->t.join();
  for (auto w : threads)
    delete w;
  merge.join();
}

// vim: sts=2 sw=2 et
