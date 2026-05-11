#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../lonejson_internal.h"

#include <lauxlib.h>
#include <lua.h>

#define LJLUA_SCHEMA_MT "lonejson.schema"
#define LJLUA_RECORD_MT "lonejson.record"
#define LJLUA_STREAM_MT "lonejson.stream"
#define LJLUA_ARRAY_STREAM_MT "lonejson.array_stream"
#define LJLUA_SPOOL_MT "lonejson.spool"
#define LJLUA_PATH_MT "lonejson.path"
#define LJLUA_SCHEMA_MAGIC 0x4c4a5343u
#define LJLUA_RECORD_MAGIC 0x4c4a5243u
#define LJLUA_STREAM_MAGIC 0x4c4a5354u
#define LJLUA_ARRAY_STREAM_MAGIC 0x4c4a4153u
#define LJLUA_SPOOL_MAGIC 0x4c4a5350u
#define LJLUA_PATH_MAGIC 0x4c4a5048u

typedef enum ljlua_field_kind {
  LJLUA_FIELD_STRING = 1,
  LJLUA_FIELD_SPOOLED_TEXT,
  LJLUA_FIELD_SPOOLED_BYTES,
  LJLUA_FIELD_JSON_VALUE,
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
typedef struct ljlua_json_value_decode_state ljlua_json_value_decode_state;

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
  int has_json_value;
  lonejson_field *fields;
  ljlua_field_meta *metas;
  ljlua_compiled_path *path_cache;
  unsigned char *scratch_record;
  char *encode_buffer;
  size_t encode_capacity;
  int scratch_in_use;
};

typedef struct ljlua_schema_ud {
  unsigned int magic;
  ljlua_schema *schema;
} ljlua_schema_ud;

typedef struct ljlua_record_ud {
  unsigned int magic;
  ljlua_schema *schema;
  int schema_ref;
  int cleared;
  unsigned char data[1];
} ljlua_record_ud;

typedef struct ljlua_stream_ud {
  unsigned int magic;
  ljlua_schema *schema;
  int schema_ref;
  lonejson_stream *stream;
  int clear_destination;
  unsigned char *owned_input;
  size_t owned_input_len;
  size_t owned_input_offset;
} ljlua_stream_ud;

typedef struct ljlua_decode_context {
  lua_State *L;
  ljlua_schema *schema;
  ljlua_json_value_decode_state *json_values;
} ljlua_decode_context;

typedef struct ljlua_array_stream_ud {
  unsigned int magic;
  ljlua_schema *schema;
  int schema_ref;
  lonejson_array_stream *stream;
  int clear_destination;
  unsigned char *pending_record;
  ljlua_decode_context pending_ctx;
  unsigned char *owned_input;
  size_t owned_input_len;
  size_t owned_input_offset;
} ljlua_array_stream_ud;

typedef struct ljlua_spool_ud {
  unsigned int magic;
  lonejson_spooled *spool;
  lonejson_spooled owned;
  int owns_data;
  int owner_ref;
  int kind;
} ljlua_spool_ud;

typedef struct ljlua_path_ud {
  unsigned int magic;
  ljlua_schema *schema;
  ljlua_compiled_path *compiled;
  int schema_ref;
} ljlua_path_ud;

typedef struct ljlua_json_buf {
  char *data;
  size_t len;
  size_t cap;
} ljlua_json_buf;

typedef struct ljlua_json_key {
  char *text;
  size_t len;
} ljlua_json_key;

typedef struct ljlua_json_parser {
  const unsigned char *data;
  size_t len;
  size_t off;
} ljlua_json_parser;

static char ljlua_json_null_sentinel;

typedef enum ljlua_json_builder_token_mode {
  LJLUA_JSON_TOKEN_NONE = 0,
  LJLUA_JSON_TOKEN_STRING,
  LJLUA_JSON_TOKEN_NUMBER,
  LJLUA_JSON_TOKEN_TRUE,
  LJLUA_JSON_TOKEN_FALSE,
  LJLUA_JSON_TOKEN_NULL
} ljlua_json_builder_token_mode;

typedef enum ljlua_json_builder_frame_kind {
  LJLUA_JSON_FRAME_OBJECT = 1,
  LJLUA_JSON_FRAME_ARRAY = 2
} ljlua_json_builder_frame_kind;

typedef enum ljlua_json_builder_frame_state {
  LJLUA_JSON_OBJECT_KEY_OR_END = 1,
  LJLUA_JSON_OBJECT_COLON,
  LJLUA_JSON_OBJECT_VALUE,
  LJLUA_JSON_OBJECT_COMMA_OR_END,
  LJLUA_JSON_ARRAY_VALUE_OR_END,
  LJLUA_JSON_ARRAY_COMMA_OR_END
} ljlua_json_builder_frame_state;

typedef struct ljlua_json_builder_frame {
  ljlua_json_builder_frame_kind kind;
  ljlua_json_builder_frame_state state;
  int table_ref;
  size_t array_index;
  char *pending_key;
} ljlua_json_builder_frame;

typedef struct ljlua_json_builder {
  lua_State *L;
  ljlua_json_buf token;
  ljlua_json_builder_frame *frames;
  size_t frame_count;
  size_t frame_cap;
  ljlua_json_builder_token_mode token_mode;
  int token_is_key;
  int token_escape;
  unsigned unicode_digits_needed;
  unsigned unicode_accum;
  unsigned unicode_pending_high;
  int root_ref;
  int finished;
} ljlua_json_builder;

struct ljlua_json_value_decode_state {
  ljlua_json_builder builder;
  lonejson_value_visitor visitor;
  int active;
  int complete;
};

static int ljlua_record_to_table_method(lua_State *L);
static int ljlua_record_get(lua_State *L);
static int ljlua_record_count(lua_State *L);
static int ljlua_record_clear(lua_State *L);
static int ljlua_path_get(lua_State *L);
static int ljlua_path_count(lua_State *L);
static int ljlua_array_stream_close(lua_State *L);
static int ljlua_getter_call(lua_State *L);
static int ljlua_counter_call(lua_State *L);
static void ljlua_cleanup_record_storage(ljlua_schema *schema, void *record);
static int
ljlua_record_to_table_with_overrides(lua_State *L, ljlua_schema *schema,
                                     void *record,
                                     const ljlua_decode_context *ctx);

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

static void *ljlua_xrealloc(void *ptr, size_t size) {
  void *next = realloc(ptr, size);
  if (next == NULL) {
    abort();
  }
  return next;
}

static int ljlua_json_buf_reserve(ljlua_json_buf *buf, size_t extra) {
  size_t needed = buf->len + extra + 1u;
  size_t cap;

  if (needed <= buf->cap) {
    return 1;
  }
  cap = buf->cap == 0u ? 128u : buf->cap;
  while (cap < needed) {
    cap *= 2u;
  }
  buf->data = (char *)ljlua_xrealloc(buf->data, cap);
  buf->cap = cap;
  return 1;
}

static int ljlua_json_buf_append(ljlua_json_buf *buf, const void *data,
                                 size_t len) {
  if (!ljlua_json_buf_reserve(buf, len)) {
    return 0;
  }
  memcpy(buf->data + buf->len, data, len);
  buf->len += len;
  buf->data[buf->len] = '\0';
  return 1;
}

static int ljlua_json_buf_append_cstr(ljlua_json_buf *buf, const char *text) {
  return ljlua_json_buf_append(buf, text, strlen(text));
}

static lonejson_status ljlua_json_buf_sink(void *user, const void *data,
                                           size_t len, lonejson_error *error) {
  (void)error;
  return ljlua_json_buf_append((ljlua_json_buf *)user, data, len)
             ? LONEJSON_STATUS_OK
             : LONEJSON_STATUS_ALLOCATION_FAILED;
}

static void ljlua_json_mark_shape(lua_State *L, int index, const char *kind) {
  index = lua_absindex(L, index);
  lua_createtable(L, 0, 1);
  lua_pushstring(L, kind);
  lua_setfield(L, -2, "__lonejson_json_kind");
  lua_setmetatable(L, index);
}

static const char *ljlua_json_shape(lua_State *L, int index) {
  const char *shape = NULL;

  index = lua_absindex(L, index);
  if (!lua_getmetatable(L, index)) {
    return NULL;
  }
  lua_getfield(L, -1, "__lonejson_json_kind");
  if (lua_isstring(L, -1)) {
    shape = lua_tostring(L, -1);
  }
  lua_pop(L, 2);
  return shape;
}

static void ljlua_push_json_null(lua_State *L) {
  lua_pushlightuserdata(L, (void *)&ljlua_json_null_sentinel);
}

static int ljlua_is_json_null(lua_State *L, int index) {
  index = lua_absindex(L, index);
  return lua_type(L, index) == LUA_TLIGHTUSERDATA &&
         lua_touserdata(L, index) == (void *)&ljlua_json_null_sentinel;
}

static int ljlua_json_key_compare(const void *lhs, const void *rhs) {
  const ljlua_json_key *a = (const ljlua_json_key *)lhs;
  const ljlua_json_key *b = (const ljlua_json_key *)rhs;
  size_t shared = a->len < b->len ? a->len : b->len;
  int cmp = memcmp(a->text, b->text, shared);

  if (cmp != 0) {
    return cmp;
  }
  if (a->len < b->len) {
    return -1;
  }
  if (a->len > b->len) {
    return 1;
  }
  return 0;
}

static int ljlua_json_buffer_add_utf8(luaL_Buffer *buffer,
                                      lonejson_uint32 codepoint) {
  char out[4];
  size_t len;

  if (codepoint <= 0x7Fu) {
    out[0] = (char)codepoint;
    len = 1u;
  } else if (codepoint <= 0x7FFu) {
    out[0] = (char)(0xC0u | ((codepoint >> 6u) & 0x1Fu));
    out[1] = (char)(0x80u | (codepoint & 0x3Fu));
    len = 2u;
  } else if (codepoint <= 0xFFFFu) {
    out[0] = (char)(0xE0u | ((codepoint >> 12u) & 0x0Fu));
    out[1] = (char)(0x80u | ((codepoint >> 6u) & 0x3Fu));
    out[2] = (char)(0x80u | (codepoint & 0x3Fu));
    len = 3u;
  } else if (codepoint <= 0x10FFFFu) {
    out[0] = (char)(0xF0u | ((codepoint >> 18u) & 0x07u));
    out[1] = (char)(0x80u | ((codepoint >> 12u) & 0x3Fu));
    out[2] = (char)(0x80u | ((codepoint >> 6u) & 0x3Fu));
    out[3] = (char)(0x80u | (codepoint & 0x3Fu));
    len = 4u;
  } else {
    return 0;
  }
  luaL_addlstring(buffer, out, len);
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

static int ljlua_json_parse_value(lua_State *L, ljlua_json_parser *parser,
                                  size_t depth);

static void ljlua_json_skip_space(ljlua_json_parser *parser) {
  while (parser->off < parser->len &&
         lonejson__is_json_space(parser->data[parser->off])) {
    parser->off++;
  }
}

static int ljlua_json_parse_string(lua_State *L, ljlua_json_parser *parser) {
  luaL_Buffer buffer;

  luaL_buffinit(L, &buffer);
  while (parser->off < parser->len) {
    unsigned char ch = parser->data[parser->off++];
    if (ch == '"') {
      luaL_pushresult(&buffer);
      return 1;
    }
    if (ch == '\\') {
      if (parser->off >= parser->len) {
        return luaL_error(L, "invalid JSON string escape");
      }
      ch = parser->data[parser->off++];
      switch (ch) {
      case '"':
      case '\\':
      case '/':
        luaL_addchar(&buffer, (char)ch);
        break;
      case 'b':
        luaL_addchar(&buffer, '\b');
        break;
      case 'f':
        luaL_addchar(&buffer, '\f');
        break;
      case 'n':
        luaL_addchar(&buffer, '\n');
        break;
      case 'r':
        luaL_addchar(&buffer, '\r');
        break;
      case 't':
        luaL_addchar(&buffer, '\t');
        break;
      case 'u': {
        lonejson_uint32 codepoint;

        if (parser->off + 4u > parser->len ||
            !lonejson__decode_unicode_quad(parser->data + parser->off,
                                           &codepoint)) {
          return luaL_error(L, "invalid JSON unicode escape");
        }
        parser->off += 4u;
        if (codepoint >= 0xD800u && codepoint <= 0xDBFFu) {
          lonejson_uint32 low;

          if (parser->off + 6u > parser->len ||
              parser->data[parser->off] != '\\' ||
              parser->data[parser->off + 1u] != 'u' ||
              !lonejson__decode_unicode_quad(parser->data + parser->off + 2u,
                                             &low) ||
              low < 0xDC00u || low > 0xDFFFu) {
            return luaL_error(L, "invalid JSON unicode surrogate pair");
          }
          parser->off += 6u;
          codepoint =
              0x10000u + (((codepoint - 0xD800u) << 10u) | (low - 0xDC00u));
        } else if (codepoint >= 0xDC00u && codepoint <= 0xDFFFu) {
          return luaL_error(L, "invalid JSON unicode surrogate pair");
        }
        if (!ljlua_json_buffer_add_utf8(&buffer, codepoint)) {
          return luaL_error(L, "failed to encode JSON unicode codepoint");
        }
        break;
      }
      default:
        return luaL_error(L, "invalid JSON string escape");
      }
      continue;
    }
    if (ch < 0x20u) {
      return luaL_error(L, "control character in JSON string");
    }
    luaL_addchar(&buffer, (char)ch);
  }
  return luaL_error(L, "unterminated JSON string");
}

static int ljlua_json_parse_number(lua_State *L, ljlua_json_parser *parser) {
  size_t start = parser->off;
  size_t len;
  char stack_buf[128];
  char *text;
  int integer_like = 1;
  size_t i;
  char *end = NULL;
  double number;

  while (parser->off < parser->len) {
    unsigned char ch = parser->data[parser->off];
    if (!(lonejson__is_digit(ch) || ch == '-' || ch == '+' || ch == '.' ||
          ch == 'e' || ch == 'E')) {
      break;
    }
    parser->off++;
  }
  len = parser->off - start;
  text = (len + 1u <= sizeof(stack_buf)) ? stack_buf
                                         : (char *)ljlua_xmalloc(len + 1u);
  memcpy(text, parser->data + start, len);
  text[len] = '\0';
  for (i = 0u; i < len; ++i) {
    if (text[i] == '.' || text[i] == 'e' || text[i] == 'E') {
      integer_like = 0;
      break;
    }
  }
  if (integer_like) {
    lonejson_int64 i64;

    if (lonejson__parse_i64_token(text, &i64)) {
      lua_pushinteger(L, (lua_Integer)i64);
      if (text != stack_buf) {
        free(text);
      }
      return 1;
    }
  }
  errno = 0;
  number = strtod(text, &end);
  if (errno != 0 || end == text || *end != '\0' ||
      !lonejson__is_finite_f64(number)) {
    if (text != stack_buf) {
      free(text);
    }
    return luaL_error(L, "invalid JSON number");
  }
  lua_pushnumber(L, (lua_Number)number);
  if (text != stack_buf) {
    free(text);
  }
  return 1;
}

static int ljlua_json_parse_array(lua_State *L, ljlua_json_parser *parser,
                                  size_t depth) {
  size_t index = 1u;

  lua_createtable(L, 0, 0);
  ljlua_json_skip_space(parser);
  if (parser->off < parser->len && parser->data[parser->off] == ']') {
    parser->off++;
    ljlua_json_mark_shape(L, -1, "array");
    return 1;
  }
  for (;;) {
    ljlua_json_parse_value(L, parser, depth + 1u);
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      ljlua_push_json_null(L);
    }
    lua_rawseti(L, -2, (lua_Integer)index++);
    ljlua_json_skip_space(parser);
    if (parser->off >= parser->len) {
      return luaL_error(L, "unterminated JSON array");
    }
    if (parser->data[parser->off] == ']') {
      parser->off++;
      ljlua_json_mark_shape(L, -1, "array");
      return 1;
    }
    if (parser->data[parser->off] != ',') {
      return luaL_error(L, "expected ',' in JSON array");
    }
    parser->off++;
    ljlua_json_skip_space(parser);
  }
}

static int ljlua_json_parse_object(lua_State *L, ljlua_json_parser *parser,
                                   size_t depth) {
  lua_createtable(L, 0, 0);
  ljlua_json_skip_space(parser);
  if (parser->off < parser->len && parser->data[parser->off] == '}') {
    parser->off++;
    ljlua_json_mark_shape(L, -1, "object");
    return 1;
  }
  for (;;) {
    if (parser->off >= parser->len || parser->data[parser->off] != '"') {
      return luaL_error(L, "expected JSON object key");
    }
    parser->off++;
    ljlua_json_parse_string(L, parser);
    ljlua_json_skip_space(parser);
    if (parser->off >= parser->len || parser->data[parser->off] != ':') {
      return luaL_error(L, "expected ':' after JSON object key");
    }
    parser->off++;
    ljlua_json_skip_space(parser);
    ljlua_json_parse_value(L, parser, depth + 1u);
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      ljlua_push_json_null(L);
    }
    lua_settable(L, -3);
    ljlua_json_skip_space(parser);
    if (parser->off >= parser->len) {
      return luaL_error(L, "unterminated JSON object");
    }
    if (parser->data[parser->off] == '}') {
      parser->off++;
      ljlua_json_mark_shape(L, -1, "object");
      return 1;
    }
    if (parser->data[parser->off] != ',') {
      return luaL_error(L, "expected ',' in JSON object");
    }
    parser->off++;
    ljlua_json_skip_space(parser);
  }
}

static int ljlua_json_parse_value(lua_State *L, ljlua_json_parser *parser,
                                  size_t depth) {
  if (depth > 128u) {
    return luaL_error(L, "JSON value nesting exceeds Lua binding limit");
  }
  ljlua_json_skip_space(parser);
  if (parser->off >= parser->len) {
    return luaL_error(L, "expected JSON value");
  }
  switch (parser->data[parser->off++]) {
  case '"':
    return ljlua_json_parse_string(L, parser);
  case '{':
    return ljlua_json_parse_object(L, parser, depth);
  case '[':
    return ljlua_json_parse_array(L, parser, depth);
  case 't':
    if (parser->off + 2u > parser->len ||
        memcmp(parser->data + parser->off, "rue", 3u) != 0) {
      return luaL_error(L, "invalid JSON literal");
    }
    parser->off += 3u;
    lua_pushboolean(L, 1);
    return 1;
  case 'f':
    if (parser->off + 3u > parser->len ||
        memcmp(parser->data + parser->off, "alse", 4u) != 0) {
      return luaL_error(L, "invalid JSON literal");
    }
    parser->off += 4u;
    lua_pushboolean(L, 0);
    return 1;
  case 'n':
    if (parser->off + 2u > parser->len ||
        memcmp(parser->data + parser->off, "ull", 3u) != 0) {
      return luaL_error(L, "invalid JSON literal");
    }
    parser->off += 3u;
    lua_pushnil(L);
    return 1;
  default:
    parser->off--;
    return ljlua_json_parse_number(L, parser);
  }
}

static int ljlua_push_json_from_text(lua_State *L, const char *json,
                                     size_t len) {
  ljlua_json_parser parser;

  parser.data = (const unsigned char *)json;
  parser.len = len;
  parser.off = 0u;
  ljlua_json_parse_value(L, &parser, 0u);
  ljlua_json_skip_space(&parser);
  if (parser.off != parser.len) {
    return luaL_error(L, "trailing data after JSON value");
  }
  return 1;
}

static void ljlua_json_builder_cleanup(lua_State *L,
                                       ljlua_json_builder *builder) {
  size_t i;

  if (builder == NULL) {
    return;
  }
  if (L != NULL) {
    if (builder->root_ref != LUA_NOREF && builder->root_ref != LUA_REFNIL) {
      luaL_unref(L, LUA_REGISTRYINDEX, builder->root_ref);
    }
    for (i = 0u; i < builder->frame_count; ++i) {
      if (builder->frames[i].table_ref != LUA_NOREF &&
          builder->frames[i].table_ref != LUA_REFNIL) {
        luaL_unref(L, LUA_REGISTRYINDEX, builder->frames[i].table_ref);
      }
      free(builder->frames[i].pending_key);
      builder->frames[i].pending_key = NULL;
    }
  } else {
    for (i = 0u; i < builder->frame_count; ++i) {
      free(builder->frames[i].pending_key);
      builder->frames[i].pending_key = NULL;
    }
  }
  free(builder->frames);
  free(builder->token.data);
  memset(builder, 0, sizeof(*builder));
  builder->root_ref = LUA_NOREF;
}

static void ljlua_json_builder_init(ljlua_json_builder *builder, lua_State *L) {
  memset(builder, 0, sizeof(*builder));
  builder->L = L;
  builder->root_ref = LUA_NOREF;
}

static int ljlua_json_builder_push_frame(ljlua_json_builder *builder,
                                         ljlua_json_builder_frame_kind kind,
                                         int table_ref) {
  if (builder->frame_count == builder->frame_cap) {
    size_t next_cap = builder->frame_cap == 0u ? 4u : builder->frame_cap * 2u;
    void *next = realloc(builder->frames, next_cap * sizeof(*builder->frames));
    if (next == NULL) {
      return 0;
    }
    builder->frames = (ljlua_json_builder_frame *)next;
    builder->frame_cap = next_cap;
  }
  memset(&builder->frames[builder->frame_count], 0,
         sizeof(builder->frames[builder->frame_count]));
  builder->frames[builder->frame_count].kind = kind;
  builder->frames[builder->frame_count].table_ref = table_ref;
  builder->frames[builder->frame_count].array_index = 1u;
  builder->frames[builder->frame_count].state =
      (kind == LJLUA_JSON_FRAME_OBJECT) ? LJLUA_JSON_OBJECT_KEY_OR_END
                                        : LJLUA_JSON_ARRAY_VALUE_OR_END;
  builder->frames[builder->frame_count].pending_key = NULL;
  builder->frame_count++;
  return 1;
}

static ljlua_json_builder_frame *
ljlua_json_builder_top(ljlua_json_builder *builder) {
  if (builder->frame_count == 0u) {
    return NULL;
  }
  return &builder->frames[builder->frame_count - 1u];
}

static int ljlua_json_builder_attach_value(lua_State *L,
                                           ljlua_json_builder *builder) {
  ljlua_json_builder_frame *frame = ljlua_json_builder_top(builder);

  if (frame == NULL) {
    if (builder->root_ref != LUA_NOREF) {
      lua_pop(L, 1);
      return luaL_error(L, "multiple JSON root values in Lua builder");
    }
    builder->root_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    builder->finished = 1;
    return 1;
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, frame->table_ref);
  lua_insert(L, -2);
  if (frame->kind == LJLUA_JSON_FRAME_ARRAY) {
    lua_rawseti(L, -2, (lua_Integer)frame->array_index++);
    frame->state = LJLUA_JSON_ARRAY_COMMA_OR_END;
  } else {
    if (frame->pending_key == NULL) {
      lua_pop(L, 1);
      lua_pop(L, 1);
      return luaL_error(L, "object value missing key in Lua builder");
    }
    lua_setfield(L, -2, frame->pending_key);
    free(frame->pending_key);
    frame->pending_key = NULL;
    frame->state = LJLUA_JSON_OBJECT_COMMA_OR_END;
  }
  lua_pop(L, 1);
  return 1;
}

static int ljlua_json_builder_finish_string(lua_State *L,
                                            ljlua_json_builder *builder) {
  ljlua_json_builder_frame *frame = ljlua_json_builder_top(builder);
  int was_key = builder->token_is_key;

  builder->token_mode = LJLUA_JSON_TOKEN_NONE;
  builder->token_escape = 0;
  builder->unicode_digits_needed = 0u;
  builder->unicode_accum = 0u;
  builder->unicode_pending_high = 0u;
  if (was_key) {
    char *copy = (char *)malloc(builder->token.len + 1u);
    if (copy == NULL) {
      return luaL_error(L, "failed to allocate JSON object key");
    }
    memcpy(copy, builder->token.data ? builder->token.data : "",
           builder->token.len);
    copy[builder->token.len] = '\0';
    builder->token.len = 0u;
    builder->token_is_key = 0;
    if (frame == NULL || frame->kind != LJLUA_JSON_FRAME_OBJECT ||
        frame->state != LJLUA_JSON_OBJECT_KEY_OR_END) {
      free(copy);
      return luaL_error(L, "object key outside object context");
    }
    free(frame->pending_key);
    frame->pending_key = copy;
    frame->state = LJLUA_JSON_OBJECT_COLON;
    return 1;
  }
  lua_pushlstring(L, builder->token.data ? builder->token.data : "",
                  builder->token.len);
  builder->token.len = 0u;
  builder->token_is_key = 0;
  return ljlua_json_builder_attach_value(L, builder);
}

static int ljlua_json_builder_close_container(lua_State *L,
                                              ljlua_json_builder *builder) {
  ljlua_json_builder_frame frame;

  if (builder->frame_count == 0u) {
    return luaL_error(L, "unexpected JSON container terminator");
  }
  frame = builder->frames[builder->frame_count - 1u];
  builder->frame_count--;
  lua_rawgeti(L, LUA_REGISTRYINDEX, frame.table_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, frame.table_ref);
  free(frame.pending_key);
  return ljlua_json_builder_attach_value(L, builder);
}

static lonejson_status
ljlua_json_builder_callback_failed(lua_State *L, lonejson_error *error) {
  const char *msg = lua_tostring(L, -1);
  lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                      msg ? msg : "Lua JSON visitor callback failed");
  lua_pop(L, 1);
  return LONEJSON_STATUS_CALLBACK_FAILED;
}

static int ljlua_json_builder_begin_token(ljlua_json_builder *builder,
                                          ljlua_json_builder_token_mode mode,
                                          int is_key) {
  builder->token_mode = mode;
  builder->token_is_key = is_key;
  builder->token.len = 0u;
  return 1;
}

static int ljlua_json_builder_prepare_key(ljlua_json_builder *builder) {
  ljlua_json_builder_frame *frame = ljlua_json_builder_top(builder);

  if (frame != NULL && frame->kind == LJLUA_JSON_FRAME_OBJECT &&
      frame->state == LJLUA_JSON_OBJECT_COMMA_OR_END) {
    frame->state = LJLUA_JSON_OBJECT_KEY_OR_END;
  }
  return 1;
}

static int ljlua_json_builder_prepare_value(ljlua_json_builder *builder) {
  ljlua_json_builder_frame *frame = ljlua_json_builder_top(builder);

  if (frame == NULL) {
    return 1;
  }
  if (frame->kind == LJLUA_JSON_FRAME_OBJECT &&
      frame->state == LJLUA_JSON_OBJECT_COLON) {
    frame->state = LJLUA_JSON_OBJECT_VALUE;
  } else if (frame->kind == LJLUA_JSON_FRAME_ARRAY &&
             frame->state == LJLUA_JSON_ARRAY_COMMA_OR_END) {
    frame->state = LJLUA_JSON_ARRAY_VALUE_OR_END;
  }
  return 1;
}

static int ljlua_json_builder_append_chunk(ljlua_json_builder *builder,
                                           const char *data, size_t len) {
  return ljlua_json_buf_append(&builder->token, data, len);
}

static int
ljlua_json_builder_push_container(lua_State *L, ljlua_json_builder *builder,
                                  ljlua_json_builder_frame_kind kind) {
  ljlua_json_builder_prepare_value(builder);
  lua_createtable(L, kind == LJLUA_JSON_FRAME_ARRAY ? 4 : 0,
                  kind == LJLUA_JSON_FRAME_OBJECT ? 4 : 0);
  ljlua_json_mark_shape(L, -1,
                        kind == LJLUA_JSON_FRAME_OBJECT ? "object" : "array");
  if (!ljlua_json_builder_push_frame(builder, kind,
                                     luaL_ref(L, LUA_REGISTRYINDEX))) {
    lua_pop(L, 1);
    return luaL_error(L, "failed to grow Lua JSON builder stack");
  }
  return 1;
}

static int ljlua_json_builder_push_boolean(lua_State *L,
                                           ljlua_json_builder *builder,
                                           int boolean_value) {
  lua_pushboolean(L, boolean_value);
  return ljlua_json_builder_attach_value(L, builder);
}

static int ljlua_json_builder_push_null(lua_State *L,
                                        ljlua_json_builder *builder) {
  if (ljlua_json_builder_top(builder) == NULL) {
    lua_pushnil(L);
  } else {
    ljlua_push_json_null(L);
  }
  return ljlua_json_builder_attach_value(L, builder);
}

static int ljlua_json_builder_finish_number(lua_State *L,
                                            ljlua_json_builder *builder) {
  builder->token_mode = LJLUA_JSON_TOKEN_NONE;
  ljlua_push_json_from_text(L, builder->token.data ? builder->token.data : "",
                            builder->token.len);
  builder->token.len = 0u;
  return ljlua_json_builder_attach_value(L, builder);
}

static lonejson_status ljlua_json_value_object_begin(void *user,
                                                     lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "Lua JSON builder state is required");
  }
  if (ljlua_json_builder_push_container(state->builder.L, &state->builder,
                                        LJLUA_JSON_FRAME_OBJECT) != 1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_object_end(void *user,
                                                   lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "Lua JSON builder state is required");
  }
  if (ljlua_json_builder_close_container(state->builder.L, &state->builder) !=
      1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  state->complete = state->builder.finished;
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_array_begin(void *user,
                                                    lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "Lua JSON builder state is required");
  }
  if (ljlua_json_builder_push_container(state->builder.L, &state->builder,
                                        LJLUA_JSON_FRAME_ARRAY) != 1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_array_end(void *user,
                                                  lonejson_error *error) {
  return ljlua_json_value_object_end(user, error);
}

static lonejson_status ljlua_json_value_key_begin(void *user,
                                                  lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "Lua JSON builder state is required");
  }
  ljlua_json_builder_prepare_key(&state->builder);
  ljlua_json_builder_begin_token(&state->builder, LJLUA_JSON_TOKEN_STRING, 1);
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_key_chunk(void *user, const char *data,
                                                  size_t len,
                                                  lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "Lua JSON builder state is required");
  }
  if (!ljlua_json_builder_append_chunk(&state->builder, data, len)) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to grow Lua JSON key buffer");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_key_end(void *user,
                                                lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "Lua JSON builder state is required");
  }
  if (ljlua_json_builder_finish_string(state->builder.L, &state->builder) !=
      1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_string_begin(void *user,
                                                     lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "Lua JSON builder state is required");
  }
  ljlua_json_builder_prepare_value(&state->builder);
  ljlua_json_builder_begin_token(&state->builder, LJLUA_JSON_TOKEN_STRING, 0);
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_string_chunk(void *user,
                                                     const char *data,
                                                     size_t len,
                                                     lonejson_error *error) {
  return ljlua_json_value_key_chunk(user, data, len, error);
}

static lonejson_status ljlua_json_value_string_end(void *user,
                                                   lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "Lua JSON builder state is required");
  }
  state->builder.token_is_key = 0;
  if (ljlua_json_builder_finish_string(state->builder.L, &state->builder) !=
      1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  state->complete = state->builder.finished;
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_number_begin(void *user,
                                                     lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "Lua JSON builder state is required");
  }
  ljlua_json_builder_prepare_value(&state->builder);
  ljlua_json_builder_begin_token(&state->builder, LJLUA_JSON_TOKEN_NUMBER, 0);
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_number_chunk(void *user,
                                                     const char *data,
                                                     size_t len,
                                                     lonejson_error *error) {
  return ljlua_json_value_key_chunk(user, data, len, error);
}

static lonejson_status ljlua_json_value_number_end(void *user,
                                                   lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "Lua JSON builder state is required");
  }
  if (ljlua_json_builder_finish_number(state->builder.L, &state->builder) !=
      1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  state->complete = state->builder.finished;
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_boolean(void *user, int boolean_value,
                                                lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "Lua JSON builder state is required");
  }
  ljlua_json_builder_prepare_value(&state->builder);
  if (ljlua_json_builder_push_boolean(state->builder.L, &state->builder,
                                      boolean_value) != 1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  state->complete = state->builder.finished;
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_json_value_null(void *user,
                                             lonejson_error *error) {
  ljlua_json_value_decode_state *state = (ljlua_json_value_decode_state *)user;
  if (state == NULL || state->builder.L == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "Lua JSON builder state is required");
  }
  ljlua_json_builder_prepare_value(&state->builder);
  if (ljlua_json_builder_push_null(state->builder.L, &state->builder) != 1) {
    return ljlua_json_builder_callback_failed(state->builder.L, error);
  }
  state->complete = state->builder.finished;
  return LONEJSON_STATUS_OK;
}

static int ljlua_json_table_is_array(lua_State *L, int index) {
  size_t count = 0u;
  size_t n;

  index = lua_absindex(L, index);
  n = (size_t)lua_rawlen(L, index);
  lua_pushnil(L);
  while (lua_next(L, index) != 0) {
    if (!lua_isinteger(L, -2)) {
      lua_pop(L, 2);
      return 0;
    }
    {
      lua_Integer key = lua_tointeger(L, -2);
      if (key <= 0 || (size_t)key > n) {
        lua_pop(L, 2);
        return 0;
      }
    }
    count++;
    lua_pop(L, 1);
  }
  return count != 0u && count == n;
}

static int ljlua_encode_json_value(lua_State *L, int index, ljlua_json_buf *buf,
                                   const void **visited, size_t depth);

static int ljlua_encode_json_string(lua_State *L, int index,
                                    ljlua_json_buf *buf) {
  size_t len;
  const char *text = luaL_checklstring(L, index, &len);
  lonejson_error error;
  lonejson_status status;

  if (!ljlua_json_buf_append(buf, "\"", 1u)) {
    return luaL_error(L, "failed to grow JSON buffer");
  }
  status = lonejson__emit_escaped_fragment(ljlua_json_buf_sink, buf, &error,
                                           (const unsigned char *)text, len);
  if (status != LONEJSON_STATUS_OK) {
    return luaL_error(L, "failed to encode JSON string: %s", error.message);
  }
  if (!ljlua_json_buf_append(buf, "\"", 1u)) {
    return luaL_error(L, "failed to grow JSON buffer");
  }
  return 1;
}

static int ljlua_encode_json_table(lua_State *L, int index, ljlua_json_buf *buf,
                                   const void **visited, size_t depth) {
  const char *shape;
  size_t i;

  index = lua_absindex(L, index);
  shape = ljlua_json_shape(L, index);
  if (shape != NULL && strcmp(shape, "array") == 0) {
    size_t n = (size_t)lua_rawlen(L, index);
    ljlua_json_buf_append(buf, "[", 1u);
    for (i = 1u; i <= n; ++i) {
      if (i != 1u) {
        ljlua_json_buf_append(buf, ",", 1u);
      }
      lua_rawgeti(L, index, (lua_Integer)i);
      ljlua_encode_json_value(L, lua_gettop(L), buf, visited, depth + 1u);
      lua_pop(L, 1);
    }
    ljlua_json_buf_append(buf, "]", 1u);
    return 1;
  }
  if ((shape != NULL && strcmp(shape, "object") == 0) ||
      !ljlua_json_table_is_array(L, index)) {
    ljlua_json_key *keys = NULL;
    size_t count = 0u;

    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
      size_t len;
      const char *key;

      if (!lua_isstring(L, -2)) {
        free(keys);
        lua_pop(L, 2);
        return luaL_error(L, "JSON object keys must be strings");
      }
      key = lua_tolstring(L, -2, &len);
      keys =
          (ljlua_json_key *)ljlua_xrealloc(keys, sizeof(*keys) * (count + 1u));
      keys[count].text = (char *)ljlua_xmalloc(len + 1u);
      memcpy(keys[count].text, key, len);
      keys[count].text[len] = '\0';
      keys[count].len = len;
      count++;
      lua_pop(L, 1);
    }
    qsort(keys, count, sizeof(*keys), ljlua_json_key_compare);
    ljlua_json_buf_append(buf, "{", 1u);
    for (i = 0u; i < count; ++i) {
      lonejson_error error;
      lonejson_status status;

      if (i != 0u) {
        ljlua_json_buf_append(buf, ",", 1u);
      }
      if (!ljlua_json_buf_append(buf, "\"", 1u)) {
        free(keys[i].text);
        free(keys);
        return luaL_error(L, "failed to grow JSON buffer");
      }
      status = lonejson__emit_escaped_fragment(
          ljlua_json_buf_sink, buf, &error, (const unsigned char *)keys[i].text,
          keys[i].len);
      if (status != LONEJSON_STATUS_OK) {
        free(keys[i].text);
        free(keys);
        return luaL_error(L, "failed to encode JSON object key");
      }
      if (!ljlua_json_buf_append(buf, "\":", 2u)) {
        free(keys[i].text);
        free(keys);
        return luaL_error(L, "failed to grow JSON buffer");
      }
      lua_pushlstring(L, keys[i].text, keys[i].len);
      lua_gettable(L, index);
      ljlua_encode_json_value(L, lua_gettop(L), buf, visited, depth + 1u);
      lua_pop(L, 1);
      free(keys[i].text);
    }
    free(keys);
    ljlua_json_buf_append(buf, "}", 1u);
    return 1;
  }
  {
    size_t n = (size_t)lua_rawlen(L, index);
    ljlua_json_buf_append(buf, "[", 1u);
    for (i = 1u; i <= n; ++i) {
      if (i != 1u) {
        ljlua_json_buf_append(buf, ",", 1u);
      }
      lua_rawgeti(L, index, (lua_Integer)i);
      ljlua_encode_json_value(L, lua_gettop(L), buf, visited, depth + 1u);
      lua_pop(L, 1);
    }
    ljlua_json_buf_append(buf, "]", 1u);
    return 1;
  }
}

static int ljlua_encode_json_value(lua_State *L, int index, ljlua_json_buf *buf,
                                   const void **visited, size_t depth) {
  int type;
  size_t i;

  if (depth > 128u) {
    return luaL_error(L, "JSON value nesting exceeds Lua binding limit");
  }
  index = lua_absindex(L, index);
  if (ljlua_is_json_null(L, index)) {
    return ljlua_json_buf_append_cstr(buf, "null");
  }
  type = lua_type(L, index);
  switch (type) {
  case LUA_TNIL:
    return ljlua_json_buf_append_cstr(buf, "null");
  case LUA_TBOOLEAN:
    return ljlua_json_buf_append_cstr(buf, lua_toboolean(L, index) ? "true"
                                                                   : "false");
  case LUA_TNUMBER:
#if defined(LUA_MAXINTEGER)
    if (lua_isinteger(L, index)) {
      char num[64];
      snprintf(num, sizeof(num), "%lld", (long long)lua_tointeger(L, index));
      return ljlua_json_buf_append_cstr(buf, num);
    }
#endif
    {
      double num = (double)lua_tonumber(L, index);
      char text[64];

      if (!lonejson__is_finite_f64(num)) {
        return luaL_error(L, "JSON numbers must be finite");
      }
      snprintf(text, sizeof(text), "%.17g", num);
      return ljlua_json_buf_append_cstr(buf, text);
    }
  case LUA_TSTRING:
    return ljlua_encode_json_string(L, index, buf);
  case LUA_TTABLE: {
    const void *ptr = lua_topointer(L, index);
    for (i = 0u; i < depth; ++i) {
      if (visited[i] == ptr) {
        return luaL_error(L, "cyclic Lua tables cannot be encoded as JSON");
      }
    }
    visited[depth] = ptr;
    return ljlua_encode_json_table(L, index, buf, visited, depth);
  }
  default:
    return luaL_error(L, "unsupported Lua type for JSON value");
  }
}

static int ljlua_push_json_value(lua_State *L,
                                 const lonejson_json_value *value) {
  ljlua_json_buf buf;
  lonejson_error error;
  lonejson_status status;

  if (value == NULL || value->kind == LONEJSON_JSON_VALUE_NULL) {
    lua_pushnil(L);
    return 1;
  }
  memset(&buf, 0, sizeof(buf));
  status = lonejson_json_value_write_to_sink(value, ljlua_json_buf_sink, &buf,
                                             &error);
  if (status != LONEJSON_STATUS_OK) {
    free(buf.data);
    return luaL_error(L, "failed to materialize JSON value: %s", error.message);
  }
  ljlua_push_json_from_text(L, buf.data, buf.len);
  free(buf.data);
  return 1;
}

static int ljlua_assign_json_value_from_lua(lua_State *L,
                                            lonejson_json_value *value,
                                            int value_index) {
  ljlua_json_buf buf;
  lonejson_error error;
  lonejson_status status;
  const void *visited[129];

  memset(&buf, 0, sizeof(buf));
  memset(visited, 0, sizeof(visited));
  ljlua_encode_json_value(L, value_index, &buf, visited, 0u);
  status = lonejson_json_value_set_buffer(value, buf.data, buf.len, &error);
  free(buf.data);
  if (status != LONEJSON_STATUS_OK) {
    return luaL_error(L, "failed to assign JSON value: %s", error.message);
  }
  return 1;
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
  if (strcmp(name, "json_value") == 0) {
    return LJLUA_FIELD_JSON_VALUE;
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

