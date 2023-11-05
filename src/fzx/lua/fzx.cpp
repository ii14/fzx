#include "fzx/config.hpp"
#include "fzx/fzx.hpp"
#include "fzx/helper/eventfd.hpp"
#include "fzx/helper/line_scanner.hpp"
#include "fzx/score.hpp"

#include <algorithm>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace fzx::lua {

struct Instance
{
  EventFd mEventFd;
  LineScanner mLineScanner;
  Fzx mFzx;
};

static constexpr const char* kMetatable = "fzx-instance";

static Instance*& getUserdata(lua_State* lstate)
{
  return *static_cast<Instance**>(luaL_checkudata(lstate, 1, kMetatable));
}

static int create(lua_State* lstate)
{
  unsigned threads = 1;
  if (!lua_isnil(lstate, 1)) {
    if (!lua_istable(lstate, 1))
      return luaL_error(lstate, "fzx: expected table");

    lua_getfield(lstate, 1, "threads");
    if (!lua_isnil(lstate, -1)) {
      if (lua_type(lstate, -1) != LUA_TNUMBER)
        return luaL_error(lstate, "fzx: 'threads' has to be a number");
      threads = std::clamp(lua_tointeger(lstate, -1), lua_Integer{1}, lua_Integer{kMaxThreads});
    }
    lua_pop(lstate, 1);
  }

  auto*& p = *static_cast<Instance**>(lua_newuserdata(lstate, sizeof(Instance*)));
  try {
    p = new Instance();
    p->mFzx.setThreads(threads);
    if (auto err = p->mEventFd.open(); !err.empty())
      return luaL_error(lstate, "fzx: %s", err.c_str());
    p->mFzx.setCallback([](void* userData) {
      static_cast<Instance*>(userData)->mEventFd.notify();
    }, p);
  } catch (const std::exception& e) {
    return luaL_error(lstate, "fzx: %s", e.what());
  }
  luaL_getmetatable(lstate, kMetatable);
  lua_setmetatable(lstate, -2);
  return 1;
}

static int gc(lua_State* lstate)
{
  auto*& p = getUserdata(lstate);
  delete p;
  p = nullptr;
  return 0;
}

static int toString(lua_State* lstate)
{
  auto* p = getUserdata(lstate);
  lua_pushfstring(lstate, "fzx: %p", p);
  return 1;
}

static int isNil(lua_State* lstate)
{
  auto* p = getUserdata(lstate);
  lua_pushboolean(lstate, p == nullptr);
  return 1;
}

static int getFd(lua_State* lstate)
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  lua_pushinteger(lstate, p->mEventFd.fd());
  return 1;
}

static int start(lua_State* lstate) try
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  p->mFzx.start();
  return 0;
} catch (const std::exception& e) {
  return luaL_error(lstate, "fzx: %s", e.what());
}

static int stop(lua_State* lstate) try
{
  auto*& p = getUserdata(lstate);
  if (p == nullptr)
    return 0;
  p->mFzx.stop();
  delete p;
  p = nullptr;
  return 0;
} catch (const std::exception& e) {
  return luaL_error(lstate, "fzx: %s", e.what());
}

static int push(lua_State* lstate) try
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  const int type = lua_type(lstate, 2);
  if (type == LUA_TSTRING) {
    size_t len = 0;
    const char* str = lua_tolstring(lstate, 2, &len);
    p->mFzx.pushItem({ str, len });
  } else if (type == LUA_TTABLE) {
    for (int i = 1;; ++i) {
      lua_rawgeti(lstate, 2, i);
      if (lua_isnil(lstate, -1))
        return 0;
      if (!lua_isstring(lstate, -1))
        return luaL_error(lstate, "fzx: invalid type passed to push function");
      size_t len = 0;
      const char* str = lua_tolstring(lstate, -1, &len);
      p->mFzx.pushItem({ str, len });
      lua_pop(lstate, 1);
    }
  } else {
    return luaL_error(lstate, "fzx: invalid type passed to push function");
  }
  return 0;
} catch (const std::exception& e) {
  return luaL_error(lstate, "fzx: %s", e.what());
}

static int scanFeed(lua_State* lstate) try
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  size_t len = 0;
  const char* str = luaL_checklstring(lstate, 2, &len);
  auto push = [&p](std::string_view item) { p->mFzx.pushItem(item); };
  if (p->mLineScanner.feed({ str, len }, push) > 0)
    p->mFzx.commit();
  return 0;
} catch (const std::exception& e) {
  return luaL_error(lstate, "fzx: %s", e.what());
}

static int scanEnd(lua_State* lstate) try
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  auto push = [&p](std::string_view item) { p->mFzx.pushItem(item); };
  if (p->mLineScanner.finalize(push))
    p->mFzx.commit();
  return 0;
} catch (const std::exception& e) {
  return luaL_error(lstate, "fzx: %s", e.what());
}

static int commit(lua_State* lstate) try
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  p->mFzx.commit();
  return 0;
} catch (const std::exception& e) {
  return luaL_error(lstate, "fzx: %s", e.what());
}

static int setQuery(lua_State* lstate) try
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  size_t len = 0;
  const char* str = luaL_checklstring(lstate, 2, &len);
    p->mFzx.setQuery({ str, len });
  return 0;
} catch (const std::exception& e) {
  return luaL_error(lstate, "fzx: %s", e.what());
}

static int loadResults(lua_State* lstate)
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  p->mEventFd.consume();
  lua_pushboolean(lstate, p->mFzx.loadResults());
  return 1;
}

static int getResults(lua_State* lstate) try
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  lua_settop(lstate, 3);

  lua_Integer max = 50;
  if (!lua_isnil(lstate, 2)) {
    max = luaL_checkinteger(lstate, 2);
    max = std::clamp(max, lua_Integer{0}, lua_Integer{std::numeric_limits<int>::max()});
  }

  lua_Integer offset = 0;
  if (!lua_isnil(lstate, 3)) {
    offset = luaL_checkinteger(lstate, 3);
    offset = std::max(offset, lua_Integer{0});
  }

  // TODO: use signed integers lol
  const auto size = p->mFzx.resultsSize();
  const auto maxoff = size > static_cast<size_t>(max) ? size - static_cast<size_t>(max) : 0;
  if (static_cast<size_t>(offset) > maxoff)
    offset = static_cast<lua_Integer>(maxoff);
  const auto end = std::min(static_cast<size_t>(offset) + static_cast<size_t>(max), size);

  const auto query = p->mFzx.query();

  lua_createtable(lstate, 0, 5);

  lua_pushinteger(lstate, static_cast<lua_Integer>(p->mFzx.itemsSize()));
  lua_setfield(lstate, -2, "total");

  lua_pushinteger(lstate, static_cast<lua_Integer>(p->mFzx.resultsSize()));
  lua_setfield(lstate, -2, "matched");

  lua_pushinteger(lstate, offset);
  lua_setfield(lstate, -2, "offset");

  lua_pushboolean(lstate, p->mFzx.processing());
  lua_setfield(lstate, -2, "processing");

  lua_pushnumber(lstate, p->mFzx.progress());
  lua_setfield(lstate, -2, "progress");

  lua_createtable(lstate, static_cast<int>(max), 0);
  const int tableSize = query.empty() ? 3 : 4;
  int n = 1;
  for (size_t i = offset; i < end; ++i, ++n) {
    auto item = p->mFzx.getResult(i);
    lua_createtable(lstate, 0, tableSize);

    lua_pushinteger(lstate, static_cast<lua_Integer>(item.mIndex));
    lua_setfield(lstate, -2, "index");

    lua_pushlstring(lstate, item.mLine.data(), item.mLine.size());
    lua_setfield(lstate, -2, "text");

    lua_createtable(lstate, static_cast<int>(query.size()), 0);
    if (!query.empty()) {
      Positions positions;
      constexpr auto kInvalid = std::numeric_limits<size_t>::max();
      std::fill(positions.begin(), positions.end(), kInvalid);
      matchPositions(query, item.mLine, &positions);

      for (int i = 0; static_cast<size_t>(i) < positions.size() && positions[i] != kInvalid; ++i) {
        lua_pushinteger(lstate, static_cast<lua_Integer>(positions[i]));
        lua_rawseti(lstate, -2, i + 1);
      }
    }
    lua_setfield(lstate, -2, "positions");

    lua_pushnumber(lstate, item.mScore);
    lua_setfield(lstate, -2, "score");

    lua_rawseti(lstate, -2, n);
  }
  lua_setfield(lstate, -2, "items");
  return 1;
} catch (const std::exception& e) {
  return luaL_error(lstate, "fzx: %s", e.what());
}

} // namespace fzx::lua

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int luaopen_fzxlua(lua_State* lstate)
{
  luaL_newmetatable(lstate, fzx::lua::kMetatable);
    lua_pushcfunction(lstate, fzx::lua::gc);
      lua_setfield(lstate, -2, "__gc");
    lua_pushcfunction(lstate, fzx::lua::toString);
      lua_setfield(lstate, -2, "__tostring");
    lua_createtable(lstate, 0, 10);
      lua_pushcfunction(lstate, fzx::lua::isNil);
        lua_setfield(lstate, -2, "is_nil");
      lua_pushcfunction(lstate, fzx::lua::getFd);
        lua_setfield(lstate, -2, "get_fd");
      lua_pushcfunction(lstate, fzx::lua::start);
        lua_setfield(lstate, -2, "start");
      lua_pushcfunction(lstate, fzx::lua::stop);
        lua_setfield(lstate, -2, "stop");
      lua_pushcfunction(lstate, fzx::lua::push);
        lua_setfield(lstate, -2, "push");
      lua_pushcfunction(lstate, fzx::lua::scanFeed);
        lua_setfield(lstate, -2, "scan_feed");
      lua_pushcfunction(lstate, fzx::lua::scanEnd);
        lua_setfield(lstate, -2, "scan_end");
      lua_pushcfunction(lstate, fzx::lua::commit);
        lua_setfield(lstate, -2, "commit");
      lua_pushcfunction(lstate, fzx::lua::setQuery);
        lua_setfield(lstate, -2, "set_query");
      lua_pushcfunction(lstate, fzx::lua::loadResults);
        lua_setfield(lstate, -2, "load_results");
      lua_pushcfunction(lstate, fzx::lua::getResults);
        lua_setfield(lstate, -2, "get_results");
      lua_setfield(lstate, -2, "__index");
    lua_pop(lstate, 1);

  lua_createtable(lstate, 0, 2);
    lua_pushcfunction(lstate, fzx::lua::create);
      lua_setfield(lstate, -2, "new");
    lua_pushinteger(lstate, lua_Integer{std::numeric_limits<int>::max()});
      lua_setfield(lstate, -2, "MAX_OFFSET");
  return 1;
}
