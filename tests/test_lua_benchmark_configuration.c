/* Compiled with the benchmark module's flags to enforce production paths. */
#if defined(LONEJSON_TEST_FORCE_LUA_LEGACY_USERVALUE) || \
    defined(LONEJSON_TEST_MAP_ANALYSIS_COUNTER) || \
    defined(LONEJSON_TEST_LUA_ENCODE_STATS) || \
    defined(LONEJSON_TEST_LUA_SIMULATE_NO_MAXINTEGER)
#error "Lua benchmarks must not use test-only module definitions"
#endif
typedef int lonejson_lua_benchmark_configuration_is_production;
