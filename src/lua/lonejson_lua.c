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
#define LJLUA_SPOOL_MT "lonejson.spool"
#define LJLUA_PATH_MT "lonejson.path"
#define LJLUA_SCHEMA_MAGIC 0x4c4a5343u
#define LJLUA_RECORD_MAGIC 0x4c4a5243u
#define LJLUA_STREAM_MAGIC 0x4c4a5354u
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

typedef struct ljlua_json_value_decode_state {
  ljlua_json_builder builder;
  lonejson_value_visitor visitor;
  int active;
  int complete;
} ljlua_json_value_decode_state;

typedef struct ljlua_decode_context {
  lua_State *L;
  ljlua_schema *schema;
  ljlua_json_value_decode_state *json_values;
} ljlua_decode_context;

static int ljlua_record_to_table_method(lua_State *L);
static int ljlua_record_get(lua_State *L);
static int ljlua_record_count(lua_State *L);
static int ljlua_record_clear(lua_State *L);
static int ljlua_path_get(lua_State *L);
static int ljlua_path_count(lua_State *L);
static int ljlua_getter_call(lua_State *L);
static int ljlua_counter_call(lua_State *L);
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
  case LJLUA_FIELD_JSON_VALUE:
    return sizeof(lonejson_json_value);
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
static int ljlua_json_null(lua_State *L);

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
  case LJLUA_FIELD_JSON_VALUE:
    meta->field.kind = LONEJSON_FIELD_KIND_JSON_VALUE;
    meta->field.storage = LONEJSON_STORAGE_FIXED;
    break;
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

  schema->has_json_value = 0;
  offset = 0u;
  for (i = 0u; i < schema->field_count; ++i) {
    ljlua_field_meta *meta = &schema->metas[i];
    size_t align = sizeof(void *);

    if (meta->lua_kind == LJLUA_FIELD_JSON_VALUE) {
      schema->has_json_value = 1;
    }
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
  if (lua_type(L, index) == LUA_TUSERDATA) {
    ljlua_schema_ud *ud = (ljlua_schema_ud *)lua_touserdata(L, index);
    if (ud != NULL && ud->magic == LJLUA_SCHEMA_MAGIC) {
      return ud;
    }
  }
  return (ljlua_schema_ud *)luaL_checkudata(L, index, LJLUA_SCHEMA_MT);
}

static ljlua_record_ud *ljlua_test_record(lua_State *L, int index) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    ljlua_record_ud *ud = (ljlua_record_ud *)lua_touserdata(L, index);
    if (ud != NULL && ud->magic == LJLUA_RECORD_MAGIC) {
      return ud;
    }
  }
  return NULL;
}

static ljlua_record_ud *ljlua_check_record(lua_State *L, int index) {
  ljlua_record_ud *ud = ljlua_test_record(L, index);

  if (ud != NULL) {
    return ud;
  }
  return (ljlua_record_ud *)luaL_checkudata(L, index, LJLUA_RECORD_MT);
}

static ljlua_stream_ud *ljlua_check_stream(lua_State *L, int index) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    ljlua_stream_ud *ud = (ljlua_stream_ud *)lua_touserdata(L, index);
    if (ud != NULL && ud->magic == LJLUA_STREAM_MAGIC) {
      return ud;
    }
  }
  return (ljlua_stream_ud *)luaL_checkudata(L, index, LJLUA_STREAM_MT);
}

static ljlua_spool_ud *ljlua_check_spool(lua_State *L, int index) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    ljlua_spool_ud *ud = (ljlua_spool_ud *)lua_touserdata(L, index);
    if (ud != NULL && ud->magic == LJLUA_SPOOL_MAGIC) {
      return ud;
    }
  }
  return (ljlua_spool_ud *)luaL_checkudata(L, index, LJLUA_SPOOL_MT);
}

static ljlua_path_ud *ljlua_check_path(lua_State *L, int index) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    ljlua_path_ud *ud = (ljlua_path_ud *)lua_touserdata(L, index);
    if (ud != NULL && ud->magic == LJLUA_PATH_MAGIC) {
      return ud;
    }
  }
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
  path_ud->magic = LJLUA_PATH_MAGIC;
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
  memset(path_ud, 0, sizeof(*path_ud));
  path_ud->magic = LJLUA_PATH_MAGIC;
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
  memset(path_ud, 0, sizeof(*path_ud));
  path_ud->magic = LJLUA_PATH_MAGIC;
  path_ud->schema = schema_ud->schema;
  path_ud->compiled = compiled;
  lua_pushvalue(L, 1);
  path_ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_PATH_MT);
  lua_pushvalue(L, -1);
  lua_pushcclosure(L, ljlua_counter_call, 1);
  return 1;
}

static const lonejson_write_options *
ljlua_get_write_options(lua_State *L, int index,
                        lonejson_write_options *options) {
  if (index == 0 || lua_isnoneornil(L, index)) {
    return NULL;
  }
  luaL_checktype(L, index, LUA_TTABLE);
  *options = lonejson_default_write_options();
  lua_getfield(L, index, "overflow");
  if (!lua_isnil(L, -1)) {
    const char *name = luaL_checkstring(L, -1);
    if (strcmp(name, "fail") == 0) {
      options->overflow_policy = LONEJSON_OVERFLOW_FAIL;
    } else if (strcmp(name, "truncate") == 0) {
      options->overflow_policy = LONEJSON_OVERFLOW_TRUNCATE;
    } else if (strcmp(name, "truncate_silent") == 0) {
      options->overflow_policy = LONEJSON_OVERFLOW_TRUNCATE_SILENT;
    } else {
      luaL_error(L, "unsupported overflow policy '%s'", name);
    }
  }
  lua_pop(L, 1);
  lua_getfield(L, index, "pretty");
  if (!lua_isnil(L, -1)) {
    options->pretty = lua_toboolean(L, -1) ? 1 : 0;
  }
  lua_pop(L, 1);
  return options;
}

static int ljlua_push_spool(lua_State *L, lonejson_spooled *spool, int kind,
                            int owner_index, int copy_value) {
  ljlua_spool_ud *ud;
  lonejson_error error;
  unsigned char buffer[4096];

  ud = (ljlua_spool_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->magic = LJLUA_SPOOL_MAGIC;
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

static int ljlua_schema_has_json_value(ljlua_schema *schema) {
  return schema != NULL && schema->has_json_value;
}

static void ljlua_decode_context_cleanup(ljlua_decode_context *ctx) {
  size_t i;

  if (ctx == NULL || ctx->json_values == NULL) {
    return;
  }
  for (i = 0u; i < ctx->schema->field_count; ++i) {
    if (ctx->json_values[i].active) {
      ljlua_json_builder_cleanup(ctx->L, &ctx->json_values[i].builder);
    }
  }
  free(ctx->json_values);
  ctx->json_values = NULL;
}

static int ljlua_decode_context_prepare_table_mode(lua_State *L,
                                                   ljlua_schema *schema,
                                                   void *record,
                                                   ljlua_decode_context *ctx) {
  size_t i;

  memset(ctx, 0, sizeof(*ctx));
  ctx->L = L;
  ctx->schema = schema;
  ctx->json_values = (ljlua_json_value_decode_state *)calloc(
      schema->field_count, sizeof(*ctx->json_values));
  if (ctx->json_values == NULL) {
    return luaL_error(L, "failed to allocate Lua JSON decode state");
  }
  for (i = 0u; i < schema->field_count; ++i) {
    if (schema->metas[i].lua_kind == LJLUA_FIELD_JSON_VALUE) {
      lonejson_json_value *value =
          (lonejson_json_value *)((unsigned char *)record +
                                  schema->metas[i].field.struct_offset);
      ljlua_json_builder_init(&ctx->json_values[i].builder, L);
      memset(&ctx->json_values[i].visitor, 0,
             sizeof(ctx->json_values[i].visitor));
      ctx->json_values[i].visitor.object_begin = ljlua_json_value_object_begin;
      ctx->json_values[i].visitor.object_end = ljlua_json_value_object_end;
      ctx->json_values[i].visitor.object_key_begin = ljlua_json_value_key_begin;
      ctx->json_values[i].visitor.object_key_chunk = ljlua_json_value_key_chunk;
      ctx->json_values[i].visitor.object_key_end = ljlua_json_value_key_end;
      ctx->json_values[i].visitor.array_begin = ljlua_json_value_array_begin;
      ctx->json_values[i].visitor.array_end = ljlua_json_value_array_end;
      ctx->json_values[i].visitor.string_begin = ljlua_json_value_string_begin;
      ctx->json_values[i].visitor.string_chunk = ljlua_json_value_string_chunk;
      ctx->json_values[i].visitor.string_end = ljlua_json_value_string_end;
      ctx->json_values[i].visitor.number_begin = ljlua_json_value_number_begin;
      ctx->json_values[i].visitor.number_chunk = ljlua_json_value_number_chunk;
      ctx->json_values[i].visitor.number_end = ljlua_json_value_number_end;
      ctx->json_values[i].visitor.boolean_value = ljlua_json_value_boolean;
      ctx->json_values[i].visitor.null_value = ljlua_json_value_null;
      ctx->json_values[i].active = 1;
      if (lonejson_json_value_set_parse_visitor(
              value, &ctx->json_values[i].visitor, &ctx->json_values[i], NULL,
              NULL) != LONEJSON_STATUS_OK) {
        return luaL_error(L, "failed to configure Lua JSON parse visitor");
      }
    }
  }
  return 1;
}

static void ljlua_json_value_enable_parse_capture_preserving_runtime(
    lonejson_json_value *value) {
  if (value == NULL) {
    return;
  }
  value->parse_mode = LONEJSON_JSON_VALUE_PARSE_CAPTURE;
  value->parse_sink = NULL;
  value->parse_sink_user = NULL;
  value->parse_visitor = NULL;
  value->parse_visitor_user = NULL;
  value->parse_visitor_limits = lonejson_default_value_limits();
}

static int ljlua_prepare_record_json_value_capture(lua_State *L,
                                                   ljlua_schema *schema,
                                                   void *record,
                                                   int preserve_runtime) {
  size_t i;
  lonejson_error error;

  for (i = 0u; i < schema->field_count; ++i) {
    if (schema->metas[i].lua_kind == LJLUA_FIELD_JSON_VALUE) {
      lonejson_json_value *value =
          (lonejson_json_value *)((unsigned char *)record +
                                  schema->metas[i].field.struct_offset);
      if (preserve_runtime) {
        ljlua_json_value_enable_parse_capture_preserving_runtime(value);
      } else if (lonejson_json_value_enable_parse_capture(value, &error) !=
                 LONEJSON_STATUS_OK) {
        return luaL_error(L, "failed to enable JSON value capture: %s",
                          error.message);
      }
    }
  }
  return 1;
}

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

static int
ljlua_record_to_table_with_overrides(lua_State *L, ljlua_schema *schema,
                                     void *record,
                                     const ljlua_decode_context *ctx) {
  size_t i;
  lua_createtable(L, 0, (int)schema->field_count);
  for (i = 0u; i < schema->field_count; ++i) {
    if (ctx != NULL && ctx->json_values != NULL && ctx->json_values[i].active) {
      int ref = ctx->json_values[i].builder.root_ref;
      if (ref == LUA_NOREF || ref == LUA_REFNIL) {
        lua_pushnil(L);
      } else {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
      }
    } else {
      ljlua_push_value(L, schema, record, &schema->metas[i], 0);
    }
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
  case LJLUA_FIELD_JSON_VALUE:
    return ljlua_push_json_value(L, (const lonejson_json_value *)ptr);
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

static void ljlua_cleanup_record_storage(ljlua_schema *schema, void *record) {
  if (schema == NULL || record == NULL) {
    return;
  }
  lonejson__cleanup_map(&schema->map, record);
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
  case LJLUA_FIELD_JSON_VALUE:
    return ljlua_assign_json_value_from_lua(L, (lonejson_json_value *)ptr,
                                            value_index);
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
  memset(ud, 0, sizeof(*ud));
  ud->magic = LJLUA_SCHEMA_MAGIC;
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
  memset(record_ud, 0, sizeof(*record_ud) + schema_ud->schema->record_size);
  record_ud->magic = LJLUA_RECORD_MAGIC;
  record_ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  record_ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  record_ud->cleared = 1;
  luaL_setmetatable(L, LJLUA_RECORD_MT);
  ljlua_prepare_record_storage(record_ud->schema, record_ud->data);
  return 1;
}

static int ljlua_record_gc(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  if (ud->schema != NULL) {
    ljlua_cleanup_record_storage(ud->schema, ud->data);
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
  ljlua_compiled_path *compiled;
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
  compiled = ljlua_find_compiled_path(ud->schema, key);
  if (compiled == NULL) {
    compiled = ljlua_compile_path(L, ud->schema, key);
  }
  if (compiled != NULL && compiled->step_count == 1u &&
      compiled->steps[0].index == 0u) {
    return ljlua_push_compiled_path_value(L, ud->data, compiled, 1);
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
    ljlua_compiled_path *compiled = ljlua_find_compiled_path(ud->schema, key);
    if (compiled == NULL) {
      compiled = ljlua_compile_path(L, ud->schema, key);
    }
    if (compiled == NULL || compiled->step_count != 1u ||
        compiled->steps[0].index != 0u) {
      return luaL_error(L, "unknown field '%s'", key);
    }
    meta = compiled->steps[0].meta;
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
  ud->cleared = 1;
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

static int
ljlua_schema_decode_into_buffer(lua_State *L, ljlua_schema *schema,
                                void *record, const char *json, size_t len,
                                const lonejson_parse_options *input_options) {
  lonejson_parse_options options;
  lonejson_error error;
  lonejson_status status;

  options = input_options ? *input_options : lonejson_default_parse_options();
  if (ljlua_schema_has_json_value(schema)) {
    options.clear_destination = 0;
  }
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
  lonejson_parse_options options;
  lonejson_parse_options effective_options;
  size_t len;
  const char *json = luaL_checklstring(L, 3, &len);

  ljlua_parse_parse_options(L, 4, &options);
  effective_options = options;
  if (record_ud->schema != schema_ud->schema) {
    return luaL_error(L, "record belongs to a different schema");
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    if (options.clear_destination) {
      lonejson_reset(&schema_ud->schema->map, record_ud->data);
    }
    ljlua_prepare_record_json_value_capture(L, schema_ud->schema,
                                            record_ud->data,
                                            options.clear_destination ? 0 : 1);
  } else {
    if (options.clear_destination) {
      lonejson_reset(&schema_ud->schema->map, record_ud->data);
      effective_options.clear_destination = 0;
    } else if (record_ud->cleared) {
      effective_options.clear_destination = 0;
    }
  }
  if (!ljlua_schema_decode_into_buffer(L, schema_ud->schema, record_ud->data,
                                       json, len, &effective_options)) {
    return 2;
  }
  record_ud->cleared = 0;
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
  ljlua_decode_context ctx;
  ljlua_parse_parse_options(L, 3, &options);
  memset(&ctx, 0, sizeof(ctx));
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  ljlua_prepare_record_storage(schema_ud->schema, record);
  options.clear_destination = 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
  }
  status = lonejson_parse_path(&schema_ud->schema->map, record, path, &options,
                               &error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_decode_context_cleanup(&ctx);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
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
  ljlua_decode_context ctx;
  ljlua_parse_parse_options(L, 3, &options);
  memset(&ctx, 0, sizeof(ctx));
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  ljlua_prepare_record_storage(schema_ud->schema, record);
  options.clear_destination = 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
  }
  status = lonejson_parse_filep(&schema_ud->schema->map, record, fp, &options,
                                &error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_decode_context_cleanup(&ctx);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
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
  ljlua_decode_context ctx;
  ljlua_parse_parse_options(L, 3, &options);
  memset(&ctx, 0, sizeof(ctx));
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  ljlua_prepare_record_storage(schema_ud->schema, record);
  options.clear_destination = 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
  }
  fp = ljlua_dup_fd_to_file(L, fd, "rb");
  status = lonejson_parse_filep(&schema_ud->schema->map, record, fp, &options,
                                &error);
  fclose(fp);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_decode_context_cleanup(&ctx);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_decode(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  size_t len;
  const char *json = luaL_checklstring(L, 2, &len);
  unsigned char *record;
  int owned_record;
  ljlua_decode_context ctx;
  lonejson_parse_options options;
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  memset(&ctx, 0, sizeof(ctx));
  ljlua_parse_parse_options(L, 3, &options);
  ljlua_prepare_record_storage(schema_ud->schema, record);
  options.clear_destination = 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
    {
      lonejson_error error;
      lonejson_status status = lonejson_parse_buffer(
          &schema_ud->schema->map, record, json, len, &options, &error);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        ljlua_decode_context_cleanup(&ctx);
        ljlua_cleanup_record_storage(schema_ud->schema, record);
        ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
        lua_pushnil(L);
        ljlua_push_error(L, &error);
        return 2;
      }
    }
  } else if (!ljlua_schema_decode_into_buffer(L, schema_ud->schema, record,
                                              json, len, &options)) {
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    return 2;
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
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
  record_ud->cleared = 1;
  ljlua_assign_table_to_record(L, schema_ud->schema, record_ud->data, 3);
  record_ud->cleared = 0;
  lua_pushvalue(L, 2);
  return 1;
}

static char *ljlua_serialize_value(lua_State *L, ljlua_schema *schema,
                                   void *record, int options_index,
                                   size_t *out_len) {
  lonejson_write_options local_options;
  const lonejson_write_options *options;
  lonejson_error error;
  lonejson_status status;
  size_t needed = 0u;
  size_t capacity;
  char *buffer;

  options = ljlua_get_write_options(L, options_index, &local_options);
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
        &needed, options, &error);
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
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  }
  lua_pushlstring(L, json, out_len);
  return 1;
}

static int ljlua_schema_write_path(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 3);
  lonejson_write_options local_options;
  const lonejson_write_options *options;
  lonejson_error error;
  lonejson_status status;

  options = ljlua_get_write_options(L, 4, &local_options);
  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    status = lonejson_serialize_path(&schema_ud->schema->map, record_ud->data,
                                     path, options, &error);
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
                                     options, &error);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
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
  lonejson_write_options local_options;
  const lonejson_write_options *options;
  lonejson_error error;
  lonejson_status status;

  options = ljlua_get_write_options(L, 4, &local_options);
  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    status = lonejson_serialize_filep(&schema_ud->schema->map, record_ud->data,
                                      fp, options, &error);
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
                                      options, &error);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
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
  lonejson_write_options local_options;
  const lonejson_write_options *options;
  lonejson_error error;
  lonejson_status status;

  options = ljlua_get_write_options(L, 4, &local_options);
  fp = ljlua_dup_fd_to_file(L, fd, "wb");
  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    status = lonejson_serialize_filep(&schema_ud->schema->map, record_ud->data,
                                      fp, options, &error);
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
                                      options, &error);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
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
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_STREAM_MAGIC;
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
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_STREAM_MAGIC;
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
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_STREAM_MAGIC;
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
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_STREAM_MAGIC;
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
  ljlua_record_ud *record_ud = ljlua_test_record(L, 2);

  if (record_ud != NULL) {
    if (record_ud->schema != ud->schema) {
      return luaL_error(L, "record belongs to a different schema");
    }
    if (ljlua_schema_has_json_value(ud->schema)) {
      if (ud->clear_destination) {
        lonejson_reset(&ud->schema->map, record_ud->data);
      }
      ljlua_prepare_record_json_value_capture(L, ud->schema, record_ud->data,
                                              ud->clear_destination ? 0 : 1);
    }
    result = lonejson_stream_next(ud->stream, record_ud->data, &error);
    if (result == LONEJSON_STREAM_OBJECT) {
      record_ud->cleared = 0;
      lua_pushvalue(L, 2);
      lua_pushnil(L);
      lua_pushstring(L, "object");
      return 3;
    }
  } else {
    unsigned char *record =
        (unsigned char *)calloc(1u, ud->schema->record_size);
    ljlua_decode_context ctx;
    if (record == NULL) {
      return luaL_error(L, "failed to allocate decode buffer");
    }
    memset(&ctx, 0, sizeof(ctx));
    ljlua_prepare_record_storage(ud->schema, record);
    if (ljlua_schema_has_json_value(ud->schema)) {
      ljlua_decode_context_prepare_table_mode(L, ud->schema, record, &ctx);
    }
    result = lonejson_stream_next(ud->stream, record, &error);
    if (result == LONEJSON_STREAM_OBJECT) {
      if (ljlua_schema_has_json_value(ud->schema)) {
        ljlua_record_to_table_with_overrides(L, ud->schema, record, &ctx);
      } else {
        ljlua_record_to_table(L, ud->schema, record);
      }
      ljlua_decode_context_cleanup(&ctx);
      ljlua_cleanup_record_storage(ud->schema, record);
      free(record);
      lua_pushnil(L);
      lua_pushstring(L, "object");
      return 3;
    }
    ljlua_decode_context_cleanup(&ctx);
    ljlua_cleanup_record_storage(ud->schema, record);
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
