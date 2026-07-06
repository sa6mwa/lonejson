#include "lonejson_lua.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int luaopen_lonejson_core(lua_State *L);

typedef struct chunk_reader {
  const char *const *chunks;
  size_t count;
  size_t index;
  size_t offset;
  size_t calls;
  size_t bytes;
} chunk_reader;

static int failures;

static void expect_true(int condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
  }
}

static lonejson_read_result chunk_reader_read(void *user, unsigned char *buffer,
                                              size_t capacity) {
  chunk_reader *reader = (chunk_reader *)user;
  lonejson_read_result result = lonejson_default_read_result();
  const char *chunk;
  size_t available;

  ++reader->calls;
  if (reader->index >= reader->count) {
    result.eof = 1;
    return result;
  }
  chunk = reader->chunks[reader->index];
  available = strlen(chunk) - reader->offset;
  if (available > capacity) {
    available = capacity;
  }
  if (available != 0u) {
    memcpy(buffer, chunk + reader->offset, available);
    reader->offset += available;
    reader->bytes += available;
  }
  if (chunk[reader->offset] == '\0') {
    ++reader->index;
    reader->offset = 0u;
  }
  result.bytes_read = available;
  result.eof = reader->index >= reader->count;
  return result;
}

static int run_lua(lua_State *L, const char *source) {
  if (luaL_loadstring(L, source) != LUA_OK) {
    fprintf(stderr, "Lua load failed: %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return 1;
  }
  if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
    fprintf(stderr, "Lua run failed: %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return 1;
  }
  return 0;
}

static void require_lonejson(lua_State *L, const char *source_dir) {
  luaL_openlibs(L);
  luaL_requiref(L, "lonejson.core", luaopen_lonejson_core, 1);
  lua_pop(L, 1);
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "path");
  lua_pushfstring(L, "%s/lua/?.lua;%s/lua/?/init.lua;%s", source_dir,
                  source_dir, lua_tostring(L, -1));
  lua_remove(L, -2);
  lua_setfield(L, -2, "path");
  lua_pop(L, 1);
}

static void push_global(lua_State *L, const char *name) {
  lua_getglobal(L, name);
  expect_true(!lua_isnil(L, -1), "expected Lua global");
}

static void test_interop(lua_State *L) {
  lonejson_schema_view schema;
  lonejson_schema_view other_schema;
  lonejson_record_view record;
  lonejson_record_view checked;
  lonejson_error error;
  lonejson_status status;
  int schema_index;
  int record_index;
  int table_index;
  int registry_ref;
  const char *chunks[] = {"{\"company\":\"Pkt", "\",\"email\":\"ops@",
                          "pkt.systems\"}"};
  chunk_reader reader;

  if (run_lua(
          L, "local lonejson = require('lonejson')\n"
             "local lj = lonejson.new({overflow = 'fail'})\n"
             "schema = lj:schema('Contact', {\n"
             "  lonejson.field('company', lonejson.string({max = 16})),\n"
             "  lonejson.field('email', lonejson.string({max = 32})),\n"
             "})\n"
             "other_schema = lj:schema('Other', {\n"
             "  lonejson.field('company', lonejson.string({max = 16})),\n"
             "})\n"
             "tiny_schema = lj:schema('Tiny', {\n"
             "  lonejson.field('company', lonejson.string({fixed_capacity = 3, "
             "overflow = 'fail'})),\n"
             "})\n") != 0) {
    ++failures;
    return;
  }

  schema.size = sizeof(schema);
  schema.abi_version = LONEJSON_VIEW_ABI_VERSION;
  push_global(L, "schema");
  schema_index = lua_absindex(L, -1);
  status = lonejson_lua_check_schema(L, schema_index, &schema, &error);
  expect_true(status == LONEJSON_STATUS_OK, "schema check succeeds");
  expect_true(schema.runtime != NULL, "schema view has runtime");
  expect_true(schema.map != NULL, "schema view has map");
  expect_true(schema.record_size != 0u, "schema view has record size");

  registry_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_gc(L, LUA_GCCOLLECT, 0);
  lua_rawgeti(L, LUA_REGISTRYINDEX, registry_ref);
  status = lonejson_lua_check_schema(L, -1, &schema, &error);
  expect_true(status == LONEJSON_STATUS_OK,
              "registry reference keeps schema usable");
  lua_pop(L, 1);

  record.size = sizeof(record);
  record.abi_version = LONEJSON_VIEW_ABI_VERSION;
  lua_rawgeti(L, LUA_REGISTRYINDEX, registry_ref);
  status = lonejson_lua_new_record(L, -1, &record_index, &record, &error);
  expect_true(status == LONEJSON_STATUS_OK, "new record succeeds");
  expect_true(record.record != NULL, "record view has storage");
  expect_true(record.schema.map == schema.map, "record view carries schema");

  memset(&reader, 0, sizeof(reader));
  reader.chunks = chunks;
  reader.count = sizeof(chunks) / sizeof(chunks[0]);
  status =
      schema.runtime->parse_reader(schema.runtime, schema.map, record.record,
                                   chunk_reader_read, &reader, &error);
  expect_true(status == LONEJSON_STATUS_OK, "manual fragmented parse succeeds");
  expect_true(reader.calls > 1u, "reader was called in fragments");
  table_index = lonejson_lua_record_to_table(L, record_index);
  expect_true(table_index != 0, "record converts to table");
  lua_getfield(L, table_index, "company");
  expect_true(strcmp(lua_tostring(L, -1), "Pkt") == 0, "table company matches");
  lua_pop(L, 1);
  lua_getfield(L, table_index, "email");
  expect_true(strcmp(lua_tostring(L, -1), "ops@pkt.systems") == 0,
              "table email matches");
  lua_pop(L, 1);
  lua_pop(L, 1);

  checked.size = sizeof(checked);
  checked.abi_version = LONEJSON_VIEW_ABI_VERSION;
  status =
      lonejson_lua_check_record(L, record_index, &schema, &checked, &error);
  expect_true(status == LONEJSON_STATUS_OK, "record check succeeds");

  push_global(L, "other_schema");
  other_schema.size = sizeof(other_schema);
  other_schema.abi_version = LONEJSON_VIEW_ABI_VERSION;
  status = lonejson_lua_check_schema(L, -1, &other_schema, &error);
  expect_true(status == LONEJSON_STATUS_OK, "other schema check succeeds");
  status = lonejson_lua_check_record(L, record_index, &other_schema, &checked,
                                     &error);
  expect_true(status == LONEJSON_STATUS_INVALID_ARGUMENT,
              "mismatched record/schema pair fails");
  lua_pop(L, 1);

  status = lonejson_lua_clear_record(L, record_index, &error);
  expect_true(status == LONEJSON_STATUS_OK, "record clear succeeds");

  schema.size = 1u;
  schema.abi_version = LONEJSON_VIEW_ABI_VERSION;
  push_global(L, "schema");
  status = lonejson_lua_check_schema(L, -1, &schema, &error);
  expect_true(status == LONEJSON_STATUS_INVALID_ARGUMENT,
              "bad schema view size fails");
  lua_pop(L, 1);
  schema.size = sizeof(schema);
  schema.abi_version = 999u;
  push_global(L, "schema");
  status = lonejson_lua_check_schema(L, -1, &schema, &error);
  expect_true(status == LONEJSON_STATUS_INVALID_ARGUMENT,
              "bad schema view ABI fails");
  lua_pop(L, 1);

  lua_pushstring(L, "not a schema");
  schema.size = sizeof(schema);
  schema.abi_version = LONEJSON_VIEW_ABI_VERSION;
  status = lonejson_lua_check_schema(L, -1, &schema, &error);
  expect_true(status == LONEJSON_STATUS_INVALID_ARGUMENT,
              "wrong userdata type returns status");
  lua_pop(L, 1);

  {
    const char *tiny_chunks[] = {"{\"company\":\"toolong\"}"};
    chunk_reader tiny_reader;
    int tiny_record_index = 0;
    memset(&tiny_reader, 0, sizeof(tiny_reader));
    tiny_reader.chunks = tiny_chunks;
    tiny_reader.count = 1u;
    push_global(L, "tiny_schema");
    status = lonejson_lua_parse_reader_to_record(
        L, -1, chunk_reader_read, &tiny_reader, &tiny_record_index, &error);
    expect_true(status == LONEJSON_STATUS_OVERFLOW,
                "overflow policy fails through interop helper");
    lua_pop(L, 1);
  }

  {
    const char *table_chunks[] = {"{\"company\":\"ACME\",\"email\":\"a@b.c\"}"};
    chunk_reader table_reader;
    memset(&table_reader, 0, sizeof(table_reader));
    table_reader.chunks = table_chunks;
    table_reader.count = 1u;
    push_global(L, "schema");
    status = lonejson_lua_parse_reader_to_table(
        L, -1, chunk_reader_read, &table_reader, &table_index, &error);
    expect_true(status == LONEJSON_STATUS_OK, "parse reader to table succeeds");
    lua_getfield(L, table_index, "company");
    expect_true(strcmp(lua_tostring(L, -1), "ACME") == 0,
                "high-level table helper company matches");
    lua_pop(L, 2);
  }

  lua_pop(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, registry_ref);
}

int main(int argc, char **argv) {
  lua_State *L;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <source-dir>\n", argv[0]);
    return 1;
  }
  L = luaL_newstate();
  if (L == NULL) {
    fprintf(stderr, "failed to create Lua state\n");
    return 1;
  }
  require_lonejson(L, argv[1]);
  test_interop(L);
  lua_close(L);
  if (failures != 0) {
    fprintf(stderr, "%d Lua interop test failure(s)\n", failures);
    return 1;
  }
  printf("lua interop tests passed\n");
  return 0;
}
