extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "fzx/fzx.hpp"
#include "fzx/match/fzy.hpp"

namespace fzx {

#define FZX_MT_INSTANCE "fzx-instance"

static int luaNew(lua_State* lstate)
{
  auto** udata = static_cast<Fzx**>(lua_newuserdata(lstate, sizeof(Fzx*)));
  ASSERT(udata != nullptr);
  try {
    *udata = new Fzx();
  } catch (const std::exception& e) {
    return luaL_error(lstate, "fzx: %s", e.what());
  }
  luaL_getmetatable(lstate, FZX_MT_INSTANCE);
  lua_setmetatable(lstate, -2);
  return 1;
}

static int luaGC(lua_State* lstate)
{
  auto** p = static_cast<Fzx**>(luaL_checkudata(lstate, 1, FZX_MT_INSTANCE));
  delete *p;
  *p = nullptr;
  return 0;
}

static int luaToString(lua_State* lstate)
{
  auto* p = *static_cast<Fzx**>(luaL_checkudata(lstate, 1, FZX_MT_INSTANCE));
  lua_pushfstring(lstate, "fzx: %p", p);
  return 1;
}

static int luaNotifyFd(lua_State* lstate)
{
  auto* p = *static_cast<Fzx**>(luaL_checkudata(lstate, 1, FZX_MT_INSTANCE));
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  lua_pushnumber(lstate, p->notifyFd());
  return 1;
}

static int luaStart(lua_State* lstate)
{
  auto* p = *static_cast<Fzx**>(luaL_checkudata(lstate, 1, FZX_MT_INSTANCE));
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  try {
    p->start();
  } catch (const std::exception& e) {
    return luaL_error(lstate, "fzx: %s", e.what());
  }
  return 0;
}

static int luaStop(lua_State* lstate)
{
  auto* p = *static_cast<Fzx**>(luaL_checkudata(lstate, 1, FZX_MT_INSTANCE));
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  p->stop();
  return 0;
}

static int luaCommit(lua_State* lstate)
{
  auto* p = *static_cast<Fzx**>(luaL_checkudata(lstate, 1, FZX_MT_INSTANCE));
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  p->commitItems();
  return 0;
}

static int luaLoad(lua_State* lstate)
{
  auto* p = *static_cast<Fzx**>(luaL_checkudata(lstate, 1, FZX_MT_INSTANCE));
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  lua_pushboolean(lstate, p->loadResults());
  return 1;
}

static int luaQuery(lua_State* lstate)
{
  auto* p = *static_cast<Fzx**>(luaL_checkudata(lstate, 1, FZX_MT_INSTANCE));
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

static int luaResults(lua_State* lstate)
{
  auto* p = *static_cast<Fzx**>(luaL_checkudata(lstate, 1, FZX_MT_INSTANCE));
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  // TODO: better argument parsing
  lua_Integer max = 50;
  if (lua_isnumber(lstate, 2))
    max = lua_tointeger(lstate, 2);
  size_t size = p->resultsSize();
  lua_createtable(lstate, 0, 3);
  lua_pushinteger(lstate, int(p->itemsSize()));
  lua_setfield(lstate, -2, "total");
  lua_pushinteger(lstate, int(p->resultsSize()));
  lua_setfield(lstate, -2, "matched");
  lua_createtable(lstate, int(size), 0);
  for (size_t i = 0; i < size && int(i) < max; ++i) {
    auto item = p->getResult(i);
    lua_createtable(lstate, 0, 3);
    lua_pushnumber(lstate, int(i));
    lua_setfield(lstate, -2, "index");
    lua_pushnumber(lstate, item.mScore);
    lua_setfield(lstate, -2, "score");
    lua_pushlstring(lstate, item.mLine.data(), item.mLine.size());
    lua_setfield(lstate, -2, "text");
    lua_rawseti(lstate, -2, int(i + 1));
  }
  lua_setfield(lstate, -2, "items");
  return 1;
}

static int luaPush(lua_State* lstate)
{
  auto* p = *static_cast<Fzx**>(luaL_checkudata(lstate, 1, FZX_MT_INSTANCE));
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

static int luaScanFeed(lua_State* lstate)
{
  auto* p = *static_cast<Fzx**>(luaL_checkudata(lstate, 1, FZX_MT_INSTANCE));
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  size_t len = 0;
  const char* str = luaL_checklstring(lstate, 2, &len);
  if (p->scanFeed({ str, len }) > 0)
    p->commitItems();
  return 0;
}

static int luaScanEnd(lua_State* lstate)
{
  auto* p = *static_cast<Fzx**>(luaL_checkudata(lstate, 1, FZX_MT_INSTANCE));
  if (p == nullptr)
    return luaL_error(lstate, "fzx: null pointer");
  if (p->scanEnd())
    p->commitItems();
  return 0;
}

static int luaMatchPositions(lua_State* lstate)
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
  auto score = matchPositions(needle, haystack, &positions);

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

} // namespace fzx

// TODO: pushItem, itemsSize, getItem

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int luaopen_fzx(lua_State* lstate)
{
  luaL_newmetatable(lstate, FZX_MT_INSTANCE);
    lua_pushcfunction(lstate, fzx::luaGC);
      lua_setfield(lstate, -2, "__gc");
    lua_createtable(lstate, 0, 10);
      lua_pushcfunction(lstate, fzx::luaNotifyFd);
        lua_setfield(lstate, -2, "fd");
      lua_pushcfunction(lstate, fzx::luaStart);
        lua_setfield(lstate, -2, "start");
      lua_pushcfunction(lstate, fzx::luaStop);
        lua_setfield(lstate, -2, "stop");
      lua_pushcfunction(lstate, fzx::luaCommit);
        lua_setfield(lstate, -2, "commit");
      lua_pushcfunction(lstate, fzx::luaLoad);
        lua_setfield(lstate, -2, "load");
      lua_pushcfunction(lstate, fzx::luaQuery);
        lua_setfield(lstate, -2, "query");
      lua_pushcfunction(lstate, fzx::luaResults);
        lua_setfield(lstate, -2, "results");
      lua_pushcfunction(lstate, fzx::luaPush);
        lua_setfield(lstate, -2, "push");
      lua_pushcfunction(lstate, fzx::luaScanFeed);
        lua_setfield(lstate, -2, "scan_feed");
      lua_pushcfunction(lstate, fzx::luaScanEnd);
        lua_setfield(lstate, -2, "scan_end");
      lua_setfield(lstate, -2, "__index");
    lua_pushcfunction(lstate, fzx::luaToString);
      lua_setfield(lstate, -2, "__tostring");
    lua_pop(lstate, 1);

  lua_createtable(lstate, 0, 2);
    lua_pushcfunction(lstate, fzx::luaNew);
      lua_setfield(lstate, -2, "new");
    lua_pushcfunction(lstate, fzx::luaMatchPositions);
      lua_setfield(lstate, -2, "match_positions");
  return 1;
}
