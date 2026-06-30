#include <stdio.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static int set_package_field(lua_State *L, const char *field,
                             const char *prepend) {
  lua_getglobal(L, "package");
  if (!lua_istable(L, -1)) {
    fprintf(stderr, "Lua package table is unavailable\n");
    return 1;
  }
  lua_getfield(L, -1, field);
  if (!lua_isstring(L, -1)) {
    fprintf(stderr, "Lua package.%s is unavailable\n", field);
    return 1;
  }
  lua_pushfstring(L, "%s;%s", prepend, lua_tostring(L, -1));
  lua_remove(L, -2);
  lua_setfield(L, -2, field);
  lua_pop(L, 1);
  return 0;
}

static int traceback_handler(lua_State *L) {
  const char *message = lua_tostring(L, 1);

  luaL_traceback(L, L, message != NULL ? message : "Lua error", 1);
  return 1;
}

static int run_script(lua_State *L, const char *path) {
  int traceback_index;

  lua_pushcfunction(L, traceback_handler);
  traceback_index = lua_gettop(L);
  if (luaL_loadfile(L, path) != LUA_OK) {
    fprintf(stderr, "%s: %s\n", path, lua_tostring(L, -1));
    lua_pop(L, 2);
    return 1;
  }
  if (lua_pcall(L, 0, LUA_MULTRET, traceback_index) != LUA_OK) {
    fprintf(stderr, "%s: %s\n", path, lua_tostring(L, -1));
    lua_pop(L, 2);
    return 1;
  }
  lua_remove(L, traceback_index);
  return 0;
}

int main(int argc, char **argv) {
  const char *source_dir;
  const char *binary_dir;
  lua_State *L;
  int i;
  int rc;

  if (argc < 4) {
    fprintf(stderr, "usage: %s <source-dir> <binary-dir> <script>...\n",
            argv[0]);
    return 1;
  }

  source_dir = argv[1];
  binary_dir = argv[2];
  L = luaL_newstate();
  if (L == NULL) {
    fprintf(stderr, "failed to create Lua state\n");
    return 1;
  }

  luaL_openlibs(L);
  lua_pushfstring(L, "%s/lua/?.lua;%s/lua/?/init.lua", source_dir,
                  source_dir);
  rc = set_package_field(L, "path", lua_tostring(L, -1));
  lua_pop(L, 1);
  if (rc == 0) {
    lua_pushfstring(L, "%s/lua-target/?.so", binary_dir);
    rc = set_package_field(L, "cpath", lua_tostring(L, -1));
    lua_pop(L, 1);
  }
  if (rc == 0) {
    for (i = 3; i < argc; ++i) {
      if (run_script(L, argv[i]) != 0) {
        rc = 1;
        break;
      }
    }
  }

  lua_close(L);
  return rc;
}
