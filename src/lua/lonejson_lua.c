#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LONEJSON_IMPLEMENTATION
#include "lonejson.h"

#include <lauxlib.h>
#include <lua.h>

#define LJLUA_SCHEMA_MT "lonejson.schema"
#define LJLUA_RECORD_MT "lonejson.record"
#define LJLUA_STREAM_MT "lonejson.stream"
#define LJLUA_SPOOL_MT "lonejson.spool"
#define LJLUA_PATH_MT "lonejson.path"

typedef enum ljlua_field_kind {
  LJLUA_FIELD_STRING = 1,
  LJLUA_FIELD_SPOOLED_TEXT,
  LJLUA_FIELD_SPOOLED_BYTES,
  LJLUA_FIELD_I64,
  LJLUA_FIELD_U64,
  LJLUA_FIELD_F64,
  LJLUA_FIELD_BOOL,
  LJLUA_FIELD_OBJECT,
  LJLUA_FIELD_STRING_ARRAY,
  LJLUA_FIELD_I64_ARRAY,
  LJLUA_FIELD_U64_ARRAY,
  LJLUA_FIELD_F64_ARRAY,
  LJLUA_FIELD_BOOL_ARRAY,
  LJLUA_FIELD_OBJECT_ARRAY
} ljlua_field_kind;

typedef struct ljlua_schema ljlua_schema;
typedef struct ljlua_compiled_path ljlua_compiled_path;

typedef struct ljlua_field_meta {
  char *name;
  ljlua_field_kind lua_kind;
  lonejson_field field;
  size_t member_size;
  size_t fixed_array_capacity;
  size_t fixed_array_offset;
  ljlua_schema *subschema;
} ljlua_field_meta;

typedef struct ljlua_path_step {
  ljlua_field_meta *meta;
  size_t index;
} ljlua_path_step;

struct ljlua_compiled_path {
  char *path;
  size_t step_count;
  ljlua_path_step *steps;
  ljlua_compiled_path *next;
};

struct ljlua_schema {
  lonejson_map map;
  char *name;
  size_t record_size;
  size_t field_count;
  lonejson_field *fields;
  ljlua_field_meta *metas;
  ljlua_compiled_path *path_cache;
  unsigned char *scratch_record;
  char *encode_buffer;
  size_t encode_capacity;
  int scratch_in_use;
};

typedef struct ljlua_schema_ud {
  ljlua_schema *schema;
} ljlua_schema_ud;

typedef struct ljlua_record_ud {
  ljlua_schema *schema;
  int schema_ref;
  unsigned char data[1];
} ljlua_record_ud;

typedef struct ljlua_stream_ud {
  ljlua_schema *schema;
  int schema_ref;
  lonejson_stream *stream;
  unsigned char *owned_input;
  size_t owned_input_len;
  size_t owned_input_offset;
} ljlua_stream_ud;

typedef struct ljlua_spool_ud {
  lonejson_spooled *spool;
  lonejson_spooled owned;
  int owns_data;
  int owner_ref;
  int kind;
} ljlua_spool_ud;

typedef struct ljlua_path_ud {
  ljlua_schema *schema;
  ljlua_compiled_path *compiled;
  int schema_ref;
} ljlua_path_ud;

static int ljlua_record_to_table_method(lua_State *L);
static int ljlua_record_get(lua_State *L);
static int ljlua_record_count(lua_State *L);
static int ljlua_record_clear(lua_State *L);
static int ljlua_path_get(lua_State *L);
static int ljlua_path_count(lua_State *L);
static int ljlua_getter_call(lua_State *L);
static int ljlua_counter_call(lua_State *L);

static int ljlua_push_error(lua_State *L, const lonejson_error *error) {
  lua_newtable(L);
  lua_pushinteger(L, (lua_Integer)(error ? error->code : 0));
  lua_setfield(L, -2, "code");
  lua_pushstring(L, error ? lonejson_status_string(error->code) : "unknown");
  lua_setfield(L, -2, "status");
  lua_pushinteger(L, (lua_Integer)(error ? error->line : 0));
  lua_setfield(L, -2, "line");
  lua_pushinteger(L, (lua_Integer)(error ? error->column : 0));
  lua_setfield(L, -2, "column");
  lua_pushinteger(L, (lua_Integer)(error ? error->offset : 0));
  lua_setfield(L, -2, "offset");
  lua_pushinteger(L, (lua_Integer)(error ? error->system_errno : 0));
  lua_setfield(L, -2, "system_errno");
  lua_pushboolean(L, error ? error->truncated : 0);
  lua_setfield(L, -2, "truncated");
  lua_pushstring(L, error ? error->message : "unknown error");
  lua_setfield(L, -2, "message");
  return 1;
}

static void *ljlua_xmalloc(size_t size) {
  void *ptr;

  ptr = malloc(size);
  if (ptr == NULL) {
    abort();
  }
  return ptr;
}

static char *ljlua_strdup(const char *src) {
  size_t len;
  char *dst;

  len = strlen(src);
  dst = (char *)ljlua_xmalloc(len + 1u);
  memcpy(dst, src, len + 1u);
  return dst;
}

static void ljlua_u64_to_decimal(char *buf, size_t capacity,
                                 lonejson_uint64 value) {
  size_t idx;

  idx = capacity;
  buf[--idx] = '\0';
  do {
    lonejson_uint64 digit;
    digit = value % 10u;
    value /= 10u;
    buf[--idx] = (char)('0' + (int)digit);
  } while (value != 0u);
  memmove(buf, buf + idx, capacity - idx);
}

static int ljlua_push_u64(lua_State *L, lonejson_uint64 value) {
#if defined(LUA_MAXINTEGER)
  if (value <= (lonejson_uint64)LUA_MAXINTEGER) {
    lua_pushinteger(L, (lua_Integer)value);
    return 1;
  }
#endif
  {
    char buf[32];
    ljlua_u64_to_decimal(buf, sizeof(buf), value);
    lua_pushstring(L, buf);
  }
  return 1;
}

static lonejson_uint64 ljlua_check_u64(lua_State *L, int index) {
  int type;
  lonejson_uint64 value;

  type = lua_type(L, index);
  if (type == LUA_TNUMBER) {
    lua_Integer signed_value;
#if defined(LUA_MAXINTEGER)
    if (!lua_isinteger(L, index)) {
      luaL_error(L, "expected unsigned 64-bit integer");
    }
#endif
    signed_value = luaL_checkinteger(L, index);
    if (signed_value < 0) {
      luaL_error(L, "expected unsigned 64-bit integer");
    }
    return (lonejson_uint64)signed_value;
  }
  if (type == LUA_TSTRING) {
    const char *text = lua_tostring(L, index);
    if (!lonejson__parse_u64_token(text, &value)) {
      luaL_error(L, "expected unsigned 64-bit integer");
    }
    return value;
  }
  luaL_error(L, "expected unsigned 64-bit integer");
  return 0u;
}

static size_t ljlua_align(size_t value, size_t align) {
  return (value + align - 1u) & ~(align - 1u);
}

static lonejson_overflow_policy
ljlua_parse_overflow_policy(lua_State *L, int index, const char *field_name) {
  const char *name;

  lua_getfield(L, index, field_name);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return LONEJSON_OVERFLOW_FAIL;
  }
  name = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  if (strcmp(name, "fail") == 0) {
    return LONEJSON_OVERFLOW_FAIL;
  }
  if (strcmp(name, "truncate") == 0) {
    return LONEJSON_OVERFLOW_TRUNCATE;
  }
  if (strcmp(name, "truncate_silent") == 0) {
    return LONEJSON_OVERFLOW_TRUNCATE_SILENT;
  }
  luaL_error(L, "unsupported overflow policy '%s'", name);
  return LONEJSON_OVERFLOW_FAIL;
}

static int ljlua_field_kind_from_name(const char *name) {
  if (strcmp(name, "string") == 0) {
    return LJLUA_FIELD_STRING;
  }
  if (strcmp(name, "spooled_text") == 0) {
    return LJLUA_FIELD_SPOOLED_TEXT;
  }
  if (strcmp(name, "spooled_bytes") == 0) {
    return LJLUA_FIELD_SPOOLED_BYTES;
  }
  if (strcmp(name, "i64") == 0) {
    return LJLUA_FIELD_I64;
  }
  if (strcmp(name, "u64") == 0) {
    return LJLUA_FIELD_U64;
  }
  if (strcmp(name, "f64") == 0) {
    return LJLUA_FIELD_F64;
  }
  if (strcmp(name, "boolean") == 0) {
    return LJLUA_FIELD_BOOL;
  }
  if (strcmp(name, "object") == 0) {
    return LJLUA_FIELD_OBJECT;
  }
  if (strcmp(name, "string_array") == 0) {
    return LJLUA_FIELD_STRING_ARRAY;
  }
  if (strcmp(name, "i64_array") == 0) {
    return LJLUA_FIELD_I64_ARRAY;
  }
  if (strcmp(name, "u64_array") == 0) {
    return LJLUA_FIELD_U64_ARRAY;
  }
  if (strcmp(name, "f64_array") == 0) {
    return LJLUA_FIELD_F64_ARRAY;
  }
  if (strcmp(name, "boolean_array") == 0) {
    return LJLUA_FIELD_BOOL_ARRAY;
  }
  if (strcmp(name, "object_array") == 0) {
    return LJLUA_FIELD_OBJECT_ARRAY;
  }
  return 0;
}

static void ljlua_schema_destroy(ljlua_schema *schema);

static size_t ljlua_member_size_for_kind(const ljlua_field_meta *meta) {
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING:
    return (meta->field.storage == LONEJSON_STORAGE_FIXED)
               ? meta->field.fixed_capacity
               : sizeof(char *);
  case LJLUA_FIELD_SPOOLED_TEXT:
  case LJLUA_FIELD_SPOOLED_BYTES:
    return sizeof(lonejson_spooled);
  case LJLUA_FIELD_I64:
    return sizeof(lonejson_int64);
  case LJLUA_FIELD_U64:
    return sizeof(lonejson_uint64);
  case LJLUA_FIELD_F64:
    return sizeof(double);
  case LJLUA_FIELD_BOOL:
    return sizeof(bool);
  case LJLUA_FIELD_OBJECT:
    return meta->subschema ? meta->subschema->record_size : 0u;
  case LJLUA_FIELD_STRING_ARRAY:
    return sizeof(lonejson_string_array);
  case LJLUA_FIELD_I64_ARRAY:
    return sizeof(lonejson_i64_array);
  case LJLUA_FIELD_U64_ARRAY:
    return sizeof(lonejson_u64_array);
  case LJLUA_FIELD_F64_ARRAY:
    return sizeof(lonejson_f64_array);
  case LJLUA_FIELD_BOOL_ARRAY:
    return sizeof(lonejson_bool_array);
  case LJLUA_FIELD_OBJECT_ARRAY:
    return sizeof(lonejson_object_array);
  default:
    return 0u;
  }
}

static size_t ljlua_fixed_array_elem_size(const ljlua_field_meta *meta) {
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING_ARRAY:
    return sizeof(char *);
  case LJLUA_FIELD_I64_ARRAY:
    return sizeof(lonejson_int64);
  case LJLUA_FIELD_U64_ARRAY:
    return sizeof(lonejson_uint64);
  case LJLUA_FIELD_F64_ARRAY:
    return sizeof(double);
  case LJLUA_FIELD_BOOL_ARRAY:
    return sizeof(bool);
  case LJLUA_FIELD_OBJECT_ARRAY:
    return meta->subschema ? meta->subschema->record_size : 0u;
  default:
    return 0u;
  }
}

static int ljlua_compile_field(lua_State *L, int index, ljlua_field_meta *meta);
static int ljlua_finalize_schema(ljlua_schema *schema);
static FILE *ljlua_check_file(lua_State *L, int index);
static ljlua_path_ud *ljlua_check_path(lua_State *L, int index);
static ljlua_compiled_path *ljlua_find_compiled_path(ljlua_schema *schema,
                                                     const char *path);
static ljlua_compiled_path *
ljlua_compile_path(lua_State *L, ljlua_schema *schema, const char *path);
static int ljlua_push_compiled_path_value(lua_State *L, void *record,
                                          ljlua_compiled_path *compiled,
                                          int owner_index);
static lonejson_read_result
ljlua_mem_stream_read(void *user, unsigned char *buffer, size_t capacity);
static int ljlua_monotonic_ns(lua_State *L);

static FILE *ljlua_dup_fd_to_file(lua_State *L, int fd, const char *mode) {
  int dup_fd;
  FILE *fp;

  dup_fd = dup(fd);
  if (dup_fd < 0) {
    luaL_error(L, "failed to duplicate fd %d: %s", fd, strerror(errno));
  }
  fp = fdopen(dup_fd, mode);
  if (fp == NULL) {
    close(dup_fd);
    luaL_error(L, "failed to fdopen duplicated fd %d: %s", fd, strerror(errno));
  }
  return fp;
}

static int ljlua_check_fd_like(lua_State *L, int index) {
  if (lua_isinteger(L, index)) {
    return (int)lua_tointeger(L, index);
  }
  return fileno(ljlua_check_file(L, index));
}

static int ljlua_compile_nested_schema(lua_State *L, int index,
                                       ljlua_schema **out_schema) {
  ljlua_schema *schema;
  size_t count;
  size_t i;

  luaL_checktype(L, index, LUA_TTABLE);
  count = (size_t)lua_rawlen(L, index);
  schema = (ljlua_schema *)calloc(1u, sizeof(*schema));
  if (schema == NULL) {
    return luaL_error(L, "failed to allocate schema");
  }
  schema->name = ljlua_strdup("nested");
  schema->map.name = schema->name;
  schema->field_count = count;
  schema->metas = (ljlua_field_meta *)calloc(count, sizeof(schema->metas[0]));
  if (schema->metas == NULL) {
    ljlua_schema_destroy(schema);
    return luaL_error(L, "failed to allocate schema fields");
  }
  schema->fields = (lonejson_field *)calloc(count, sizeof(schema->fields[0]));
  if (schema->fields == NULL) {
    ljlua_schema_destroy(schema);
    return luaL_error(L, "failed to allocate runtime map fields");
  }
  for (i = 0u; i < count; ++i) {
    lua_rawgeti(L, index, (lua_Integer)i + 1);
    if (ljlua_compile_field(L, lua_gettop(L), &schema->metas[i]) != 0) {
      lua_pop(L, 1);
      ljlua_schema_destroy(schema);
      return lua_error(L);
    }
    lua_pop(L, 1);
  }
  ljlua_finalize_schema(schema);
  *out_schema = schema;
  return 0;
}

static int ljlua_compile_field(lua_State *L, int index,
                               ljlua_field_meta *meta) {
  const char *kind_name;
  size_t i;

  memset(meta, 0, sizeof(*meta));
  luaL_checktype(L, index, LUA_TTABLE);
  lua_getfield(L, index, "name");
  meta->name = ljlua_strdup(luaL_checkstring(L, -1));
  lua_pop(L, 1);
  lua_getfield(L, index, "kind");
  kind_name = luaL_checkstring(L, -1);
  meta->lua_kind = (ljlua_field_kind)ljlua_field_kind_from_name(kind_name);
  lua_pop(L, 1);
  if (meta->lua_kind == 0) {
    return luaL_error(L, "unsupported field kind '%s'", kind_name);
  }

  meta->field.json_key = meta->name;
  meta->field.json_key_len = strlen(meta->name);
  meta->field.json_key_first =
      meta->field.json_key_len != 0u ? (unsigned char)meta->name[0] : '\0';
  meta->field.json_key_last =
      meta->field.json_key_len != 0u
          ? (unsigned char)meta->name[meta->field.json_key_len - 1u]
          : '\0';

  lua_getfield(L, index, "required");
  if (lua_toboolean(L, -1)) {
    meta->field.flags |= LONEJSON_FIELD_REQUIRED;
  }
  lua_pop(L, 1);

  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING:
    lua_getfield(L, index, "fixed_capacity");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      meta->field.storage = LONEJSON_STORAGE_FIXED;
      meta->field.fixed_capacity = (size_t)lua_tointeger(L, -1);
      meta->field.overflow_policy =
          ljlua_parse_overflow_policy(L, index, "overflow");
    } else {
      meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
      meta->field.overflow_policy = LONEJSON_OVERFLOW_FAIL;
    }
    lua_pop(L, 1);
    meta->field.kind = LONEJSON_FIELD_KIND_STRING;
    break;
  case LJLUA_FIELD_SPOOLED_TEXT:
  case LJLUA_FIELD_SPOOLED_BYTES: {
    lonejson_spool_options *opts;

    meta->field.kind = (meta->lua_kind == LJLUA_FIELD_SPOOLED_TEXT)
                           ? LONEJSON_FIELD_KIND_STRING_STREAM
                           : LONEJSON_FIELD_KIND_BASE64_STREAM;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    opts = (lonejson_spool_options *)calloc(1u, sizeof(*opts));
    if (opts == NULL) {
      return luaL_error(L, "failed to allocate spool options");
    }
    *opts = lonejson_default_spool_options();
    lua_getfield(L, index, "memory_limit");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      opts->memory_limit = (size_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "max_bytes");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      opts->max_bytes = (size_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "temp_dir");
    if (!lua_isnil(L, -1)) {
      opts->temp_dir = ljlua_strdup(luaL_checkstring(L, -1));
    }
    lua_pop(L, 1);
    meta->field.spool_options = opts;
    break;
  }
  case LJLUA_FIELD_I64:
    meta->field.kind = LONEJSON_FIELD_KIND_I64;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    break;
  case LJLUA_FIELD_U64:
    meta->field.kind = LONEJSON_FIELD_KIND_U64;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    break;
  case LJLUA_FIELD_F64:
    meta->field.kind = LONEJSON_FIELD_KIND_F64;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    break;
  case LJLUA_FIELD_BOOL:
    meta->field.kind = LONEJSON_FIELD_KIND_BOOL;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    break;
  case LJLUA_FIELD_OBJECT:
  case LJLUA_FIELD_OBJECT_ARRAY:
    lua_getfield(L, index, "fields");
    if (ljlua_compile_nested_schema(L, lua_gettop(L), &meta->subschema) != 0) {
      lua_pop(L, 1);
      return 1;
    }
    lua_pop(L, 1);
    meta->field.kind = (meta->lua_kind == LJLUA_FIELD_OBJECT)
                           ? LONEJSON_FIELD_KIND_OBJECT
                           : LONEJSON_FIELD_KIND_OBJECT_ARRAY;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    meta->field.submap = &meta->subschema->map;
    if (meta->lua_kind == LJLUA_FIELD_OBJECT_ARRAY) {
      meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
      meta->field.elem_size = meta->subschema->record_size;
      meta->field.overflow_policy =
          ljlua_parse_overflow_policy(L, index, "overflow");
      lua_getfield(L, index, "fixed_capacity");
      if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
        meta->fixed_array_capacity = (size_t)lua_tointeger(L, -1);
      }
      lua_pop(L, 1);
    }
    break;
  case LJLUA_FIELD_STRING_ARRAY:
    meta->field.kind = LONEJSON_FIELD_KIND_STRING_ARRAY;
    meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
    meta->field.overflow_policy =
        ljlua_parse_overflow_policy(L, index, "overflow");
    lua_getfield(L, index, "fixed_capacity");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      meta->fixed_array_capacity = (size_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    break;
  case LJLUA_FIELD_I64_ARRAY:
    meta->field.kind = LONEJSON_FIELD_KIND_I64_ARRAY;
    meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
    meta->field.elem_size = sizeof(lonejson_int64);
    meta->field.overflow_policy =
        ljlua_parse_overflow_policy(L, index, "overflow");
    lua_getfield(L, index, "fixed_capacity");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      meta->fixed_array_capacity = (size_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    break;
  case LJLUA_FIELD_U64_ARRAY:
    meta->field.kind = LONEJSON_FIELD_KIND_U64_ARRAY;
    meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
    meta->field.elem_size = sizeof(lonejson_uint64);
    meta->field.overflow_policy =
        ljlua_parse_overflow_policy(L, index, "overflow");
    lua_getfield(L, index, "fixed_capacity");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      meta->fixed_array_capacity = (size_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    break;
  case LJLUA_FIELD_F64_ARRAY:
    meta->field.kind = LONEJSON_FIELD_KIND_F64_ARRAY;
    meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
    meta->field.elem_size = sizeof(double);
    meta->field.overflow_policy =
        ljlua_parse_overflow_policy(L, index, "overflow");
    lua_getfield(L, index, "fixed_capacity");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      meta->fixed_array_capacity = (size_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    break;
  case LJLUA_FIELD_BOOL_ARRAY:
    meta->field.kind = LONEJSON_FIELD_KIND_BOOL_ARRAY;
    meta->field.storage = LONEJSON_STORAGE_DYNAMIC;
    meta->field.elem_size = sizeof(bool);
    meta->field.overflow_policy =
        ljlua_parse_overflow_policy(L, index, "overflow");
    lua_getfield(L, index, "fixed_capacity");
    if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
      meta->fixed_array_capacity = (size_t)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    break;
  default:
    return luaL_error(L, "unsupported field kind");
  }

  meta->member_size = ljlua_member_size_for_kind(meta);
  if (meta->member_size == 0u) {
    return luaL_error(L, "failed to size field '%s'", meta->name);
  }
  if (meta->fixed_array_capacity != 0u) {
    i = ljlua_fixed_array_elem_size(meta);
    if (i == 0u) {
      return luaL_error(L, "invalid fixed-capacity array field '%s'",
                        meta->name);
    }
  }
  return 0;
}

static int ljlua_finalize_schema(ljlua_schema *schema) {
  size_t offset;
  size_t i;

  offset = 0u;
  for (i = 0u; i < schema->field_count; ++i) {
    ljlua_field_meta *meta = &schema->metas[i];
    size_t align = sizeof(void *);

    if (meta->member_size >= sizeof(void *)) {
      align = sizeof(void *);
    } else if (meta->member_size >= sizeof(double)) {
      align = sizeof(double);
    }
    offset = ljlua_align(offset, align);
    meta->field.struct_offset = offset;
    schema->fields[i] = meta->field;
    schema->fields[i].struct_offset = offset;
    offset += meta->member_size;
  }
  for (i = 0u; i < schema->field_count; ++i) {
    ljlua_field_meta *meta = &schema->metas[i];

    if (meta->fixed_array_capacity != 0u) {
      size_t elem_size = ljlua_fixed_array_elem_size(meta);
      size_t bytes = elem_size * meta->fixed_array_capacity;

      offset = ljlua_align(offset, sizeof(void *));
      meta->fixed_array_offset = offset;
      offset += bytes;
    }
  }
  schema->record_size = ljlua_align(offset, sizeof(void *));
  schema->map.name = schema->name;
  schema->map.struct_size = schema->record_size;
  schema->map.field_count = schema->field_count;
  schema->map.fields = schema->fields;
  return 0;
}

static void ljlua_schema_destroy(ljlua_schema *schema) {
  size_t i;
  ljlua_compiled_path *compiled;

  if (schema == NULL) {
    return;
  }
  for (i = 0u; i < schema->field_count; ++i) {
    free(schema->metas[i].name);
    if (schema->metas[i].subschema != NULL) {
      ljlua_schema_destroy(schema->metas[i].subschema);
    }
    if (schema->metas[i].field.spool_options != NULL) {
      free((char *)schema->metas[i].field.spool_options->temp_dir);
      free((void *)schema->metas[i].field.spool_options);
    }
  }
  compiled = schema->path_cache;
  while (compiled != NULL) {
    ljlua_compiled_path *next = compiled->next;
    free(compiled->steps);
    free(compiled->path);
    free(compiled);
    compiled = next;
  }
  free(schema->scratch_record);
  free(schema->encode_buffer);
  free(schema->fields);
  free(schema->metas);
  free(schema->name);
  free(schema);
}

static unsigned char *ljlua_schema_borrow_scratch(ljlua_schema *schema,
                                                  int *owned) {
  if (schema != NULL && schema->scratch_record != NULL &&
      !schema->scratch_in_use) {
    schema->scratch_in_use = 1;
    if (owned != NULL) {
      *owned = 0;
    }
    return schema->scratch_record;
  }
  if (owned != NULL) {
    *owned = 1;
  }
  return (unsigned char *)malloc(schema != NULL ? schema->record_size : 0u);
}

static void ljlua_schema_release_scratch(ljlua_schema *schema,
                                         unsigned char *record, int owned) {
  if (record == NULL) {
    return;
  }
  if (owned) {
    free(record);
  } else if (schema != NULL) {
    schema->scratch_in_use = 0;
  }
}

static void ljlua_prepare_record_storage(ljlua_schema *schema, void *record) {
  size_t i;

  memset(record, 0, schema->record_size);
  for (i = 0u; i < schema->field_count; ++i) {
    ljlua_field_meta *meta = &schema->metas[i];
    unsigned char *base = (unsigned char *)record + meta->field.struct_offset;
    void *fixed_ptr = (unsigned char *)record + meta->fixed_array_offset;

    if (meta->fixed_array_capacity == 0u) {
      continue;
    }
    switch (meta->lua_kind) {
    case LJLUA_FIELD_STRING_ARRAY: {
      lonejson_string_array *arr = (lonejson_string_array *)base;
      arr->items = (char **)fixed_ptr;
      arr->capacity = meta->fixed_array_capacity;
      arr->flags = LONEJSON_ARRAY_FIXED_CAPACITY;
      break;
    }
    case LJLUA_FIELD_I64_ARRAY: {
      lonejson_i64_array *arr = (lonejson_i64_array *)base;
      arr->items = (lonejson_int64 *)fixed_ptr;
      arr->capacity = meta->fixed_array_capacity;
      arr->flags = LONEJSON_ARRAY_FIXED_CAPACITY;
      break;
    }
    case LJLUA_FIELD_U64_ARRAY: {
      lonejson_u64_array *arr = (lonejson_u64_array *)base;
      arr->items = (lonejson_uint64 *)fixed_ptr;
      arr->capacity = meta->fixed_array_capacity;
      arr->flags = LONEJSON_ARRAY_FIXED_CAPACITY;
      break;
    }
    case LJLUA_FIELD_F64_ARRAY: {
      lonejson_f64_array *arr = (lonejson_f64_array *)base;
      arr->items = (double *)fixed_ptr;
      arr->capacity = meta->fixed_array_capacity;
      arr->flags = LONEJSON_ARRAY_FIXED_CAPACITY;
      break;
    }
    case LJLUA_FIELD_BOOL_ARRAY: {
      lonejson_bool_array *arr = (lonejson_bool_array *)base;
      arr->items = (bool *)fixed_ptr;
      arr->capacity = meta->fixed_array_capacity;
      arr->flags = LONEJSON_ARRAY_FIXED_CAPACITY;
      break;
    }
    case LJLUA_FIELD_OBJECT_ARRAY: {
      lonejson_object_array *arr = (lonejson_object_array *)base;
      arr->items = fixed_ptr;
      arr->capacity = meta->fixed_array_capacity;
      arr->elem_size = meta->subschema->record_size;
      arr->flags = LONEJSON_ARRAY_FIXED_CAPACITY;
      break;
    }
    default:
      break;
    }
  }
  lonejson__init_map(&schema->map, record);
}

static ljlua_schema_ud *ljlua_check_schema(lua_State *L, int index) {
  return (ljlua_schema_ud *)luaL_checkudata(L, index, LJLUA_SCHEMA_MT);
}

static ljlua_record_ud *ljlua_check_record(lua_State *L, int index) {
  return (ljlua_record_ud *)luaL_checkudata(L, index, LJLUA_RECORD_MT);
}

static ljlua_stream_ud *ljlua_check_stream(lua_State *L, int index) {
  return (ljlua_stream_ud *)luaL_checkudata(L, index, LJLUA_STREAM_MT);
}

static ljlua_spool_ud *ljlua_check_spool(lua_State *L, int index) {
  return (ljlua_spool_ud *)luaL_checkudata(L, index, LJLUA_SPOOL_MT);
}

static ljlua_path_ud *ljlua_check_path(lua_State *L, int index) {
  return (ljlua_path_ud *)luaL_checkudata(L, index, LJLUA_PATH_MT);
}

static FILE *ljlua_check_file(lua_State *L, int index) {
  luaL_Stream *stream =
      (luaL_Stream *)luaL_checkudata(L, index, LUA_FILEHANDLE);

  luaL_argcheck(L, stream->closef != NULL, index, "closed file");
  return stream->f;
}

static int ljlua_parse_parse_options(lua_State *L, int index,
                                     lonejson_parse_options *options) {
  *options = lonejson_default_parse_options();
  if (index == 0 || lua_isnoneornil(L, index)) {
    return 1;
  }
  luaL_checktype(L, index, LUA_TTABLE);
  lua_getfield(L, index, "clear_destination");
  if (!lua_isnil(L, -1)) {
    options->clear_destination = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);
  lua_getfield(L, index, "reject_duplicate_keys");
  if (!lua_isnil(L, -1)) {
    options->reject_duplicate_keys = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);
  lua_getfield(L, index, "max_depth");
  if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
    options->max_depth = (size_t)lua_tointeger(L, -1);
  }
  lua_pop(L, 1);
  return 1;
}

static int ljlua_schema_compile_path(lua_State *L) {
  ljlua_schema_ud *schema_ud;
  const char *path;
  ljlua_compiled_path *compiled;
  ljlua_path_ud *path_ud;

  schema_ud = ljlua_check_schema(L, 1);
  path = luaL_checkstring(L, 2);
  compiled = ljlua_find_compiled_path(schema_ud->schema, path);
  if (compiled == NULL) {
    compiled = ljlua_compile_path(L, schema_ud->schema, path);
  }
  path_ud = (ljlua_path_ud *)lua_newuserdatauv(L, sizeof(*path_ud), 0);
  memset(path_ud, 0, sizeof(*path_ud));
  path_ud->schema = schema_ud->schema;
  path_ud->compiled = compiled;
  lua_pushvalue(L, 1);
  path_ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_PATH_MT);
  return 1;
}

static int ljlua_schema_compile_get(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 2);
  ljlua_path_ud *path_ud;
  ljlua_compiled_path *compiled;

  compiled = ljlua_find_compiled_path(schema_ud->schema, path);
  if (compiled == NULL) {
    compiled = ljlua_compile_path(L, schema_ud->schema, path);
  }
  path_ud = (ljlua_path_ud *)lua_newuserdatauv(L, sizeof(*path_ud), 0);
  path_ud->schema = schema_ud->schema;
  path_ud->compiled = compiled;
  lua_pushvalue(L, 1);
  path_ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_PATH_MT);
  lua_pushvalue(L, -1);
  lua_pushcclosure(L, ljlua_getter_call, 1);
  return 1;
}

static int ljlua_schema_compile_count(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 2);
  ljlua_path_ud *path_ud;
  ljlua_compiled_path *compiled;

  compiled = ljlua_find_compiled_path(schema_ud->schema, path);
  if (compiled == NULL) {
    compiled = ljlua_compile_path(L, schema_ud->schema, path);
  }
  path_ud = (ljlua_path_ud *)lua_newuserdatauv(L, sizeof(*path_ud), 0);
  path_ud->schema = schema_ud->schema;
  path_ud->compiled = compiled;
  lua_pushvalue(L, 1);
  path_ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_PATH_MT);
  lua_pushvalue(L, -1);
  lua_pushcclosure(L, ljlua_counter_call, 1);
  return 1;
}

static int ljlua_parse_write_options(lua_State *L, int index,
                                     lonejson_write_options *options) {
  *options = lonejson_default_write_options();
  if (index == 0 || lua_isnoneornil(L, index)) {
    return 1;
  }
  luaL_checktype(L, index, LUA_TTABLE);
  options->overflow_policy = ljlua_parse_overflow_policy(L, index, "overflow");
  lua_getfield(L, index, "pretty");
  if (!lua_isnil(L, -1)) {
    options->pretty = lua_toboolean(L, -1) ? 1 : 0;
  }
  lua_pop(L, 1);
  return 1;
}

static int ljlua_push_spool(lua_State *L, lonejson_spooled *spool, int kind,
                            int owner_index, int copy_value) {
  ljlua_spool_ud *ud;
  lonejson_error error;
  unsigned char buffer[4096];

  ud = (ljlua_spool_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  luaL_setmetatable(L, LJLUA_SPOOL_MT);
  ud->kind = kind;
  ud->owner_ref = LUA_NOREF;
  if (!copy_value) {
    ud->spool = spool;
    if (owner_index != 0) {
      lua_pushvalue(L, owner_index);
      ud->owner_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    return 1;
  }
  lonejson_spooled_init(&ud->owned, NULL);
  if (spool->memory_limit != 0u || spool->max_bytes != 0u ||
      spool->temp_dir != NULL) {
    lonejson_spool_options opts;
    opts.memory_limit = spool->memory_limit;
    opts.max_bytes = spool->max_bytes;
    opts.temp_dir = spool->temp_dir;
    lonejson_spooled_init(&ud->owned, &opts);
  }
  if (lonejson_spooled_rewind(spool, &error) != LONEJSON_STATUS_OK) {
    return luaL_error(L, "failed to rewind spool: %s", error.message);
  }
  for (;;) {
    lonejson_read_result rr =
        lonejson_spooled_read(spool, buffer, sizeof(buffer));
    if (rr.error_code != 0) {
      return luaL_error(L, "failed to clone spool");
    }
    if (rr.bytes_read != 0u &&
        lonejson__spooled_append(&ud->owned, buffer, rr.bytes_read, &error) !=
            LONEJSON_STATUS_OK) {
      return luaL_error(L, "failed to clone spool: %s", error.message);
    }
    if (rr.eof) {
      break;
    }
  }
  lonejson_spooled_rewind(spool, &error);
  ud->spool = &ud->owned;
  ud->owns_data = 1;
  return 1;
}

static int ljlua_push_value(lua_State *L, ljlua_schema *schema, void *record,
                            ljlua_field_meta *meta, int owner_index);

static int ljlua_record_to_table(lua_State *L, ljlua_schema *schema,
                                 void *record) {
  size_t i;

  lua_createtable(L, 0, (int)schema->field_count);
  for (i = 0u; i < schema->field_count; ++i) {
    ljlua_push_value(L, schema, record, &schema->metas[i], 0);
    lua_setfield(L, -2, schema->metas[i].name);
  }
  return 1;
}

static int ljlua_push_value(lua_State *L, ljlua_schema *schema, void *record,
                            ljlua_field_meta *meta, int owner_index) {
  void *ptr;
  size_t i;

  (void)schema;
  ptr = (unsigned char *)record + meta->field.struct_offset;
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING:
    if (meta->field.storage == LONEJSON_STORAGE_DYNAMIC) {
      const char *text = *(const char *const *)ptr;
      if (text == NULL) {
        lua_pushnil(L);
      } else {
        lua_pushstring(L, text);
      }
    } else {
      lua_pushstring(L, (const char *)ptr);
    }
    return 1;
  case LJLUA_FIELD_SPOOLED_TEXT:
    return ljlua_push_spool(L, (lonejson_spooled *)ptr, meta->lua_kind,
                            owner_index, owner_index == 0);
  case LJLUA_FIELD_SPOOLED_BYTES:
    return ljlua_push_spool(L, (lonejson_spooled *)ptr, meta->lua_kind,
                            owner_index, owner_index == 0);
  case LJLUA_FIELD_I64:
    lua_pushinteger(L, (lua_Integer) * (lonejson_int64 *)ptr);
    return 1;
  case LJLUA_FIELD_U64:
    return ljlua_push_u64(L, *(lonejson_uint64 *)ptr);
  case LJLUA_FIELD_F64:
    lua_pushnumber(L, *(double *)ptr);
    return 1;
  case LJLUA_FIELD_BOOL:
    lua_pushboolean(L, *(bool *)ptr);
    return 1;
  case LJLUA_FIELD_OBJECT:
    return ljlua_record_to_table(L, meta->subschema, ptr);
  case LJLUA_FIELD_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    lua_createtable(L, (int)arr->count, 0);
    for (i = 0u; i < arr->count; ++i) {
      lua_pushstring(L, arr->items[i] ? arr->items[i] : "");
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
  }
  case LJLUA_FIELD_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    lua_createtable(L, (int)arr->count, 0);
    for (i = 0u; i < arr->count; ++i) {
      lua_pushinteger(L, (lua_Integer)arr->items[i]);
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
  }
  case LJLUA_FIELD_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    lua_createtable(L, (int)arr->count, 0);
    for (i = 0u; i < arr->count; ++i) {
      ljlua_push_u64(L, arr->items[i]);
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
  }
  case LJLUA_FIELD_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    lua_createtable(L, (int)arr->count, 0);
    for (i = 0u; i < arr->count; ++i) {
      lua_pushnumber(L, arr->items[i]);
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
  }
  case LJLUA_FIELD_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    lua_createtable(L, (int)arr->count, 0);
    for (i = 0u; i < arr->count; ++i) {
      lua_pushboolean(L, arr->items[i]);
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
  }
  case LJLUA_FIELD_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    lua_createtable(L, (int)arr->count, 0);
    for (i = 0u; i < arr->count; ++i) {
      void *elem =
          (unsigned char *)arr->items + (i * meta->subschema->record_size);
      ljlua_record_to_table(L, meta->subschema, elem);
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
  }
  default:
    lua_pushnil(L);
    return 1;
  }
}

static ljlua_field_meta *ljlua_find_meta(ljlua_schema *schema,
                                         const char *name) {
  size_t i;

  for (i = 0u; i < schema->field_count; ++i) {
    if (strcmp(schema->metas[i].name, name) == 0) {
      return &schema->metas[i];
    }
  }
  return NULL;
}

static ljlua_compiled_path *ljlua_find_compiled_path(ljlua_schema *schema,
                                                     const char *path) {
  ljlua_compiled_path *compiled;

  compiled = schema->path_cache;
  while (compiled != NULL) {
    if (strcmp(compiled->path, path) == 0) {
      return compiled;
    }
    compiled = compiled->next;
  }
  return NULL;
}

static ljlua_compiled_path *
ljlua_compile_path(lua_State *L, ljlua_schema *schema, const char *path) {
  const char *cursor;
  ljlua_schema *current_schema;
  ljlua_compiled_path *compiled;
  size_t capacity;

  cursor = path;
  current_schema = schema;
  compiled = (ljlua_compiled_path *)calloc(1u, sizeof(*compiled));
  if (compiled == NULL) {
    luaL_error(L, "failed to allocate record path cache");
  }
  compiled->path = ljlua_strdup(path);
  capacity = 4u;
  compiled->steps =
      (ljlua_path_step *)calloc(capacity, sizeof(compiled->steps[0]));
  if (compiled->steps == NULL) {
    free(compiled->path);
    free(compiled);
    luaL_error(L, "failed to allocate record path steps");
  }

  while (*cursor != '\0') {
    char segment[128];
    size_t seg_len;
    ljlua_field_meta *meta;
    size_t index;

    seg_len = 0u;
    while (*cursor != '\0' && *cursor != '.' && *cursor != '[') {
      if (seg_len + 1u >= sizeof(segment)) {
        luaL_error(L, "path segment too long");
      }
      segment[seg_len++] = *cursor++;
    }
    if (seg_len == 0u) {
      luaL_error(L, "malformed record path '%s'", path);
    }
    segment[seg_len] = '\0';
    meta = ljlua_find_meta(current_schema, segment);
    if (meta == NULL) {
      luaL_error(L, "unknown field '%s' in path '%s'", segment, path);
    }
    index = 0u;
    if (*cursor == '[') {
      ++cursor;
      while (*cursor >= '0' && *cursor <= '9') {
        index = index * 10u + (size_t)(*cursor - '0');
        ++cursor;
      }
      if (*cursor != ']') {
        luaL_error(L, "malformed array index in path '%s'", path);
      }
      ++cursor;
    }
    if (compiled->step_count == capacity) {
      ljlua_path_step *grown;
      capacity *= 2u;
      grown = (ljlua_path_step *)realloc(compiled->steps,
                                         capacity * sizeof(compiled->steps[0]));
      if (grown == NULL) {
        luaL_error(L, "failed to grow record path cache");
      }
      compiled->steps = grown;
    }
    compiled->steps[compiled->step_count].meta = meta;
    compiled->steps[compiled->step_count].index = index;
    ++compiled->step_count;

    if (*cursor == '.') {
      if (index != 0u) {
        if (meta->lua_kind != LJLUA_FIELD_OBJECT_ARRAY) {
          luaL_error(L, "field '%s' is not an object array in path '%s'",
                     segment, path);
        }
      } else if (meta->lua_kind != LJLUA_FIELD_OBJECT) {
        luaL_error(L, "field '%s' is not an object in path '%s'", segment,
                   path);
      }
      current_schema = meta->subschema;
      ++cursor;
      continue;
    }
    if (*cursor != '\0') {
      luaL_error(L, "malformed record path '%s'", path);
    }
    if (index != 0u && meta->lua_kind != LJLUA_FIELD_OBJECT_ARRAY &&
        meta->lua_kind != LJLUA_FIELD_STRING_ARRAY &&
        meta->lua_kind != LJLUA_FIELD_I64_ARRAY &&
        meta->lua_kind != LJLUA_FIELD_U64_ARRAY &&
        meta->lua_kind != LJLUA_FIELD_F64_ARRAY &&
        meta->lua_kind != LJLUA_FIELD_BOOL_ARRAY) {
      luaL_error(L, "field '%s' is not an array in path '%s'", segment, path);
    }
  }

  compiled->next = schema->path_cache;
  schema->path_cache = compiled;
  return compiled;
}

static int ljlua_push_compiled_path_value(lua_State *L, void *record,
                                          ljlua_compiled_path *compiled,
                                          int owner_index) {
  size_t i;
  ljlua_schema *schema;

  schema = NULL;
  for (i = 0u; i < compiled->step_count; ++i) {
    ljlua_field_meta *meta = compiled->steps[i].meta;
    size_t index = compiled->steps[i].index;
    void *ptr = (unsigned char *)record + meta->field.struct_offset;
    int final = (i + 1u == compiled->step_count);

    (void)schema;
    if (index != 0u) {
      switch (meta->lua_kind) {
      case LJLUA_FIELD_STRING_ARRAY: {
        lonejson_string_array *arr = (lonejson_string_array *)ptr;
        if (index > arr->count || arr->items[index - 1u] == NULL) {
          lua_pushnil(L);
        } else {
          lua_pushstring(L, arr->items[index - 1u]);
        }
        return 1;
      }
      case LJLUA_FIELD_I64_ARRAY: {
        lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
        if (index > arr->count) {
          lua_pushnil(L);
        } else {
          lua_pushinteger(L, (lua_Integer)arr->items[index - 1u]);
        }
        return 1;
      }
      case LJLUA_FIELD_U64_ARRAY: {
        lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
        if (index > arr->count) {
          lua_pushnil(L);
        } else {
          lua_pushinteger(L, (lua_Integer)arr->items[index - 1u]);
        }
        return 1;
      }
      case LJLUA_FIELD_F64_ARRAY: {
        lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
        if (index > arr->count) {
          lua_pushnil(L);
        } else {
          lua_pushnumber(L, arr->items[index - 1u]);
        }
        return 1;
      }
      case LJLUA_FIELD_BOOL_ARRAY: {
        lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
        if (index > arr->count) {
          lua_pushnil(L);
        } else {
          lua_pushboolean(L, arr->items[index - 1u]);
        }
        return 1;
      }
      case LJLUA_FIELD_OBJECT_ARRAY: {
        lonejson_object_array *arr = (lonejson_object_array *)ptr;
        if (index > arr->count) {
          lua_pushnil(L);
          return 1;
        }
        record = (unsigned char *)arr->items +
                 ((index - 1u) * meta->subschema->record_size);
        if (final) {
          return ljlua_record_to_table(L, meta->subschema, record);
        }
        break;
      }
      default:
        lua_pushnil(L);
        return 1;
      }
    } else {
      if (final) {
        return ljlua_push_value(L, NULL, record, meta, owner_index);
      }
      record = ptr;
    }
  }
  lua_pushnil(L);
  return 1;
}

static int ljlua_assign_spool_from_lua(lua_State *L, lonejson_spooled *spool,
                                       int kind, int index) {
  size_t len;
  const char *data;
  lonejson_error error;

  lonejson_spooled_reset(spool);
  if (lua_isuserdata(L, index)) {
    ljlua_spool_ud *src =
        (ljlua_spool_ud *)luaL_testudata(L, index, LJLUA_SPOOL_MT);
    unsigned char buffer[4096];

    if (src == NULL) {
      return luaL_error(L, "expected string or lonejson spool");
    }
    if (lonejson_spooled_rewind(src->spool, &error) != LONEJSON_STATUS_OK) {
      return luaL_error(L, "%s", error.message);
    }
    for (;;) {
      lonejson_read_result rr =
          lonejson_spooled_read(src->spool, buffer, sizeof(buffer));
      if (rr.error_code != 0) {
        return luaL_error(L, "failed to read spool source");
      }
      if (rr.bytes_read != 0u &&
          lonejson__spooled_append(spool, buffer, rr.bytes_read, &error) !=
              LONEJSON_STATUS_OK) {
        return luaL_error(L, "%s", error.message);
      }
      if (rr.eof) {
        break;
      }
    }
    return 1;
  }
  data = luaL_checklstring(L, index, &len);
  if (kind == LJLUA_FIELD_SPOOLED_TEXT) {
    if (lonejson__spooled_append(spool, (const unsigned char *)data, len,
                                 &error) != LONEJSON_STATUS_OK) {
      return luaL_error(L, "%s", error.message);
    }
  } else {
    if (lonejson__spooled_append(spool, (const unsigned char *)data, len,
                                 &error) != LONEJSON_STATUS_OK) {
      return luaL_error(L, "%s", error.message);
    }
  }
  return 1;
}

static int ljlua_assign_table_to_record(lua_State *L, ljlua_schema *schema,
                                        void *record, int table_index);

static void ljlua_init_helper_parser(lonejson_parser *parser) {
  static const lonejson_map helper_map = {"lua-helper", 0u, NULL, 0u};
  static unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE];

  lonejson__parser_init_state(parser, &helper_map, NULL, NULL, 0, workspace,
                              sizeof(workspace));
}

static int ljlua_assign_field_from_lua(lua_State *L, ljlua_schema *schema,
                                       void *record, ljlua_field_meta *meta,
                                       int value_index) {
  void *ptr;
  size_t i;

  (void)schema;
  ptr = (unsigned char *)record + meta->field.struct_offset;
  if (lua_isnil(L, value_index)) {
    lonejson_parser parser;

    ljlua_init_helper_parser(&parser);
    if (lonejson__assign_null(&parser, &meta->field, ptr) ==
        LONEJSON_STATUS_OK) {
      return 1;
    }
  }
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING: {
    size_t len;
    const char *text = luaL_checklstring(L, value_index, &len);
    lonejson_error error;
    lonejson_parser parser;
    unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE];

    lonejson__parser_init_state(&parser, &schema->map, record, NULL, 0,
                                workspace, sizeof(workspace));
    if (lonejson__assign_string(&parser, &meta->field, ptr, text, len) !=
        LONEJSON_STATUS_OK) {
      return luaL_error(L, "%s", parser.error.message);
    }
    (void)error;
    return 1;
  }
  case LJLUA_FIELD_SPOOLED_TEXT:
  case LJLUA_FIELD_SPOOLED_BYTES:
    return ljlua_assign_spool_from_lua(L, (lonejson_spooled *)ptr,
                                       meta->lua_kind, value_index);
  case LJLUA_FIELD_I64:
    *(lonejson_int64 *)ptr = (lonejson_int64)luaL_checkinteger(L, value_index);
    return 1;
  case LJLUA_FIELD_U64:
    *(lonejson_uint64 *)ptr = ljlua_check_u64(L, value_index);
    return 1;
  case LJLUA_FIELD_F64:
    *(double *)ptr = (double)luaL_checknumber(L, value_index);
    return 1;
  case LJLUA_FIELD_BOOL:
    *(bool *)ptr = lua_toboolean(L, value_index) ? true : false;
    return 1;
  case LJLUA_FIELD_OBJECT:
    luaL_checktype(L, value_index, LUA_TTABLE);
    return ljlua_assign_table_to_record(L, meta->subschema, ptr, value_index);
  case LJLUA_FIELD_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    lonejson_parser parser;
    luaL_checktype(L, value_index, LUA_TTABLE);
    ljlua_init_helper_parser(&parser);
    for (i = 1u; i <= (size_t)lua_rawlen(L, value_index); ++i) {
      size_t len;
      const char *text;

      lua_rawgeti(L, value_index, (lua_Integer)i);
      text = luaL_checklstring(L, -1, &len);
      if (lonejson__array_append_string(&parser, &meta->field, arr, text,
                                        len) != LONEJSON_STATUS_OK) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", parser.error.message);
      }
      lua_pop(L, 1);
    }
    return 1;
  }
  case LJLUA_FIELD_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    lonejson_parser parser;
    luaL_checktype(L, value_index, LUA_TTABLE);
    ljlua_init_helper_parser(&parser);
    for (i = 1u; i <= (size_t)lua_rawlen(L, value_index); ++i) {
      lua_rawgeti(L, value_index, (lua_Integer)i);
      if (lonejson__array_append_i64(
              &parser, &meta->field, arr,
              (lonejson_int64)luaL_checkinteger(L, -1)) != LONEJSON_STATUS_OK) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", parser.error.message);
      }
      lua_pop(L, 1);
    }
    return 1;
  }
  case LJLUA_FIELD_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    lonejson_parser parser;
    luaL_checktype(L, value_index, LUA_TTABLE);
    ljlua_init_helper_parser(&parser);
    for (i = 1u; i <= (size_t)lua_rawlen(L, value_index); ++i) {
      lua_rawgeti(L, value_index, (lua_Integer)i);
      if (lonejson__array_append_u64(&parser, &meta->field, arr,
                                     ljlua_check_u64(L, -1)) !=
          LONEJSON_STATUS_OK) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", parser.error.message);
      }
      lua_pop(L, 1);
    }
    return 1;
  }
  case LJLUA_FIELD_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    lonejson_parser parser;
    luaL_checktype(L, value_index, LUA_TTABLE);
    ljlua_init_helper_parser(&parser);
    for (i = 1u; i <= (size_t)lua_rawlen(L, value_index); ++i) {
      lua_rawgeti(L, value_index, (lua_Integer)i);
      if (lonejson__array_append_f64(&parser, &meta->field, arr,
                                     (double)luaL_checknumber(L, -1)) !=
          LONEJSON_STATUS_OK) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", parser.error.message);
      }
      lua_pop(L, 1);
    }
    return 1;
  }
  case LJLUA_FIELD_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    lonejson_parser parser;
    luaL_checktype(L, value_index, LUA_TTABLE);
    ljlua_init_helper_parser(&parser);
    for (i = 1u; i <= (size_t)lua_rawlen(L, value_index); ++i) {
      lua_rawgeti(L, value_index, (lua_Integer)i);
      if (lonejson__array_append_bool(&parser, &meta->field, arr,
                                      lua_toboolean(L, -1) ? true : false) !=
          LONEJSON_STATUS_OK) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", parser.error.message);
      }
      lua_pop(L, 1);
    }
    return 1;
  }
  case LJLUA_FIELD_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    size_t n = (size_t)lua_rawlen(L, value_index);
    lonejson_parser parser;
    luaL_checktype(L, value_index, LUA_TTABLE);
    ljlua_init_helper_parser(&parser);
    for (i = 1u; i <= n; ++i) {
      void *slot;
      lua_rawgeti(L, value_index, (lua_Integer)i);
      slot = lonejson__object_array_append_slot(&parser, &meta->field, arr);
      if (slot == NULL) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", parser.error.message);
      }
      ljlua_prepare_record_storage(meta->subschema, slot);
      ljlua_assign_table_to_record(L, meta->subschema, slot, lua_gettop(L));
      lua_pop(L, 1);
    }
    return 1;
  }
  default:
    return luaL_error(L, "unsupported assignment kind");
  }
}

static int ljlua_assign_table_to_record(lua_State *L, ljlua_schema *schema,
                                        void *record, int table_index) {
  size_t i;

  if (table_index < 0) {
    table_index = lua_gettop(L) + table_index + 1;
  }
  for (i = 0u; i < schema->field_count; ++i) {
    lua_getfield(L, table_index, schema->metas[i].name);
    if (!lua_isnil(L, -1)) {
      ljlua_assign_field_from_lua(L, schema, record, &schema->metas[i],
                                  lua_gettop(L));
    }
    lua_pop(L, 1);
  }
  return 1;
}

static int ljlua_schema_new(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  size_t count;
  size_t i;
  ljlua_schema *schema;
  ljlua_schema_ud *ud;

  luaL_checktype(L, 2, LUA_TTABLE);
  count = (size_t)lua_rawlen(L, 2);
  schema = (ljlua_schema *)calloc(1u, sizeof(*schema));
  if (schema == NULL) {
    return luaL_error(L, "failed to allocate schema");
  }
  schema->name = ljlua_strdup(name);
  schema->field_count = count;
  schema->metas = (ljlua_field_meta *)calloc(count, sizeof(schema->metas[0]));
  if (schema->metas == NULL) {
    ljlua_schema_destroy(schema);
    return luaL_error(L, "failed to allocate schema fields");
  }
  schema->fields = (lonejson_field *)calloc(count, sizeof(schema->fields[0]));
  if (schema->fields == NULL) {
    ljlua_schema_destroy(schema);
    return luaL_error(L, "failed to allocate runtime map fields");
  }
  for (i = 0u; i < count; ++i) {
    lua_rawgeti(L, 2, (lua_Integer)i + 1);
    ljlua_compile_field(L, lua_gettop(L), &schema->metas[i]);
    lua_pop(L, 1);
  }
  ljlua_finalize_schema(schema);
  if (schema->record_size != 0u) {
    schema->scratch_record = (unsigned char *)malloc(schema->record_size);
    if (schema->scratch_record == NULL) {
      ljlua_schema_destroy(schema);
      return luaL_error(L, "failed to allocate schema scratch record");
    }
  }
  ud = (ljlua_schema_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  ud->schema = schema;
  luaL_setmetatable(L, LJLUA_SCHEMA_MT);
  return 1;
}

static int ljlua_schema_gc(lua_State *L) {
  ljlua_schema_ud *ud = ljlua_check_schema(L, 1);
  ljlua_schema_destroy(ud->schema);
  ud->schema = NULL;
  return 0;
}

static int ljlua_schema_new_record(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  ljlua_record_ud *record_ud;

  record_ud = (ljlua_record_ud *)lua_newuserdatauv(
      L, sizeof(*record_ud) + schema_ud->schema->record_size, 0);
  record_ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  record_ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_RECORD_MT);
  ljlua_prepare_record_storage(record_ud->schema, record_ud->data);
  return 1;
}

static int ljlua_record_gc(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  if (ud->schema != NULL) {
    lonejson_cleanup(&ud->schema->map, ud->data);
  }
  if (ud->schema_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->schema_ref);
    ud->schema_ref = LUA_NOREF;
  }
  return 0;
}

static int ljlua_record_index(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  const char *key = luaL_checkstring(L, 2);
  if (strcmp(key, "get") == 0) {
    lua_pushcfunction(L, ljlua_record_get);
    return 1;
  }
  if (strcmp(key, "to_table") == 0) {
    lua_pushcfunction(L, ljlua_record_to_table_method);
    return 1;
  }
  if (strcmp(key, "count") == 0) {
    lua_pushcfunction(L, ljlua_record_count);
    return 1;
  }
  if (strcmp(key, "clear") == 0) {
    lua_pushcfunction(L, ljlua_record_clear);
    return 1;
  }
  {
    ljlua_field_meta *meta = ljlua_find_meta(ud->schema, key);
    if (meta != NULL) {
      return ljlua_push_value(L, ud->schema, ud->data, meta, 1);
    }
  }
  lua_pushnil(L);
  return 1;
}

static int ljlua_record_to_table_method(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  return ljlua_record_to_table(L, ud->schema, ud->data);
}

static int ljlua_record_get(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  ljlua_compiled_path *compiled = NULL;

  if (lua_type(L, 2) == LUA_TSTRING) {
    const char *path = lua_tostring(L, 2);
    compiled = ljlua_find_compiled_path(ud->schema, path);
    if (compiled == NULL) {
      compiled = ljlua_compile_path(L, ud->schema, path);
    }
  } else {
    ljlua_path_ud *path_ud = ljlua_check_path(L, 2);
    if (path_ud->schema != ud->schema) {
      return luaL_error(L, "path belongs to a different schema");
    }
    compiled = path_ud->compiled;
  }
  return ljlua_push_compiled_path_value(L, ud->data, compiled, 1);
}

static int ljlua_record_count(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  ljlua_field_meta *meta = NULL;
  void *ptr;

  if (lua_type(L, 2) == LUA_TSTRING) {
    const char *key = lua_tostring(L, 2);
    meta = ljlua_find_meta(ud->schema, key);
    if (meta == NULL) {
      return luaL_error(L, "unknown field '%s'", key);
    }
  } else {
    ljlua_path_ud *path_ud = ljlua_check_path(L, 2);
    if (path_ud->schema != ud->schema) {
      return luaL_error(L, "path belongs to a different schema");
    }
    if (path_ud->compiled->step_count != 1u ||
        path_ud->compiled->steps[0].index != 0u) {
      return luaL_error(L, "count path must reference a top-level array field");
    }
    meta = path_ud->compiled->steps[0].meta;
  }
  ptr = (unsigned char *)ud->data + meta->field.struct_offset;
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_string_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_I64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_i64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_U64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_u64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_F64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_f64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_BOOL_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_bool_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_OBJECT_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_object_array *)ptr)->count);
    return 1;
  default:
    return luaL_error(L, "field is not an array");
  }
}

static int ljlua_record_clear(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  lonejson_reset(&ud->schema->map, ud->data);
  return 0;
}

static int ljlua_path_gc(lua_State *L) {
  ljlua_path_ud *ud = ljlua_check_path(L, 1);
  if (ud->schema_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->schema_ref);
    ud->schema_ref = LUA_NOREF;
  }
  ud->schema = NULL;
  ud->compiled = NULL;
  return 0;
}

static int ljlua_path_get(lua_State *L) {
  ljlua_path_ud *path_ud = ljlua_check_path(L, 1);
  ljlua_record_ud *record_ud = ljlua_check_record(L, 2);

  if (path_ud->schema != record_ud->schema) {
    return luaL_error(L, "path belongs to a different schema");
  }
  return ljlua_push_compiled_path_value(L, record_ud->data, path_ud->compiled,
                                        2);
}

static int ljlua_path_count(lua_State *L) {
  ljlua_path_ud *path_ud = ljlua_check_path(L, 1);
  ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
  ljlua_field_meta *meta;
  void *ptr;

  if (path_ud->schema != record_ud->schema) {
    return luaL_error(L, "path belongs to a different schema");
  }
  if (path_ud->compiled->step_count != 1u ||
      path_ud->compiled->steps[0].index != 0u) {
    return luaL_error(L, "count path must reference a top-level array field");
  }
  meta = path_ud->compiled->steps[0].meta;
  ptr = (unsigned char *)record_ud->data + meta->field.struct_offset;
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_string_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_I64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_i64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_U64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_u64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_F64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_f64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_BOOL_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_bool_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_OBJECT_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_object_array *)ptr)->count);
    return 1;
  default:
    return luaL_error(L, "field is not an array");
  }
}

static int ljlua_getter_call(lua_State *L) {
  ljlua_path_ud *path_ud =
      (ljlua_path_ud *)lua_touserdata(L, lua_upvalueindex(1));
  ljlua_record_ud *record_ud = ljlua_check_record(L, 1);

  if (path_ud == NULL || path_ud->schema != record_ud->schema) {
    return luaL_error(L, "path belongs to a different schema");
  }
  return ljlua_push_compiled_path_value(L, record_ud->data, path_ud->compiled,
                                        1);
}

static int ljlua_counter_call(lua_State *L) {
  ljlua_path_ud *path_ud =
      (ljlua_path_ud *)lua_touserdata(L, lua_upvalueindex(1));
  ljlua_record_ud *record_ud = ljlua_check_record(L, 1);
  ljlua_field_meta *meta;
  void *ptr;

  if (path_ud == NULL || path_ud->schema != record_ud->schema) {
    return luaL_error(L, "path belongs to a different schema");
  }
  if (path_ud->compiled->step_count != 1u ||
      path_ud->compiled->steps[0].index != 0u) {
    return luaL_error(L, "count path must reference a top-level array field");
  }
  meta = path_ud->compiled->steps[0].meta;
  ptr = (unsigned char *)record_ud->data + meta->field.struct_offset;
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_string_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_I64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_i64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_U64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_u64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_F64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_f64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_BOOL_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_bool_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_OBJECT_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_object_array *)ptr)->count);
    return 1;
  default:
    return luaL_error(L, "field is not an array");
  }
}

static int ljlua_schema_decode_into_buffer(lua_State *L, ljlua_schema *schema,
                                           void *record, const char *json,
                                           size_t len, int options_index) {
  lonejson_parse_options options;
  lonejson_error error;
  lonejson_status status;

  ljlua_parse_parse_options(L, options_index, &options);
  status =
      lonejson_parse_buffer(&schema->map, record, json, len, &options, &error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_push_error(L, &error);
    return 0;
  }
  return 1;
}

static int ljlua_schema_decode_into(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
  size_t len;
  const char *json = luaL_checklstring(L, 3, &len);
  if (record_ud->schema != schema_ud->schema) {
    return luaL_error(L, "record belongs to a different schema");
  }
  if (!ljlua_schema_decode_into_buffer(L, schema_ud->schema, record_ud->data,
                                       json, len, 4)) {
    return 2;
  }
  lua_pushvalue(L, 2);
  return 1;
}

static int ljlua_schema_decode_path(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 2);
  lonejson_parse_options options;
  lonejson_error error;
  lonejson_status status;
  unsigned char *record;
  int owned_record;
  ljlua_parse_parse_options(L, 3, &options);
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  status = lonejson_parse_path(&schema_ud->schema->map, record, path, &options,
                               &error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson_cleanup(&schema_ud->schema->map, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  ljlua_record_to_table(L, schema_ud->schema, record);
  lonejson_cleanup(&schema_ud->schema->map, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_decode_file(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  FILE *fp = ljlua_check_file(L, 2);
  lonejson_parse_options options;
  lonejson_error error;
  lonejson_status status;
  unsigned char *record;
  int owned_record;
  ljlua_parse_parse_options(L, 3, &options);
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  status = lonejson_parse_filep(&schema_ud->schema->map, record, fp, &options,
                                &error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson_cleanup(&schema_ud->schema->map, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  ljlua_record_to_table(L, schema_ud->schema, record);
  lonejson_cleanup(&schema_ud->schema->map, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_decode_fd(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  int fd = ljlua_check_fd_like(L, 2);
  FILE *fp;
  lonejson_parse_options options;
  lonejson_error error;
  lonejson_status status;
  unsigned char *record;
  int owned_record;
  ljlua_parse_parse_options(L, 3, &options);
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  fp = ljlua_dup_fd_to_file(L, fd, "rb");
  status = lonejson_parse_filep(&schema_ud->schema->map, record, fp, &options,
                                &error);
  fclose(fp);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson_cleanup(&schema_ud->schema->map, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  ljlua_record_to_table(L, schema_ud->schema, record);
  lonejson_cleanup(&schema_ud->schema->map, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_decode(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  size_t len;
  const char *json = luaL_checklstring(L, 2, &len);
  unsigned char *record;
  int owned_record;
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  if (!ljlua_schema_decode_into_buffer(L, schema_ud->schema, record, json, len,
                                       3)) {
    lonejson_cleanup(&schema_ud->schema->map, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    return 2;
  }
  ljlua_record_to_table(L, schema_ud->schema, record);
  lonejson_cleanup(&schema_ud->schema->map, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_assign(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
  luaL_checktype(L, 3, LUA_TTABLE);
  if (record_ud->schema != schema_ud->schema) {
    return luaL_error(L, "record belongs to a different schema");
  }
  lonejson_reset(&schema_ud->schema->map, record_ud->data);
  ljlua_assign_table_to_record(L, schema_ud->schema, record_ud->data, 3);
  lua_pushvalue(L, 2);
  return 1;
}

static char *ljlua_serialize_value(lua_State *L, ljlua_schema *schema,
                                   void *record, int options_index,
                                   size_t *out_len) {
  lonejson_write_options options;
  lonejson_error error;
  lonejson_status status;
  size_t needed = 0u;
  size_t capacity;
  char *buffer;

  ljlua_parse_write_options(L, options_index, &options);
  capacity = schema->encode_capacity;
  if (capacity == 0u) {
    capacity = 256u;
  }
  if (schema->encode_buffer == NULL) {
    schema->encode_buffer = (char *)malloc(capacity);
    if (schema->encode_buffer == NULL) {
      luaL_error(L, "failed to allocate encode buffer");
    }
    schema->encode_capacity = capacity;
  }
  for (;;) {
    status = lonejson_serialize_buffer(
        &schema->map, record, schema->encode_buffer, schema->encode_capacity,
        &needed, &options, &error);
    if (status != LONEJSON_STATUS_OVERFLOW) {
      break;
    }
    capacity = schema->encode_capacity * 2u;
    if (capacity < needed + 1u) {
      capacity = needed + 1u;
    }
    if (capacity < 256u) {
      capacity = 256u;
    }
    buffer = (char *)realloc(schema->encode_buffer, capacity);
    if (buffer == NULL) {
      luaL_error(L, "failed to grow encode buffer");
    }
    schema->encode_buffer = buffer;
    schema->encode_capacity = capacity;
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    luaL_error(L, "%s", error.message);
  }
  if (out_len != NULL) {
    *out_len = needed;
  }
  return schema->encode_buffer;
}

static int ljlua_schema_encode(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  size_t out_len = 0u;
  char *json;

  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    if (record_ud->schema != schema_ud->schema) {
      return luaL_error(L, "record belongs to a different schema");
    }
    json = ljlua_serialize_value(L, schema_ud->schema, record_ud->data, 3,
                                 &out_len);
  } else {
    int owned_record;
    unsigned char *record =
        ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
    if (record == NULL) {
      return luaL_error(L, "failed to allocate encode buffer");
    }
    ljlua_prepare_record_storage(schema_ud->schema, record);
    ljlua_assign_table_to_record(L, schema_ud->schema, record, 2);
    json = ljlua_serialize_value(L, schema_ud->schema, record, 3, &out_len);
    lonejson_cleanup(&schema_ud->schema->map, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  }
  lua_pushlstring(L, json, out_len);
  return 1;
}

static int ljlua_schema_write_path(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 3);
  lonejson_write_options options;
  lonejson_error error;
  lonejson_status status;

  ljlua_parse_write_options(L, 4, &options);
  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    status = lonejson_serialize_path(&schema_ud->schema->map, record_ud->data,
                                     path, &options, &error);
  } else {
    int owned_record;
    unsigned char *record =
        ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
    if (record == NULL) {
      return luaL_error(L, "failed to allocate encode buffer");
    }
    ljlua_prepare_record_storage(schema_ud->schema, record);
    ljlua_assign_table_to_record(L, schema_ud->schema, record, 2);
    status = lonejson_serialize_path(&schema_ud->schema->map, record, path,
                                     &options, &error);
    lonejson_cleanup(&schema_ud->schema->map, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return ljlua_push_error(L, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_schema_write_file(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  FILE *fp = ljlua_check_file(L, 3);
  lonejson_write_options options;
  lonejson_error error;
  lonejson_status status;

  ljlua_parse_write_options(L, 4, &options);
  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    status = lonejson_serialize_filep(&schema_ud->schema->map, record_ud->data,
                                      fp, &options, &error);
  } else {
    int owned_record;
    unsigned char *record =
        ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
    if (record == NULL) {
      return luaL_error(L, "failed to allocate encode buffer");
    }
    ljlua_prepare_record_storage(schema_ud->schema, record);
    ljlua_assign_table_to_record(L, schema_ud->schema, record, 2);
    status = lonejson_serialize_filep(&schema_ud->schema->map, record, fp,
                                      &options, &error);
    lonejson_cleanup(&schema_ud->schema->map, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return ljlua_push_error(L, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_schema_write_fd(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  int fd = ljlua_check_fd_like(L, 3);
  FILE *fp = NULL;
  lonejson_write_options options;
  lonejson_error error;
  lonejson_status status;

  ljlua_parse_write_options(L, 4, &options);
  fp = ljlua_dup_fd_to_file(L, fd, "wb");
  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    status = lonejson_serialize_filep(&schema_ud->schema->map, record_ud->data,
                                      fp, &options, &error);
  } else {
    int owned_record;
    unsigned char *record =
        ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
    if (record == NULL) {
      return luaL_error(L, "failed to allocate encode buffer");
    }
    ljlua_prepare_record_storage(schema_ud->schema, record);
    ljlua_assign_table_to_record(L, schema_ud->schema, record, 2);
    status = lonejson_serialize_filep(&schema_ud->schema->map, record, fp,
                                      &options, &error);
    lonejson_cleanup(&schema_ud->schema->map, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  }
  fclose(fp);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return ljlua_push_error(L, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_stream_gc(lua_State *L) {
  ljlua_stream_ud *ud = ljlua_check_stream(L, 1);
  if (ud->stream != NULL) {
    lonejson_stream_close(ud->stream);
    ud->stream = NULL;
  }
  free(ud->owned_input);
  ud->owned_input = NULL;
  ud->owned_input_len = 0u;
  ud->owned_input_offset = 0u;
  if (ud->schema_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->schema_ref);
    ud->schema_ref = LUA_NOREF;
  }
  return 0;
}

static int ljlua_schema_stream_path(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 2);
  lonejson_parse_options options;
  lonejson_error error;
  ljlua_stream_ud *ud;

  ljlua_parse_parse_options(L, 3, &options);
  ud = (ljlua_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->stream = lonejson_stream_open_path(&schema_ud->schema->map, path,
                                         &options, &error);
  if (ud->stream == NULL) {
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_STREAM_MT);
  return 1;
}

static int ljlua_schema_stream_fd(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  int fd = ljlua_check_fd_like(L, 2);
  lonejson_parse_options options;
  lonejson_error error;
  ljlua_stream_ud *ud;

  ljlua_parse_parse_options(L, 3, &options);
  ud = (ljlua_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->stream =
      lonejson_stream_open_fd(&schema_ud->schema->map, fd, &options, &error);
  if (ud->stream == NULL) {
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_STREAM_MT);
  return 1;
}

static int ljlua_schema_stream_file(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  FILE *fp = ljlua_check_file(L, 2);
  lonejson_parse_options options;
  lonejson_error error;
  ljlua_stream_ud *ud;

  ljlua_parse_parse_options(L, 3, &options);
  ud = (ljlua_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->stream =
      lonejson_stream_open_filep(&schema_ud->schema->map, fp, &options, &error);
  if (ud->stream == NULL) {
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_STREAM_MT);
  return 1;
}

static int ljlua_schema_stream_string(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  size_t len;
  const char *json = luaL_checklstring(L, 2, &len);
  lonejson_parse_options options;
  lonejson_error error;
  ljlua_stream_ud *ud;

  ljlua_parse_parse_options(L, 3, &options);
  ud = (ljlua_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->owned_input = (unsigned char *)malloc(len);
  if (ud->owned_input == NULL) {
    return luaL_error(L, "failed to allocate stream input");
  }
  memcpy(ud->owned_input, json, len);
  ud->owned_input_len = len;
  ud->stream = lonejson_stream_open_reader(
      &schema_ud->schema->map, ljlua_mem_stream_read, ud, &options, &error);
  if (ud->stream == NULL) {
    free(ud->owned_input);
    ud->owned_input = NULL;
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_STREAM_MT);
  return 1;
}

static int ljlua_stream_close(lua_State *L) { return ljlua_stream_gc(L); }

static lonejson_read_result
ljlua_mem_stream_read(void *user, unsigned char *buffer, size_t capacity) {
  ljlua_stream_ud *ud = (ljlua_stream_ud *)user;
  lonejson_read_result rr;
  size_t remaining;

  memset(&rr, 0, sizeof(rr));
  if (ud->owned_input_offset >= ud->owned_input_len) {
    rr.eof = 1;
    return rr;
  }
  remaining = ud->owned_input_len - ud->owned_input_offset;
  if (remaining > capacity) {
    remaining = capacity;
  }
  memcpy(buffer, ud->owned_input + ud->owned_input_offset, remaining);
  ud->owned_input_offset += remaining;
  rr.bytes_read = remaining;
  rr.eof = ud->owned_input_offset >= ud->owned_input_len;
  return rr;
}

static int ljlua_stream_next(lua_State *L) {
  ljlua_stream_ud *ud = ljlua_check_stream(L, 1);
  lonejson_error error;
  lonejson_stream_result result;

  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    if (record_ud->schema != ud->schema) {
      return luaL_error(L, "record belongs to a different schema");
    }
    result = lonejson_stream_next(ud->stream, record_ud->data, &error);
    if (result == LONEJSON_STREAM_OBJECT) {
      lua_pushvalue(L, 2);
      lua_pushnil(L);
      lua_pushstring(L, "object");
      return 3;
    }
  } else {
    unsigned char *record =
        (unsigned char *)calloc(1u, ud->schema->record_size);
    if (record == NULL) {
      return luaL_error(L, "failed to allocate decode buffer");
    }
    ljlua_prepare_record_storage(ud->schema, record);
    result = lonejson_stream_next(ud->stream, record, &error);
    if (result == LONEJSON_STREAM_OBJECT) {
      ljlua_record_to_table(L, ud->schema, record);
      lonejson_cleanup(&ud->schema->map, record);
      free(record);
      lua_pushnil(L);
      lua_pushstring(L, "object");
      return 3;
    }
    lonejson_cleanup(&ud->schema->map, record);
    free(record);
  }
  if (result == LONEJSON_STREAM_EOF) {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushstring(L, "eof");
    return 3;
  }
  if (result == LONEJSON_STREAM_WOULD_BLOCK) {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushstring(L, "would_block");
    return 3;
  }
  lua_pushnil(L);
  ljlua_push_error(L, &error);
  lua_pushstring(L, "error");
  return 3;
}

static int ljlua_spool_gc(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  if (ud->owns_data) {
    lonejson_spooled_cleanup(&ud->owned);
  }
  if (ud->owner_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->owner_ref);
    ud->owner_ref = LUA_NOREF;
  }
  return 0;
}

static int ljlua_spool_size(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  lua_pushinteger(L, (lua_Integer)lonejson_spooled_size(ud->spool));
  return 1;
}

static int ljlua_spool_spilled(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  lua_pushboolean(L, lonejson_spooled_spilled(ud->spool));
  return 1;
}

static int ljlua_spool_path(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  if (ud->spool->temp_path[0] == '\0') {
    lua_pushnil(L);
  } else {
    lua_pushstring(L, ud->spool->temp_path);
  }
  return 1;
}

static int ljlua_spool_rewind(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  lonejson_error error;
  if (lonejson_spooled_rewind(ud->spool, &error) != LONEJSON_STATUS_OK) {
    return ljlua_push_error(L, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_spool_read(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  size_t capacity = (size_t)luaL_optinteger(L, 2, 4096);
  unsigned char *buffer;
  lonejson_read_result rr;

  buffer = (unsigned char *)lua_newuserdatauv(L, capacity, 0);
  rr = lonejson_spooled_read(ud->spool, buffer, capacity);
  if (rr.error_code != 0) {
    lua_pop(L, 1);
    return luaL_error(L, "failed to read spool");
  }
  if (rr.bytes_read == 0u && rr.eof) {
    lua_pop(L, 1);
    lua_pushnil(L);
    return 1;
  }
  lua_pushlstring(L, (const char *)buffer, rr.bytes_read);
  return 1;
}

static int ljlua_spool_read_all(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  luaL_Buffer buf;
  unsigned char chunk[4096];
  lonejson_error error;

  if (lonejson_spooled_rewind(ud->spool, &error) != LONEJSON_STATUS_OK) {
    return ljlua_push_error(L, &error);
  }
  luaL_buffinit(L, &buf);
  for (;;) {
    lonejson_read_result rr =
        lonejson_spooled_read(ud->spool, chunk, sizeof(chunk));
    if (rr.error_code != 0) {
      return luaL_error(L, "failed to read spool");
    }
    if (rr.bytes_read != 0u) {
      luaL_addlstring(&buf, (const char *)chunk, rr.bytes_read);
    }
    if (rr.eof) {
      break;
    }
  }
  luaL_pushresult(&buf);
  return 1;
}

static int ljlua_spool_write_to(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  size_t capacity = (size_t)luaL_optinteger(L, 3, 4096);
  unsigned char *buffer;
  lonejson_error error;

  luaL_checktype(L, 2, LUA_TFUNCTION);
  buffer = (unsigned char *)malloc(capacity);
  if (buffer == NULL) {
    return luaL_error(L, "failed to allocate write buffer");
  }
  if (lonejson_spooled_rewind(ud->spool, &error) != LONEJSON_STATUS_OK) {
    free(buffer);
    return ljlua_push_error(L, &error);
  }
  for (;;) {
    lonejson_read_result rr =
        lonejson_spooled_read(ud->spool, buffer, capacity);
    if (rr.error_code != 0) {
      free(buffer);
      return luaL_error(L, "failed to read spool");
    }
    if (rr.bytes_read != 0u) {
      lua_pushvalue(L, 2);
      lua_pushlstring(L, (const char *)buffer, rr.bytes_read);
      lua_call(L, 1, 0);
    }
    if (rr.eof) {
      break;
    }
  }
  free(buffer);
  lua_pushboolean(L, 1);
  return 1;
}

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
    {NULL, NULL}};

static const luaL_Reg ljlua_record_methods[] = {
    {"get", ljlua_record_get},
    {"to_table", ljlua_record_to_table_method},
    {"count", ljlua_record_count},
    {"clear", ljlua_record_clear},
    {NULL, NULL}};

static const luaL_Reg ljlua_stream_methods[] = {
    {"next", ljlua_stream_next}, {"close", ljlua_stream_close}, {NULL, NULL}};

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
                                   {"monotonic_ns", ljlua_monotonic_ns},
                                   {NULL, NULL}};

  luaL_checkversion(L);
  ljlua_register_metatable(L, LJLUA_SCHEMA_MT, ljlua_schema_methods,
                           ljlua_schema_gc, NULL);
  ljlua_register_metatable(L, LJLUA_RECORD_MT, ljlua_record_methods,
                           ljlua_record_gc, ljlua_record_index);
  ljlua_register_metatable(L, LJLUA_STREAM_MT, ljlua_stream_methods,
                           ljlua_stream_gc, NULL);
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
