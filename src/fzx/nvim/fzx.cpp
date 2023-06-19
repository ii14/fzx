#include "fzx/fzx.hpp"
#include "fzx/match/fzy.hpp"

#include <algorithm>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace fzx::lua {

static constexpr const char* kMetatable = "fzx-instance";

static Fzx*& getUserdata(lua_State* lstate)
{
  return *static_cast<Fzx**>(luaL_checkudata(lstate, 1, kMetatable));
}

static int create(lua_State* lstate)
{
  auto*& p = *static_cast<Fzx**>(lua_newuserdata(lstate, sizeof(Fzx*)));
  try {
    p = new Fzx();
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

static int getFd(lua_State* lstate)
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  lua_pushinteger(lstate, p->notifyFd());
  return 1;
}

static int start(lua_State* lstate)
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  try {
    p->start();
  } catch (const std::exception& e) {
    return luaL_error(lstate, "fzx: %s", e.what());
  }
  return 0;
}

static int stop(lua_State* lstate)
{
  auto*& p = getUserdata(lstate);
  if (p == nullptr)
    return 0;
  p->stop();
  delete p;
  p = nullptr;
  return 0;
}

static int push(lua_State* lstate)
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  const int type = lua_type(lstate, 2);
  if (type == LUA_TSTRING) {
    size_t len = 0;
    const char* str = lua_tolstring(lstate, 2, &len);
    try {
      p->pushItem({ str, len });
    } catch (const std::exception& e) {
      return luaL_error(lstate, "fzx: %s", e.what());
    }
  } else if (type == LUA_TTABLE) {
    try {
      for (int i = 1;; ++i) {
        lua_rawgeti(lstate, 2, i);
        if (lua_isnil(lstate, -1))
          return 0;
        if (!lua_isstring(lstate, -1))
          return luaL_error(lstate, "fzx: invalid type passed to push function");
        size_t len = 0;
        const char* str = lua_tolstring(lstate, -1, &len);
        p->pushItem({ str, len });
        lua_pop(lstate, 1);
      }
    } catch (const std::exception& e) {
      return luaL_error(lstate, "fzx: %s", e.what());
    }
  } else {
    return luaL_error(lstate, "fzx: invalid type passed to push function");
  }
  return 0;
}

static int scanFeed(lua_State* lstate)
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  size_t len = 0;
  const char* str = luaL_checklstring(lstate, 2, &len);
  if (p->scanFeed({ str, len }) > 0)
    p->commitItems();
  return 0;
}

static int scanEnd(lua_State* lstate)
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  if (p->scanEnd())
    p->commitItems();
  return 0;
}

static int commit(lua_State* lstate)
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  p->commitItems();
  return 0;
}

static int setQuery(lua_State* lstate)
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  size_t len = 0;
  const char* str = luaL_checklstring(lstate, 2, &len);
  try {
    p->setQuery({ str, len });
  } catch (const std::exception& e) {
    return luaL_error(lstate, "fzx: %s", e.what());
  }
  return 0;
}

static int loadResults(lua_State* lstate)
{
  auto* p = getUserdata(lstate);
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  lua_pushboolean(lstate, p->loadResults());
  return 1;
}

static int getResults(lua_State* lstate)
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
  int n = 1;
  const auto size = p->resultsSize();
  const auto maxoff = size > size_t(max) ? size - size_t(max) : 0;
  if (size_t(offset) > maxoff)
    offset = lua_Integer(maxoff);
  const auto end = std::min(size_t(offset) + size_t(max), size);

  lua_createtable(lstate, 0, 4);
  lua_pushinteger(lstate, lua_Integer(p->itemsSize()));
  lua_setfield(lstate, -2, "total");
  lua_pushinteger(lstate, lua_Integer(p->resultsSize()));
  lua_setfield(lstate, -2, "matched");
  lua_pushinteger(lstate, offset);
  lua_setfield(lstate, -2, "offset");
  lua_createtable(lstate, int(max), 0);
  for (size_t i = offset; i < end; ++i, ++n) {
    auto item = p->getResult(i);
    lua_createtable(lstate, 0, 3);
    lua_pushinteger(lstate, lua_Integer(i));
    lua_setfield(lstate, -2, "index");
    lua_pushnumber(lstate, item.mScore);
    lua_setfield(lstate, -2, "score");
    lua_pushlstring(lstate, item.mLine.data(), item.mLine.size());
    lua_setfield(lstate, -2, "text");
    lua_rawseti(lstate, -2, n);
  }
  lua_setfield(lstate, -2, "items");
  return 1;
}

static int matchPositions(lua_State* lstate)
{
  size_t needleLen = 0;
  const char* needleStr = luaL_checklstring(lstate, 1, &needleLen);
  size_t haystackLen = 0;
  const char* haystackStr = luaL_checklstring(lstate, 2, &haystackLen);
  std::string_view needle { needleStr, needleLen };
  std::string_view haystack { haystackStr, haystackLen };
  std::array<size_t, kMatchMaxLen> positions {};
  constexpr auto kInvalid = std::numeric_limits<size_t>::max();
  std::fill(positions.begin(), positions.end(), kInvalid);
  auto score = fzx::matchPositions(needle, haystack, &positions);

  lua_createtable(lstate, 0, 2);
  lua_pushnumber(lstate, score);
  lua_setfield(lstate, -2, "score");
  lua_createtable(lstate, int(needleLen), 0);
  for (int i = 0; size_t(i) < positions.size(); ++i) {
    auto pos = positions[i];
    if (pos == kInvalid)
      break;
    lua_pushinteger(lstate, int(pos));
    lua_rawseti(lstate, -2, i + 1);
  }
  lua_setfield(lstate, -2, "positions");
  return 1;
}

} // namespace fzx::lua

// TODO: itemsSize, getItem

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int luaopen_fzx(lua_State* lstate)
{
  luaL_newmetatable(lstate, fzx::lua::kMetatable);
    lua_pushcfunction(lstate, fzx::lua::gc);
      lua_setfield(lstate, -2, "__gc");
    lua_pushcfunction(lstate, fzx::lua::toString);
      lua_setfield(lstate, -2, "__tostring");
    lua_createtable(lstate, 0, 10);
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
    lua_pushcfunction(lstate, fzx::lua::matchPositions);
      lua_setfield(lstate, -2, "match_positions");
    lua_pushinteger(lstate, lua_Integer{std::numeric_limits<int>::max()});
      lua_setfield(lstate, -2, "MAX_OFFSET");
  return 1;
}
