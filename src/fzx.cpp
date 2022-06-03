#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <cassert>

#include <uv.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
uv_loop_t* luv_loop(lua_State* L);
}

#include "queue.hpp"
#include "match.hpp"
#include "allocator.hpp"
#include "choice.hpp"
#include "results.hpp"

using std::string;
using std::vector;

using LuaRef = int;

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

  static thread_match_t* create() {
    return new thread_match_t(new_queue_buffer(), new_queue_buffer());
  }

  explicit thread_match_t(queue_buffer_t* ibuf, queue_buffer_t* obuf)
    : p{ibuf}, c{obuf}
    , t{std::thread{run, ibuf, obuf}}
  {
  }

  static void run(queue_buffer_t* ibuf, queue_buffer_t* obuf)
  {
    queue_consumer_t c{ibuf};
    queue_producer_t p{obuf};
    do {
      for (c.fetch(); c.pos < c.size(); ++c.pos) {
        if (has_match(PATTERN.c_str(), c.get().value())) {
          match(PATTERN.c_str(), c.get().value());
          p.push(c.get());
        }
      }
    } while (c.next());
    p.stop();
  }

  void push(choice_t s) { p.push(s); }
  void stop() { p.stop(); }
};

struct ctx_t
{
  lua_State *L {nullptr};
  LuaRef callback {LUA_REFNIL};

  uv_async_t async;
  std::atomic<bool> signalled {false};

  allocator_t mem;
  results_t results;
  vector<thread_match_t*> threads;
  std::thread t_merge;
  std::thread t_main;
  std::atomic<bool> done {false};

  void run();
  void merge();
  void update() {
    if (!signalled.exchange(true))
      uv_async_send(&async);
  }
};

void ctx_t::merge()
{
  vector<thread_match_t*> ts {threads};
  size_t selected = 0; // TODO: implement mpsc queue

  while (!ts.empty()) {
    auto& c = ts[selected]->c;
    for (c.fetch(); c.pos < c.size(); ++c.pos)
      results.insert(c.get());
    if (!c.next())
      ts.erase(ts.begin() + selected);
    else
      ++selected;
    if (selected >= ts.size())
      selected = 0;
  }
}

// for (size_t i = 0; i < 6; ++i) {
//   std::this_thread::sleep_for(std::chrono::milliseconds(15));
//   results_buffer_t res;
//   results.fetch(res, 0, 10);
//   printf("items = %ld\n", res.max);
//   // for (auto item : res.buf)
//   //   printf("#%u %s\n", item.index(), item.value());
// }


void ctx_t::run()
{
  for (size_t i = 0; i < WORKERS; ++i)
    threads.push_back(thread_match_t::create());
  t_merge = std::thread{&ctx_t::merge, this};

  size_t selected = 0;
  for (size_t i = 0; i < 40000; ++i) {
    for (const auto& choice : example) {
      auto s = mem.insert(choice.c_str());
      auto w = threads[selected++];
      // TODO: implement spmc queue
      if (selected >= threads.size())
        selected = 0;
      w->push(s);
      update();
    }
  }

  for (auto w : threads)
    w->stop();

  for (auto w : threads)
    w->t.join();
  t_merge.join();

  for (auto w : threads)
    delete w;

  done = true;
  update();
}



static int deferred_cb(lua_State* L)
{
  auto ctx = static_cast<ctx_t*>(lua_touserdata(L, lua_upvalueindex(1)));
  lua_getref(L, ctx->callback);
  lua_pushboolean(L, ctx->done);

  results_buffer_t buf;
  ctx->results.fetch(buf, 0, 10);

  lua_createtable(L, 0, 1);
  lua_pushnumber(L, buf.max);
  lua_setfield(L, -2, "total");

  lua_createtable(L, 0, 1);
  int idx = 1;
  for (auto choice : buf.buf) {
    lua_pushstring(L, choice.value());
    lua_rawseti(L, -2, idx++);
  }
  lua_setfield(L, -2, "items");

  ctx->signalled = false; // if it's after pcall ctx can be potentially destroyed
  if (lua_pcall(L, 2, 0, 0))
    lua_pop(L, 1);
  return 0;
}

static void async_cb(uv_async_t* async)
{
  auto ctx = static_cast<ctx_t*>(async->data);
  // TODO: check everything so there can be no error here. bad things can happen
  auto L = ctx->L;
  lua_getglobal(L, "vim");
  lua_getfield(L, -1, "schedule");
  lua_pushlightuserdata(L, ctx);
  lua_pushcclosure(L, deferred_cb, 1);
  lua_call(L, 1, 0); // TODO: pcall?
  lua_pop(L, 1);
}


static int start(lua_State* L)
{
  if (lua_type(L, 1) != LUA_TFUNCTION)
    luaL_error(L, "expected function");
  auto ctx = new ctx_t; // TODO: std::nothrow
  uv_async_init(luv_loop(L), &ctx->async, async_cb); // TODO: check error
  ctx->async.data = ctx;
  ctx->L = L;
  ctx->callback = luaL_ref(L, LUA_REGISTRYINDEX);
  ctx->t_main = std::thread{&ctx_t::run, ctx};
  lua_pushlightuserdata(L, ctx);
  return 1;
}

static int stop(lua_State* L)
{
  auto ctx = static_cast<ctx_t*>(lua_touserdata(L, 1));
  uv_close(reinterpret_cast<uv_handle_t*>(&ctx->async), nullptr);
  ctx->t_main.join();
  if (ctx->callback != LUA_REFNIL)
    luaL_unref(L, LUA_REGISTRYINDEX, ctx->callback);
  delete ctx;
  return 0;
}

extern "C" int luaopen_fzx(lua_State* L)
{
  lua_createtable(L, 0, 1);
  lua_pushcfunction(L, start);
  lua_setfield(L, -2, "start");
  lua_pushcfunction(L, stop);
  lua_setfield(L, -2, "stop");
  return 1;
}

// vim: sts=2 sw=2 et
