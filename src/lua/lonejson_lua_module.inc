static const luaL_Reg ljlua_schema_methods[] = {
    {"new_record", ljlua_schema_new_record},
    {"compile_path", ljlua_schema_compile_path},
    {"compile_get", ljlua_schema_compile_get},
    {"compile_count", ljlua_schema_compile_count},
    {"decode", ljlua_schema_decode},
    {"decode_into", ljlua_schema_decode_into},
    {"decode_path", ljlua_schema_decode_path},
    {"decode_file", ljlua_schema_decode_file},
    {"decode_fd", ljlua_schema_decode_fd},
    {"assign", ljlua_schema_assign},
    {"encode", ljlua_schema_encode},
    {"write_path", ljlua_schema_write_path},
    {"write_file", ljlua_schema_write_file},
    {"write_fd", ljlua_schema_write_fd},
    {"stream_path", ljlua_schema_stream_path},
    {"stream_fd", ljlua_schema_stream_fd},
    {"stream_file", ljlua_schema_stream_file},
    {"stream_string", ljlua_schema_stream_string},
    {"array_stream_path", ljlua_schema_array_stream_path},
    {"array_stream_fd", ljlua_schema_array_stream_fd},
    {"array_stream_file", ljlua_schema_array_stream_file},
    {"array_stream_string", ljlua_schema_array_stream_string},
    {NULL, NULL}};

static const luaL_Reg ljlua_record_methods[] = {
    {"get", ljlua_record_get},
    {"to_table", ljlua_record_to_table_method},
    {"count", ljlua_record_count},
    {"clear", ljlua_record_clear},
    {NULL, NULL}};

static const luaL_Reg ljlua_stream_methods[] = {
    {"next", ljlua_stream_next}, {"close", ljlua_stream_close}, {NULL, NULL}};

static const luaL_Reg ljlua_array_stream_methods[] = {
    {"next", ljlua_array_stream_next},
    {"next_value", ljlua_array_stream_next_value},
    {"close", ljlua_array_stream_close},
    {NULL, NULL}};

static const luaL_Reg ljlua_spool_methods[] = {
    {"size", ljlua_spool_size},         {"spilled", ljlua_spool_spilled},
    {"path", ljlua_spool_path},         {"rewind", ljlua_spool_rewind},
    {"read", ljlua_spool_read},         {"read_all", ljlua_spool_read_all},
    {"write_to", ljlua_spool_write_to}, {NULL, NULL}};

static const luaL_Reg ljlua_path_methods[] = {
    {"get", ljlua_path_get}, {"count", ljlua_path_count}, {NULL, NULL}};

static void ljlua_register_metatable(lua_State *L, const char *name,
                                     const luaL_Reg *methods,
                                     lua_CFunction gc_fn,
                                     lua_CFunction index_fn) {
  luaL_newmetatable(L, name);
  if (gc_fn != NULL) {
    lua_pushcfunction(L, gc_fn);
    lua_setfield(L, -2, "__gc");
  }
  if (index_fn != NULL) {
    lua_pushcfunction(L, index_fn);
    lua_setfield(L, -2, "__index");
  } else {
    lua_newtable(L);
    if (methods != NULL) {
      luaL_setfuncs(L, methods, 0);
    }
    lua_setfield(L, -2, "__index");
  }
  if (methods != NULL && index_fn != NULL) {
    luaL_setfuncs(L, methods, 0);
  }
  lua_pop(L, 1);
}

int luaopen_lonejson_core(lua_State *L) {
  static const luaL_Reg funcs[] = {{"compile_schema", ljlua_schema_new},
                                   {"json_null", ljlua_json_null},
                                   {"monotonic_ns", ljlua_monotonic_ns},
                                   {NULL, NULL}};

  luaL_checkversion(L);
  ljlua_register_metatable(L, LJLUA_SCHEMA_MT, ljlua_schema_methods,
                           ljlua_schema_gc, NULL);
  ljlua_register_metatable(L, LJLUA_RECORD_MT, ljlua_record_methods,
                           ljlua_record_gc, ljlua_record_index);
  ljlua_register_metatable(L, LJLUA_STREAM_MT, ljlua_stream_methods,
                           ljlua_stream_gc, NULL);
  ljlua_register_metatable(L, LJLUA_ARRAY_STREAM_MT,
                           ljlua_array_stream_methods, ljlua_array_stream_gc,
                           NULL);
  ljlua_register_metatable(L, LJLUA_SPOOL_MT, ljlua_spool_methods,
                           ljlua_spool_gc, NULL);
  ljlua_register_metatable(L, LJLUA_PATH_MT, ljlua_path_methods, ljlua_path_gc,
                           NULL);
  luaL_getmetatable(L, LJLUA_PATH_MT);
  lua_pushcfunction(L, ljlua_path_get);
  lua_setfield(L, -2, "__call");
  lua_pop(L, 1);
  luaL_newlib(L, funcs);
  return 1;
}

static int ljlua_monotonic_ns(lua_State *L) {
  struct timespec ts;
  lua_Integer value;

  (void)L;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return luaL_error(L, "clock_gettime failed: %s", strerror(errno));
  }
  value = (lua_Integer)ts.tv_sec * (lua_Integer)1000000000 +
          (lua_Integer)ts.tv_nsec;
  lua_pushinteger(L, value);
  return 1;
}

static int ljlua_json_null(lua_State *L) {
  ljlua_push_json_null(L);
  return 1;
}
