extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "fzx/fzx.hpp"

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
  lua_createtable(lstate, int(size), 0);
  for (size_t i = 0; i < size && int(i) < max; ++i) {
    auto item = p->getResult(i);
    lua_pushlstring(lstate, item.mLine.data(), item.mLine.size());
    lua_rawseti(lstate, -2, int(i + 1));
  }
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

} // namespace fzx

// TODO: pushItem, itemsSize, getItem

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int luaopen_fzx(lua_State* lstate)
{
  luaL_newmetatable(lstate, FZX_MT_INSTANCE);
    lua_pushcfunction(lstate, fzx::luaGC);
      lua_setfield(lstate, -2, "__gc");
    lua_createtable(lstate, 0, 8);
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
      lua_setfield(lstate, -2, "__index");
    lua_pushcfunction(lstate, fzx::luaToString);
      lua_setfield(lstate, -2, "__tostring");
    lua_pop(lstate, 1);

  lua_createtable(lstate, 0, 1);
    lua_pushcfunction(lstate, fzx::luaNew);
      lua_setfield(lstate, -2, "new");
  return 1;
}
