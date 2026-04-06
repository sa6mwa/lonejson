
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum lonejson_container_kind {
  LONEJSON_CONTAINER_OBJECT = 1,
  LONEJSON_CONTAINER_ARRAY = 2
} lonejson_container_kind;

typedef enum lonejson_frame_state {
  LONEJSON_FRAME_OBJECT_KEY_OR_END = 1,
  LONEJSON_FRAME_OBJECT_COLON = 2,
  LONEJSON_FRAME_OBJECT_VALUE = 3,
  LONEJSON_FRAME_OBJECT_COMMA_OR_END = 4,
  LONEJSON_FRAME_ARRAY_VALUE_OR_END = 5,
  LONEJSON_FRAME_ARRAY_COMMA_OR_END = 6
} lonejson_frame_state;

typedef enum lonejson_lex_mode {
  LONEJSON_LEX_NONE = 0,
  LONEJSON_LEX_STRING,
  LONEJSON_LEX_NUMBER,
  LONEJSON_LEX_TRUE,
  LONEJSON_LEX_FALSE,
  LONEJSON_LEX_NULL
} lonejson_lex_mode;

typedef enum lonejson_string_capture_mode {
  LONEJSON_STRING_CAPTURE_TOKEN = 0,
  LONEJSON_STRING_CAPTURE_DIRECT = 1,
  LONEJSON_STRING_CAPTURE_STREAM = 2,
  LONEJSON_STRING_CAPTURE_JSON_VISITOR = 3
} lonejson_string_capture_mode;

typedef struct lonejson_parser lonejson_parser;

typedef struct lonejson_scratch {
  char *data;
  size_t len;
  size_t cap;
} lonejson_scratch;

typedef struct lonejson_frame {
  lonejson_container_kind kind;
  lonejson_frame_state state;
  const lonejson_map *map;
  void *object_ptr;
  const lonejson_field *field;
  const lonejson_field *pending_field;
  lonejson_uint64 seen_mask;
  unsigned char *seen_fields;
  size_t seen_bytes;
  size_t next_field_hint;
  size_t required_remaining;
  int after_comma;
} lonejson_frame;

typedef union lonejson__parser_workspace_align_union {
  lonejson_frame frame;
  void *ptr;
  lonejson_uint64 u64;
  double f64;
} lonejson__parser_workspace_align_union;

typedef struct lonejson__parser_workspace_align_probe {
  char c;
  lonejson__parser_workspace_align_union value;
} lonejson__parser_workspace_align_probe;

#define LONEJSON__PARSER_WORKSPACE_ALIGNMENT                                \
  offsetof(lonejson__parser_workspace_align_probe, value)
#define LONEJSON__PARSER_WORKSPACE_SLACK                                    \
  (LONEJSON__PARSER_WORKSPACE_ALIGNMENT - 1u)

#define LONEJSON__MAP_MASK_CACHE_SIZE 8u
#define LONEJSON__SPOOLED_MAGIC 0x4C4A5350u
#define LONEJSON__JSON_VALUE_MAGIC 0x4C4A4A56u

#if defined(__GNUC__) || defined(__clang__)
#define LONEJSON__INLINE __inline__ __attribute__((always_inline))
#define LONEJSON__NOINLINE __attribute__((noinline))
#define LONEJSON__COLD __attribute__((cold))
#define LONEJSON__HOT __attribute__((hot))
#define LONEJSON__TEXT_LEN(text)                                            \
  (__builtin_constant_p(text) ? (sizeof(text) - 1u) : strlen(text))
#else
#define LONEJSON__INLINE
#define LONEJSON__NOINLINE
#define LONEJSON__COLD
#define LONEJSON__HOT
#define LONEJSON__TEXT_LEN(text) strlen(text)
#endif

static LONEJSON__INLINE unsigned
lonejson__init_cookie(const void *ptr, unsigned base_magic) {
  uintptr_t bits = (uintptr_t)ptr;
  return base_magic ^ (unsigned)(bits >> 4u) ^ (unsigned)(bits >> 9u);
}

struct lonejson_parser {
  lonejson_frame *frames;
  unsigned char *workspace;
  size_t workspace_size;
  size_t workspace_top;
  size_t workspace_peak;
  size_t frame_count;
  size_t self_alloc_size;
  int validate_only;
  int owns_self;
  int root_started;
  int root_finished;
  int failed;
  lonejson_lex_mode lex_mode;
  lonejson_string_capture_mode string_capture_mode;
  int lex_is_key;
  int lex_escape;
  lonejson_uint32 unicode_pending_high;
  int unicode_digits_needed;
  lonejson_uint32 unicode_accum;
  int stream_value_active;
  const lonejson_field *stream_field;
  void *stream_ptr;
  unsigned char stream_base64_quad[4];
  size_t stream_base64_quad_len;
  int stream_base64_saw_padding;
  int direct_string_active;
  char *direct_string_ptr;
  size_t direct_string_len;
  size_t direct_string_capacity;
  int direct_string_truncated;
  lonejson_overflow_policy direct_string_overflow_policy;
  const lonejson_field *direct_string_field;
  lonejson_scratch token;
  const lonejson_map *required_mask_maps[LONEJSON__MAP_MASK_CACHE_SIZE];
  lonejson_uint64 required_masks[LONEJSON__MAP_MASK_CACHE_SIZE];
  lonejson_json_value *json_stream_value;
  int json_stream_active;
  int json_stream_visit_active;
  int json_stream_sink_active;
  size_t json_stream_depth;
  size_t json_stream_total_bytes;
  size_t json_stream_text_bytes;
  int json_stream_text_is_key;
  const lonejson_map *root_map;
  void *root_dst;
  lonejson_parse_options options;
  lonejson_error error;
  lonejson_allocator allocator;
};

typedef struct lonejson_buffer_sink {
  char *buffer;
  size_t capacity;
  size_t length;
  size_t needed;
  size_t alloc_size;
  lonejson_overflow_policy policy;
  int truncated;
  const lonejson_allocator *allocator;
} lonejson_buffer_sink;

typedef enum lonejson_stream_source_kind {
  LONEJSON_STREAM_SOURCE_READER = 1,
  LONEJSON_STREAM_SOURCE_FILE = 2,
  LONEJSON_STREAM_SOURCE_FD = 3
} lonejson_stream_source_kind;

struct lonejson_stream {
  lonejson_reader_fn reader;
  void *reader_user;
  FILE *fp;
  int fd;
  int owns_fp;
  int owns_fd;
  int saw_eof;
  int object_in_progress;
  size_t buffered_start;
  size_t buffered_end;
  size_t self_alloc_size;
  lonejson_stream_source_kind source_kind;
  void *current_dst;
  void *prepared_dst;
  lonejson_parser *parser;
  const lonejson_map *map;
  lonejson_parse_options options;
  lonejson_error error;
  unsigned char io_buffer[LONEJSON_STREAM_BUFFER_SIZE];
  lonejson_allocator allocator;
};

typedef union lonejson__alloc_header_align_union {
  void *ptr;
  lonejson_uint64 u64;
  double f64;
  long double ld;
} lonejson__alloc_header_align_union;

typedef union lonejson__alloc_header {
  struct {
    lonejson_allocator allocator;
    size_t size;
  } meta;
  lonejson__alloc_header_align_union _align;
} lonejson__alloc_header;

static LONEJSON__INLINE int
lonejson__json_value_parse_visitor_active(const lonejson_parser *parser);
static LONEJSON__INLINE void
lonejson__parser_set_json_stream_value(lonejson_parser *parser,
                                       lonejson_json_value *value);
static LONEJSON__INLINE void
lonejson__parser_clear_json_stream_value(lonejson_parser *parser);
static lonejson_status lonejson__json_value_string_begin(lonejson_parser *parser,
                                                         int is_key);
static lonejson_status lonejson__json_value_string_chunk(
    lonejson_parser *parser, const char *data, size_t len);
static lonejson_status lonejson__json_value_string_end(lonejson_parser *parser);

static lonejson_status lonejson__set_error(lonejson_error *error,
                                           lonejson_status code, size_t offset,
                                           size_t line, size_t column,
                                           const char *fmt, ...) {
  va_list ap;

  if (error == NULL) {
    return code;
  }
  error->code = code;
  error->offset = offset;
  error->line = line;
  error->column = column;
  error->truncated = (code == LONEJSON_STATUS_TRUNCATED) ? 1 : error->truncated;
  va_start(ap, fmt);
  vsnprintf(error->message, sizeof(error->message), fmt, ap);
  va_end(ap);
  return code;
}

static void lonejson__clear_error(lonejson_error *error) {
  if (error == NULL) {
    return;
  }
  memset(error, 0, sizeof(*error));
  error->code = LONEJSON_STATUS_OK;
}

void lonejson_error_init(lonejson_error *error) {
  lonejson__clear_error(error);
}

static void *lonejson__default_malloc(void *ctx, size_t size) {
  (void)ctx;
  return LONEJSON_MALLOC(size);
}

static void *lonejson__default_realloc(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  return LONEJSON_REALLOC(ptr, size);
}

static void lonejson__default_free(void *ctx, void *ptr) {
  (void)ctx;
  LONEJSON_FREE(ptr);
}

lonejson_allocator lonejson_default_allocator(void) {
  lonejson_allocator allocator;

  allocator.malloc_fn = lonejson__default_malloc;
  allocator.realloc_fn = lonejson__default_realloc;
  allocator.free_fn = lonejson__default_free;
  allocator.ctx = NULL;
  allocator.stats = NULL;
  return allocator;
}

#define LONEJSON__ALLOCATOR_MISSING_CALLBACKS(allocator)                         \
  ((allocator) == NULL                                                           \
       ? 3                                                                       \
       : (((allocator)->malloc_fn == NULL ? 1 : 0) +                            \
          ((allocator)->realloc_fn == NULL ? 1 : 0) +                           \
          ((allocator)->free_fn == NULL ? 1 : 0)))

#define LONEJSON__ALLOCATOR_IS_VALID_CONFIG(allocator)                           \
  (LONEJSON__ALLOCATOR_MISSING_CALLBACKS(allocator) == 0 ||                     \
   LONEJSON__ALLOCATOR_MISSING_CALLBACKS(allocator) == 3)

static LONEJSON__INLINE lonejson_allocator
lonejson__allocator_resolve(const lonejson_allocator *allocator) {
  if (LONEJSON__ALLOCATOR_MISSING_CALLBACKS(allocator) == 3) {
    return lonejson_default_allocator();
  }
  return *allocator;
}

static int lonejson__allocator_is_default_family(
    const lonejson_allocator *allocator) {
  lonejson_allocator def;

  if (LONEJSON__ALLOCATOR_MISSING_CALLBACKS(allocator) == 3) {
    return 1;
  }
  if (!LONEJSON__ALLOCATOR_IS_VALID_CONFIG(allocator)) {
    return 0;
  }
  def = lonejson_default_allocator();
  return allocator->malloc_fn == def.malloc_fn &&
         allocator->realloc_fn == def.realloc_fn &&
         allocator->free_fn == def.free_fn && allocator->ctx == def.ctx;
}

static void lonejson__allocator_note_alloc(lonejson_allocator_stats *stats,
                                           size_t size) {
  if (stats == NULL) {
    return;
  }
  stats->alloc_calls++;
  stats->bytes_live += size;
  if (stats->bytes_live > stats->peak_bytes_live) {
    stats->peak_bytes_live = stats->bytes_live;
  }
}

static void lonejson__allocator_note_realloc(lonejson_allocator_stats *stats,
                                             size_t old_size,
                                             size_t new_size) {
  if (stats == NULL) {
    return;
  }
  stats->realloc_calls++;
  if (stats->bytes_live >= old_size) {
    stats->bytes_live -= old_size;
  } else {
    stats->bytes_live = 0u;
  }
  stats->bytes_live += new_size;
  if (stats->bytes_live > stats->peak_bytes_live) {
    stats->peak_bytes_live = stats->bytes_live;
  }
}

static void lonejson__allocator_note_free(lonejson_allocator_stats *stats,
                                          size_t size) {
  if (stats == NULL) {
    return;
  }
  stats->free_calls++;
  if (stats->bytes_live >= size) {
    stats->bytes_live -= size;
  } else {
    stats->bytes_live = 0u;
  }
}

static void *lonejson__buffer_alloc(const lonejson_allocator *allocator,
                                    size_t size) {
  lonejson_allocator resolved;
  void *ptr;

  resolved = lonejson__allocator_resolve(allocator);
  ptr = resolved.malloc_fn(resolved.ctx, size);
  if (ptr != NULL) {
    lonejson__allocator_note_alloc(resolved.stats, size);
  }
  return ptr;
}

static void *lonejson__buffer_realloc(const lonejson_allocator *allocator,
                                      void *ptr, size_t old_size,
                                      size_t new_size) {
  lonejson_allocator resolved;
  void *next;

  resolved = lonejson__allocator_resolve(allocator);
  if (ptr == NULL) {
    return lonejson__buffer_alloc(allocator, new_size);
  }
  next = resolved.realloc_fn(resolved.ctx, ptr, new_size);
  if (next != NULL) {
    lonejson__allocator_note_realloc(resolved.stats, old_size, new_size);
  }
  return next;
}

static void lonejson__buffer_free(const lonejson_allocator *allocator,
                                  void *ptr, size_t alloc_size) {
  lonejson_allocator resolved;

  if (ptr == NULL) {
    return;
  }
  resolved = lonejson__allocator_resolve(allocator);
  lonejson__allocator_note_free(resolved.stats, alloc_size);
  resolved.free_fn(resolved.ctx, ptr);
}

static void *lonejson__owned_malloc(const lonejson_allocator *allocator,
                                    size_t size) {
  lonejson_allocator resolved;
  lonejson__alloc_header *header;

  resolved = lonejson__allocator_resolve(allocator);
  header = (lonejson__alloc_header *)resolved.malloc_fn(
      resolved.ctx, sizeof(*header) + size);
  if (header == NULL) {
    return NULL;
  }
  header->meta.allocator = resolved;
  header->meta.size = size;
  lonejson__allocator_note_alloc(resolved.stats, size);
  return (void *)(header + 1u);
}

static void *lonejson__owned_realloc(const lonejson_allocator *allocator,
                                     void *ptr, size_t size) {
  lonejson__alloc_header *header;
  lonejson_allocator resolved;
  lonejson__alloc_header *next;

  if (ptr == NULL) {
    return lonejson__owned_malloc(allocator, size);
  }
  if (size == 0u) {
    return ptr;
  }
  header = ((lonejson__alloc_header *)ptr) - 1u;
  resolved = header->meta.allocator;
  next = (lonejson__alloc_header *)resolved.realloc_fn(
      resolved.ctx, header, sizeof(*header) + size);
  if (next == NULL) {
    return NULL;
  }
  lonejson__allocator_note_realloc(resolved.stats, next->meta.size, size);
  next->meta.allocator = resolved;
  next->meta.size = size;
  return (void *)(next + 1u);
}

static size_t lonejson__owned_size(const void *ptr) {
  const lonejson__alloc_header *header;

  if (ptr == NULL) {
    return 0u;
  }
  header = ((const lonejson__alloc_header *)ptr) - 1u;
  return header->meta.size;
}

static void lonejson__owned_free(void *ptr) {
  lonejson__alloc_header *header;

  if (ptr == NULL) {
    return;
  }
  header = ((lonejson__alloc_header *)ptr) - 1u;
  lonejson__allocator_note_free(header->meta.allocator.stats, header->meta.size);
  header->meta.allocator.free_fn(header->meta.allocator.ctx, header);
}

lonejson_spool_options lonejson_default_spool_options(void) {
  lonejson_spool_options options;

  options.memory_limit = LONEJSON_SPOOL_MEMORY_LIMIT;
  options.max_bytes = 0u;
  options.temp_dir = NULL;
  return options;
}

lonejson_read_result lonejson_default_read_result(void) {
  lonejson_read_result result;

  memset(&result, 0, sizeof(result));
  return result;
}

lonejson_value_visitor lonejson_default_value_visitor(void) {
  lonejson_value_visitor visitor;

  memset(&visitor, 0, sizeof(visitor));
  return visitor;
}

static void
lonejson__spooled_apply_options(lonejson_spooled *value,
                                const lonejson_spool_options *options) {
  lonejson_spool_options local;

  if (value == NULL) {
    return;
  }
  local = options ? *options : lonejson_default_spool_options();
  value->memory_limit = local.memory_limit;
  value->max_bytes = local.max_bytes;
  value->temp_dir = local.temp_dir;
}

static void lonejson__spooled_apply_allocator(
    lonejson_spooled *value, const lonejson_allocator *allocator) {
  if (value == NULL) {
    return;
  }
  value->allocator = LONEJSON__ALLOCATOR_IS_VALID_CONFIG(allocator)
                         ? lonejson__allocator_resolve(allocator)
                         : lonejson_default_allocator();
}

static LONEJSON__INLINE int
lonejson__spooled_is_initialized(const lonejson_spooled *value) {
  return value != NULL &&
         value->_lonejson_magic ==
             lonejson__init_cookie(value, LONEJSON__SPOOLED_MAGIC);
}

static void lonejson__json_value_apply_allocator(
    lonejson_json_value *value, const lonejson_allocator *allocator) {
  if (value == NULL) {
    return;
  }
  value->allocator = LONEJSON__ALLOCATOR_IS_VALID_CONFIG(allocator)
                         ? lonejson__allocator_resolve(allocator)
                         : lonejson_default_allocator();
}

static LONEJSON__INLINE int
lonejson__json_value_is_initialized(const lonejson_json_value *value) {
  return value != NULL &&
         value->_lonejson_magic ==
             lonejson__init_cookie(value, LONEJSON__JSON_VALUE_MAGIC);
}

void lonejson_source_init(lonejson_source *value) {
  if (value == NULL) {
    return;
  }
  memset(value, 0, sizeof(*value));
  value->kind = LONEJSON_SOURCE_NONE;
  value->fd = -1;
}

void lonejson_source_cleanup(lonejson_source *value) {
  if (value == NULL) {
    return;
  }
  lonejson__owned_free(value->path);
  value->path = NULL;
  value->fp = NULL;
  value->fd = -1;
  value->kind = LONEJSON_SOURCE_NONE;
}

void lonejson_source_reset(lonejson_source *value) {
  lonejson_source_cleanup(value);
}

void lonejson_json_value_init(lonejson_json_value *value) {
  lonejson_json_value_init_with_allocator(value, NULL);
}

void lonejson_json_value_init_with_allocator(
    lonejson_json_value *value, const lonejson_allocator *allocator) {
  if (value == NULL) {
    return;
  }
  memset(value, 0, sizeof(*value));
  value->kind = LONEJSON_JSON_VALUE_NULL;
  value->fd = -1;
  value->parse_mode = LONEJSON_JSON_VALUE_PARSE_NONE;
  value->parse_visitor = NULL;
  value->parse_visitor_user = NULL;
  value->parse_visitor_limits = lonejson_default_value_limits();
  lonejson__json_value_apply_allocator(value, allocator);
  value->_lonejson_magic =
      lonejson__init_cookie(value, LONEJSON__JSON_VALUE_MAGIC);
}

void lonejson_json_value_cleanup(lonejson_json_value *value) {
  lonejson_allocator allocator;

  if (value == NULL) {
    return;
  }
  allocator = value->allocator;
  lonejson__owned_free(value->json);
  lonejson__owned_free(value->path);
  value->json = NULL;
  value->path = NULL;
  value->len = 0u;
  value->reader = NULL;
  value->reader_user = NULL;
  value->fp = NULL;
  value->fd = -1;
  value->kind = LONEJSON_JSON_VALUE_NULL;
  value->parse_sink = NULL;
  value->parse_sink_user = NULL;
  value->parse_visitor = NULL;
  value->parse_visitor_user = NULL;
  value->parse_visitor_limits = lonejson_default_value_limits();
  value->parse_mode = LONEJSON_JSON_VALUE_PARSE_NONE;
  value->allocator = allocator;
  value->_lonejson_magic =
      lonejson__init_cookie(value, LONEJSON__JSON_VALUE_MAGIC);
}

void lonejson_json_value_reset(lonejson_json_value *value) {
  lonejson_json_value_cleanup(value);
}

static void
lonejson__json_value_clear_runtime(lonejson_json_value *value) {
  if (value == NULL) {
    return;
  }
  lonejson__owned_free(value->json);
  lonejson__owned_free(value->path);
  value->json = NULL;
  value->path = NULL;
  value->len = 0u;
  value->reader = NULL;
  value->reader_user = NULL;
  value->fp = NULL;
  value->fd = -1;
  value->kind = LONEJSON_JSON_VALUE_NULL;
}

static lonejson_status
lonejson__json_value_prepare_parse(lonejson_parser *parser,
                                   lonejson_json_value *value,
                                   lonejson_error *error) {
  (void)parser;
  if (value == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle is required");
  }
  if (value->parse_mode == LONEJSON_JSON_VALUE_PARSE_NONE) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "JSON value parse requires a configured parse sink, parse visitor, or parse capture");
  }
  lonejson__json_value_clear_runtime(value);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_source_set_file(lonejson_source *value, FILE *fp,
                                         lonejson_error *error) {
  if (value == NULL || fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "source handle and FILE* are required");
  }
  lonejson_source_cleanup(value);
  value->kind = LONEJSON_SOURCE_FILE;
  value->fp = fp;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_source_set_fd(lonejson_source *value, int fd,
                                       lonejson_error *error) {
  if (value == NULL || fd < 0) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "source handle and non-negative fd are required");
  }
  lonejson_source_cleanup(value);
  value->kind = LONEJSON_SOURCE_FD;
  value->fd = fd;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_source_set_path(lonejson_source *value,
                                         const char *path,
                                         lonejson_error *error) {
  size_t len;
  char *copy;

  if (value == NULL || path == NULL || path[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "source handle and path are required");
  }
  len = strlen(path);
  copy = (char *)lonejson__owned_malloc(NULL, len + 1u);
  if (copy == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to duplicate source path");
  }
  memcpy(copy, path, len + 1u);
  lonejson_source_cleanup(value);
  value->kind = LONEJSON_SOURCE_PATH;
  value->path = copy;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

void lonejson_spooled_init(lonejson_spooled *value,
                           const lonejson_spool_options *options) {
  lonejson_spooled_init_with_allocator(value, options, NULL);
}

void lonejson_spooled_init_with_allocator(
    lonejson_spooled *value, const lonejson_spool_options *options,
    const lonejson_allocator *allocator) {
  if (value == NULL) {
    return;
  }
  memset(value, 0, sizeof(*value));
  lonejson__spooled_apply_options(value, options);
  lonejson__spooled_apply_allocator(value, allocator);
  value->_lonejson_magic = lonejson__init_cookie(value, LONEJSON__SPOOLED_MAGIC);
}

static void lonejson__spooled_close_temp(lonejson_spooled *value) {
  if (value == NULL) {
    return;
  }
  if (value->spill_fp != NULL) {
    fclose(value->spill_fp);
    value->spill_fp = NULL;
  }
  if (value->temp_path[0] != '\0') {
    unlink(value->temp_path);
    value->temp_path[0] = '\0';
  }
}

void lonejson_spooled_cleanup(lonejson_spooled *value) {
  if (value == NULL) {
    return;
  }
  lonejson__owned_free(value->memory);
  value->memory = NULL;
  value->memory_len = 0u;
  value->size = 0u;
  value->read_offset = 0u;
  value->spilled = 0;
  lonejson__spooled_close_temp(value);
  value->_lonejson_magic = lonejson__init_cookie(value, LONEJSON__SPOOLED_MAGIC);
}

void lonejson_spooled_reset(lonejson_spooled *value) {
  lonejson_spool_options options;
  lonejson_allocator allocator;

  if (value == NULL) {
    return;
  }
  options.memory_limit = value->memory_limit;
  options.max_bytes = value->max_bytes;
  options.temp_dir = value->temp_dir;
  allocator = value->allocator;
  lonejson_spooled_cleanup(value);
  lonejson__spooled_apply_options(value, &options);
  value->allocator = allocator;
}

size_t lonejson_spooled_size(const lonejson_spooled *value) {
  return value ? value->size : 0u;
}

int lonejson_spooled_spilled(const lonejson_spooled *value) {
  return value ? value->spilled : 0;
}

static lonejson_status lonejson__spooled_open_temp(lonejson_spooled *value,
                                                   lonejson_error *error) {
  if (value->spill_fp != NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (value->temp_dir != NULL && value->temp_dir[0] != '\0') {
    int fd;
    int written;

    written = snprintf(value->temp_path, sizeof(value->temp_path),
                       "%s/lonejson-spool-XXXXXX", value->temp_dir);
    if (written < 0 || (size_t)written >= sizeof(value->temp_path)) {
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "temporary spool path is too long");
    }
    fd = mkstemp(value->temp_path);
    if (fd < 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to create spool file");
    }
    value->spill_fp = fdopen(fd, "w+b");
    if (value->spill_fp == NULL) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      close(fd);
      unlink(value->temp_path);
      value->temp_path[0] = '\0';
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to open spool file");
    }
  } else {
    value->spill_fp = tmpfile();
    if (value->spill_fp == NULL) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to create anonymous spool file");
    }
  }
  value->spilled = 1;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__spooled_reserve_memory(lonejson_spooled *value,
                                                        size_t need,
                                                        lonejson_error *error) {
  unsigned char *next;

  if (need <= value->memory_len) {
    return LONEJSON_STATUS_OK;
  }
  next = (unsigned char *)lonejson__owned_realloc(&value->allocator,
                                                  value->memory, need);
  if (next == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to expand spooled in-memory prefix");
  }
  value->memory = next;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__spooled_append(lonejson_spooled *value,
                                                const unsigned char *data,
                                                size_t len,
                                                lonejson_error *error) {
  size_t memory_room;
  size_t memory_copy;
  lonejson_status status;

  if (value == NULL || (data == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "invalid spooled append arguments");
  }
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (value->max_bytes != 0u && value->size + len > value->max_bytes) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "spooled field exceeds configured max bytes");
  }

  memory_room = (value->memory_limit > value->memory_len)
                    ? (value->memory_limit - value->memory_len)
                    : 0u;
  memory_copy = (len < memory_room) ? len : memory_room;
  if (memory_copy != 0u) {
    status = lonejson__spooled_reserve_memory(
        value, value->memory_len + memory_copy, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    memcpy(value->memory + value->memory_len, data, memory_copy);
    value->memory_len += memory_copy;
  }
  if (memory_copy < len) {
    size_t spill_len = len - memory_copy;

    status = lonejson__spooled_open_temp(value, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    if (fwrite(data + memory_copy, 1u, spill_len, value->spill_fp) !=
        spill_len) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to write spool file");
    }
  }
  value->size += len;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_spooled_rewind(lonejson_spooled *value,
                                        lonejson_error *error) {
  if (value == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "spooled value is required");
  }
  value->read_offset = 0u;
  if (value->spill_fp != NULL) {
    if (fseek(value->spill_fp, 0L, SEEK_SET) != 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to rewind spool file");
    }
  }
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_read_result lonejson_spooled_read(lonejson_spooled *value,
                                           unsigned char *buffer,
                                           size_t capacity) {
  lonejson_read_result result;
  size_t memory_remaining;
  size_t copied;

  memset(&result, 0, sizeof(result));
  if (value == NULL || buffer == NULL || capacity == 0u) {
    result.eof = 1;
    return result;
  }
  memory_remaining = (value->read_offset < value->memory_len)
                         ? (value->memory_len - value->read_offset)
                         : 0u;
  copied = (memory_remaining < capacity) ? memory_remaining : capacity;
  if (copied != 0u) {
    memcpy(buffer, value->memory + value->read_offset, copied);
    value->read_offset += copied;
    result.bytes_read = copied;
    result.eof = (value->read_offset == value->size) ? 1 : 0;
    return result;
  }
  if (value->spill_fp != NULL) {
    size_t got = fread(buffer, 1u, capacity, value->spill_fp);
    value->read_offset += got;
    result.bytes_read = got;
    result.eof = (value->read_offset >= value->size) ? 1 : 0;
    result.error_code = ferror(value->spill_fp) ? errno : 0;
    return result;
  }
  result.eof = 1;
  return result;
}

lonejson_status lonejson_spooled_write_to_sink(const lonejson_spooled *value,
                                               lonejson_sink_fn sink,
                                               void *user,
                                               lonejson_error *error) {
  unsigned char buffer[4096];
  lonejson_spooled cursor;
  lonejson_status status;

  if (value == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "spooled value and sink are required");
  }
  cursor = *value;
  status = lonejson_spooled_rewind(&cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (;;) {
    lonejson_read_result chunk =
        lonejson_spooled_read(&cursor, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read spooled value");
    }
    if (chunk.bytes_read != 0u) {
      status = sink(user, buffer, chunk.bytes_read, error);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (chunk.eof) {
      break;
    }
  }
  return LONEJSON_STATUS_OK;
}
