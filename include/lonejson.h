/*
 * MIT License
 *
 * lonejson is Copyright (c) 2026 Michel Blomgren <mike@pkt.systems>
 * https://pkt.systems
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LONEJSON_H
#define LONEJSON_H

#include <stddef.h>
#include <stdio.h>

#if defined(LJ_DISABLE_SHORT_NAMES) && !defined(LONEJSON_DISABLE_SHORT_NAMES)
#define LONEJSON_DISABLE_SHORT_NAMES 1
#endif

#if defined(LJ_IMPLEMENTATION) && !defined(LONEJSON_IMPLEMENTATION)
#define LONEJSON_IMPLEMENTATION
#endif
#if defined(LJ_WITH_CURL) && !defined(LONEJSON_WITH_CURL)
#define LONEJSON_WITH_CURL
#endif
#if defined(LJ_MALLOC) && !defined(LONEJSON_MALLOC)
#define LONEJSON_MALLOC LJ_MALLOC
#endif
#if defined(LJ_CALLOC) && !defined(LONEJSON_CALLOC)
#define LONEJSON_CALLOC LJ_CALLOC
#endif
#if defined(LJ_REALLOC) && !defined(LONEJSON_REALLOC)
#define LONEJSON_REALLOC LJ_REALLOC
#endif
#if defined(LJ_FREE) && !defined(LONEJSON_FREE)
#define LONEJSON_FREE LJ_FREE
#endif
#if defined(LJ_PARSER_BUFFER_SIZE) && !defined(LONEJSON_PARSER_BUFFER_SIZE)
#define LONEJSON_PARSER_BUFFER_SIZE LJ_PARSER_BUFFER_SIZE
#endif
#if defined(LJ_READER_BUFFER_SIZE) && !defined(LONEJSON_READER_BUFFER_SIZE)
#define LONEJSON_READER_BUFFER_SIZE LJ_READER_BUFFER_SIZE
#endif
#if defined(LJ_PUSH_PARSER_BUFFER_SIZE) &&                                     \
    !defined(LONEJSON_PUSH_PARSER_BUFFER_SIZE)
#define LONEJSON_PUSH_PARSER_BUFFER_SIZE LJ_PUSH_PARSER_BUFFER_SIZE
#endif
#if defined(LJ_STREAM_BUFFER_SIZE) && !defined(LONEJSON_STREAM_BUFFER_SIZE)
#define LONEJSON_STREAM_BUFFER_SIZE LJ_STREAM_BUFFER_SIZE
#endif
#if defined(LJ_SPOOL_MEMORY_LIMIT) && !defined(LONEJSON_SPOOL_MEMORY_LIMIT)
#define LONEJSON_SPOOL_MEMORY_LIMIT LJ_SPOOL_MEMORY_LIMIT
#endif
#if defined(LJ_SPOOL_TEMP_PATH_CAPACITY) &&                                    \
    !defined(LONEJSON_SPOOL_TEMP_PATH_CAPACITY)
#define LONEJSON_SPOOL_TEMP_PATH_CAPACITY LJ_SPOOL_TEMP_PATH_CAPACITY
#endif
#if defined(LJ_TRACK_WORKSPACE_USAGE) &&                                       \
    !defined(LONEJSON_TRACK_WORKSPACE_USAGE)
#define LONEJSON_TRACK_WORKSPACE_USAGE LJ_TRACK_WORKSPACE_USAGE
#endif

#if !defined(__cplusplus)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdbool.h>
#elif !defined(bool)
typedef int bool;
#define bool bool
#define true 1
#define false 0
#endif
#endif

#if defined(_MSC_VER)
typedef unsigned __int32 lonejson_uint32;
typedef __int64 lonejson_int64;
typedef unsigned __int64 lonejson_uint64;
#elif defined(__GNUC__) || defined(__clang__)
__extension__ typedef unsigned long lonejson_uint32;
__extension__ typedef signed long long lonejson_int64;
__extension__ typedef unsigned long long lonejson_uint64;
#else
typedef unsigned long lonejson_uint32;
typedef signed long long lonejson_int64;
typedef unsigned long long lonejson_uint64;
#endif

#if !defined(SIZE_MAX)
#define SIZE_MAX ((size_t)-1)
#endif

#if defined(_MSC_VER)
#define LONEJSON_SHORT_ALIAS_INLINE static __inline
#elif defined(__GNUC__) || defined(__clang__)
#define LONEJSON_SHORT_ALIAS_INLINE static __inline__ __attribute__((unused))
#else
#define LONEJSON_SHORT_ALIAS_INLINE static
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file lonejson.h
 *
 * lonejson is a single-header C library for schema-guided JSON input and
 * output. It is meant for programs that already know the shape of the data
 * they care about and want to move between JSON objects and C structs without
 * dragging a generic DOM through the middle of the program. The library can
 * parse from memory, files, file descriptors, and object-framed streams; it
 * can serialize to buffers, sinks, files, and JSON Lines; and it can keep very
 * large field values out of memory by spilling inbound values to temporary
 * files or by serializing outbound values directly from paths, `FILE *`, or
 * file descriptors.
 *
 * The usual lonejson workflow is simple. You define a C struct, describe its
 * JSON-visible fields with `LONEJSON_FIELD_*` macros, bind those fields into a
 * `lonejson_map`, and then parse or serialize through that map. The design is
 * deliberately schema-guided: lonejson is at its best when you want stable,
 * typed decoding into known structures, predictable fixed-capacity behavior,
 * and efficient object-framed streaming instead of a line-delimited protocol.
 *
 * The project home, release artifacts, and full example programs are at:
 *
 *   https://github.com/sa6mwa/lonejson
 *   https://github.com/sa6mwa/lonejson/tree/main/examples
 *
 * To integrate the header, define `LONEJSON_IMPLEMENTATION` in exactly one
 * translation unit before including it:
 *
 * ```c
 * #define LONEJSON_IMPLEMENTATION
 * #include "lonejson.h"
 * ```
 *
 * Short aliases are enabled by default. The long names remain canonical, but
 * the library also exposes `lj_*` and `LJ_*` aliases for projects that prefer
 * a shorter prefix. If those names collide with another dependency, disable
 * them before the include:
 *
 * ```c
 * #define LONEJSON_DISABLE_SHORT_NAMES 1
 * #include "lonejson.h"
 * ```
 *
 * The public API falls into a few coherent groups. Mapping is done with
 * `LONEJSON_FIELD_*` and `LONEJSON_MAP_DEFINE(...)`. One-shot parsing is
 * handled by `lonejson_parse_cstr`, `lonejson_parse_buffer`,
 * `lonejson_parse_reader`, `lonejson_parse_filep`, and `lonejson_parse_path`,
 * with `lonejson_validate_*` variants available when you only need syntax
 * validation. Object-framed streams use `lonejson_stream_open_*`,
 * `lonejson_stream_next`, `lonejson_stream_error`, and
 * `lonejson_stream_close`. Serialization is exposed through
 * `lonejson_serialize_*` and `lonejson_serialize_jsonl_*`. Ownership and reuse
 * of mapped values is handled by `lonejson_cleanup` and `lonejson_reset`.
 * Large inbound values are represented by `lonejson_spooled`, large outbound
 * values by `lonejson_source`, and behavior is configured through
 * `lonejson_parse_options`, `lonejson_spool_options`, and
 * `lonejson_write_options`.
 *
 * The small examples below are meant as orientation, not as exhaustive
 * coverage. The repository examples are fuller programs with proper error
 * paths, build instructions, and end-to-end behavior.
 *
 * Parsing one JSON object into a struct looks like this:
 *
 * ```c
 * typedef struct user_doc {
 *   char *name;
 *   lonejson_int64 age;
 * } user_doc;
 *
 * static const lonejson_field user_doc_fields[] = {
 *   LONEJSON_FIELD_STRING_ALLOC_REQ(user_doc, name, "name"),
 *   LONEJSON_FIELD_I64(user_doc, age, "age")
 * };
 *
 * LONEJSON_MAP_DEFINE(user_doc_map, user_doc, user_doc_fields);
 *
 * user_doc doc;
 * lonejson_error error;
 * lonejson_status status;
 *
 * status = lonejson_parse_cstr(
 *   &user_doc_map, &doc, "{\"name\":\"Ada\",\"age\":37}", NULL, &error);
 * if (status != LONEJSON_STATUS_OK) {
 *   return;
 * }
 *
 * lonejson_cleanup(&user_doc_map, &doc);
 * ```
 *
 * Object-framed streaming is the right fit when the input consists of
 * consecutive JSON objects with optional whitespace between them. lonejson does
 * not treat newlines as framing; it treats completed objects as framing.
 *
 * ```c
 * lonejson_stream stream;
 * lonejson_stream_result result;
 * user_doc doc;
 *
 * lonejson_stream_open_fd(&stream, &user_doc_map, fd, NULL);
 *
 * for (;;) {
 *   result = lonejson_stream_next(&stream, &doc);
 *   if (result == LONEJSON_STREAM_OBJECT) {
 *     lonejson_cleanup(&user_doc_map, &doc);
 *     continue;
 *   }
 *   if (result == LONEJSON_STREAM_EOF) {
 *     break;
 *   }
 *   break;
 * }
 *
 * lonejson_stream_close(&stream);
 * ```
 *
 * Serializing one mapped object is symmetrical:
 *
 * ```c
 * char buffer[256];
 * size_t written = 0u;
 * lonejson_write_options options = lonejson_default_write_options();
 *
 * options.pretty = 1;
 *
 * if (lonejson_serialize_buffer(
 *       &user_doc_map, &doc, buffer, sizeof(buffer), &written,
 *       &options, &error) != LONEJSON_STATUS_OK) {
 *   return;
 * }
 * ```
 *
 * JSON Lines output is available when you need one compact object per line:
 *
 * ```c
 * user_doc docs[2];
 * char out[256];
 * size_t written = 0u;
 *
 * lonejson_serialize_jsonl_buffer(
 *   &user_doc_map, docs, 2u, 0u, out, sizeof(out), &written, NULL, &error);
 * ```
 *
 * Very large inbound values can spill to disk instead of growing without
 * bound in memory:
 *
 * ```c
 * typedef struct blob_doc {
 *   lonejson_spooled body;
 * } blob_doc;
 *
 * static const lonejson_field blob_doc_fields[] = {
 *   LONEJSON_FIELD_STRING_STREAM_REQ(blob_doc, body, "body")
 * };
 *
 * LONEJSON_MAP_DEFINE(blob_doc_map, blob_doc, blob_doc_fields);
 *
 * blob_doc doc;
 * lonejson_parse_options options = lonejson_default_parse_options();
 *
 * options.spool.memory_limit = 64u * 1024u;
 * options.spool.temp_dir = "/tmp";
 *
 * lonejson_parse_path(&blob_doc_map, &doc, "input.json", &options, &error);
 * lonejson_spooled_write_to_sink(&doc.body, sink_fn, sink_user);
 * lonejson_cleanup(&blob_doc_map, &doc);
 * ```
 *
 * Outbound source-backed fields cover the opposite direction. They let you
 * serialize a JSON string or base64 field directly from a path, `FILE *`, or
 * file descriptor without first building a spool:
 *
 * ```c
 * typedef struct outbound_doc {
 *   lonejson_source payload;
 * } outbound_doc;
 *
 * static const lonejson_field outbound_doc_fields[] = {
 *   LONEJSON_FIELD_BASE64_SOURCE_REQ(outbound_doc, payload, "payload")
 * };
 *
 * LONEJSON_MAP_DEFINE(outbound_doc_map, outbound_doc, outbound_doc_fields);
 *
 * outbound_doc doc;
 * lonejson_source_set_path(&doc.payload, "payload.bin");
 * lonejson_serialize_path(&outbound_doc_map, &doc, "request.json", NULL,
 * &error); lonejson_cleanup(&outbound_doc_map, &doc);
 * ```
 *
 * The repository examples expand these patterns into complete programs covering
 * one-shot parsing, object-framed streams, fixed-capacity fields, spool-to-disk
 * fields, source-backed outbound fields, JSONL output, optional curl
 * integration, and the Lua binding.
 */

/** Major component of the lonejson header version. */
#define LONEJSON_VERSION_MAJOR 0
/** Minor component of the lonejson header version. */
#define LONEJSON_VERSION_MINOR 1
/** Patch component of the lonejson header version. */
#define LONEJSON_VERSION_PATCH 0

/** Marks a mapping field as required during parse. */
#define LONEJSON_FIELD_REQUIRED (1u << 0)

/** Internal/runtime flag indicating that an array container owns its backing
 * allocation. */
#define LONEJSON_ARRAY_OWNS_ITEMS (1u << 0)
/** Marks an array container as using caller-owned fixed-capacity storage. */
#define LONEJSON_ARRAY_FIXED_CAPACITY (1u << 1)
/** Overrides lonejson's internal `malloc` implementation when
 * `LONEJSON_IMPLEMENTATION` is enabled. */
#ifndef LONEJSON_MALLOC
#define LONEJSON_MALLOC malloc
#endif
/** Overrides lonejson's internal `calloc` implementation when
 * `LONEJSON_IMPLEMENTATION` is enabled. */
#ifndef LONEJSON_CALLOC
#define LONEJSON_CALLOC calloc
#endif
/** Overrides lonejson's internal `realloc` implementation when
 * `LONEJSON_IMPLEMENTATION` is enabled. */
#ifndef LONEJSON_REALLOC
#define LONEJSON_REALLOC realloc
#endif
/** Overrides lonejson's internal `free` implementation when
 * `LONEJSON_IMPLEMENTATION` is enabled. */
#ifndef LONEJSON_FREE
#define LONEJSON_FREE free
#endif

/** Total internal parser workspace budget used by one-shot parse/validate
 * entry points. */
#ifndef LONEJSON_PARSER_BUFFER_SIZE
#define LONEJSON_PARSER_BUFFER_SIZE 4096u
#endif
/** Private read-buffer size used by one-shot reader-based parse/validate entry
 * points. */
#ifndef LONEJSON_READER_BUFFER_SIZE
#define LONEJSON_READER_BUFFER_SIZE 1024u
#endif
/** Total internal parser workspace budget used by the push parser.
 * Defaults to `LONEJSON_PARSER_BUFFER_SIZE`. */
#ifndef LONEJSON_PUSH_PARSER_BUFFER_SIZE
#define LONEJSON_PUSH_PARSER_BUFFER_SIZE LONEJSON_PARSER_BUFFER_SIZE
#endif
/** Private read-buffer size used by object-framed streaming APIs. Defaults to
 * `LONEJSON_READER_BUFFER_SIZE`. */
#ifndef LONEJSON_STREAM_BUFFER_SIZE
#define LONEJSON_STREAM_BUFFER_SIZE LONEJSON_READER_BUFFER_SIZE
#endif
/** Default in-memory threshold before streamed fields spill into a temporary
 * file. */
#ifndef LONEJSON_SPOOL_MEMORY_LIMIT
#define LONEJSON_SPOOL_MEMORY_LIMIT 65536u
#endif
/** Fixed path-buffer capacity used for named temporary spool files. */
#ifndef LONEJSON_SPOOL_TEMP_PATH_CAPACITY
#define LONEJSON_SPOOL_TEMP_PATH_CAPACITY 512u
#endif

#ifndef LONEJSON_TRACK_WORKSPACE_USAGE
#define LONEJSON_TRACK_WORKSPACE_USAGE 0
#endif

/** Outcome codes returned by lonejson parse, stream, and serialization APIs.
 */
typedef enum lonejson_status {
  /** Operation completed successfully. */
  LONEJSON_STATUS_OK = 0,
  /** Caller supplied an invalid pointer, size, or incompatible argument. */
  LONEJSON_STATUS_INVALID_ARGUMENT,
  /** Input was not syntactically valid JSON. */
  LONEJSON_STATUS_INVALID_JSON,
  /** A JSON value did not match the mapped destination field type. */
  LONEJSON_STATUS_TYPE_MISMATCH,
  /** A field marked required by the map was not present in the object. */
  LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
  /** Duplicate object keys were rejected by parse options. */
  LONEJSON_STATUS_DUPLICATE_FIELD,
  /** A fixed-capacity destination or configured spool limit was exceeded. */
  LONEJSON_STATUS_OVERFLOW,
  /** Output was truncated under a non-failing overflow policy. */
  LONEJSON_STATUS_TRUNCATED,
  /** lonejson could not allocate internal or mapped dynamic storage. */
  LONEJSON_STATUS_ALLOCATION_FAILED,
  /** A caller-provided sink or reader callback reported failure. */
  LONEJSON_STATUS_CALLBACK_FAILED,
  /** An underlying file descriptor or `FILE *` operation failed. */
  LONEJSON_STATUS_IO_ERROR,
  /** lonejson encountered an unexpected internal state. */
  LONEJSON_STATUS_INTERNAL_ERROR
} lonejson_status;

/** Internal field categories understood by `lonejson_field` maps. Most users
 * select these indirectly through the `LONEJSON_FIELD_*` macros. */
typedef enum lonejson_field_kind {
  LONEJSON_FIELD_KIND_STRING,
  LONEJSON_FIELD_KIND_STRING_STREAM,
  LONEJSON_FIELD_KIND_BASE64_STREAM,
  LONEJSON_FIELD_KIND_STRING_SOURCE,
  LONEJSON_FIELD_KIND_BASE64_SOURCE,
  LONEJSON_FIELD_KIND_I64,
  LONEJSON_FIELD_KIND_U64,
  LONEJSON_FIELD_KIND_F64,
  LONEJSON_FIELD_KIND_BOOL,
  LONEJSON_FIELD_KIND_OBJECT,
  LONEJSON_FIELD_KIND_STRING_ARRAY,
  LONEJSON_FIELD_KIND_I64_ARRAY,
  LONEJSON_FIELD_KIND_U64_ARRAY,
  LONEJSON_FIELD_KIND_F64_ARRAY,
  LONEJSON_FIELD_KIND_BOOL_ARRAY,
  LONEJSON_FIELD_KIND_OBJECT_ARRAY
} lonejson_field_kind;

/** Backing source kind used by `lonejson_source` outbound field handles. */
typedef enum lonejson_source_kind {
  /** No source configured. Serializes as JSON `null`. */
  LONEJSON_SOURCE_NONE = 0,
  /** Read raw bytes from a caller-provided `FILE *`. */
  LONEJSON_SOURCE_FILE = 1,
  /** Read raw bytes from a caller-provided file descriptor. */
  LONEJSON_SOURCE_FD = 2,
  /** Open and read a filesystem path on each serialization. */
  LONEJSON_SOURCE_PATH = 3
} lonejson_source_kind;

/** Storage model used by a mapped field. */
typedef enum lonejson_storage_kind {
  /** lonejson owns and allocates storage as needed. */
  LONEJSON_STORAGE_DYNAMIC = 0,
  /** Caller provides fixed-capacity storage inside the mapped struct. */
  LONEJSON_STORAGE_FIXED = 1
} lonejson_storage_kind;

/** Behavior used when a fixed-capacity output or mapped field is too small. */
typedef enum lonejson_overflow_policy {
  /** Treat overflow as an error and stop. */
  LONEJSON_OVERFLOW_FAIL = 0,
  /** Truncate but report `LONEJSON_STATUS_TRUNCATED`. */
  LONEJSON_OVERFLOW_TRUNCATE = 1,
  /** Truncate silently while still setting `error.truncated`. */
  LONEJSON_OVERFLOW_TRUNCATE_SILENT = 2
} lonejson_overflow_policy;

/** Detailed error information populated by most public APIs. */
typedef struct lonejson_error {
  /** Primary status code for the failure or warning. */
  lonejson_status code;
  /** 1-based input line number when relevant. */
  size_t line;
  /** 1-based input column number when relevant. */
  size_t column;
  /** 1-based byte offset into the current input stream or buffer. */
  size_t offset;
  /** `errno`-style code for I/O and callback-backed failures. */
  int system_errno;
  /** Non-zero when truncation occurred under a permissive overflow policy. */
  int truncated;
  /** Human-readable summary suitable for logs and diagnostics. */
  char message[160];
} lonejson_error;

/** Configuration used by streamed text/base64 fields backed by
 * `lonejson_spooled`. */
typedef struct lonejson_spool_options {
  /** Maximum bytes retained in memory before lonejson spills additional data to
   * a temporary file. */
  size_t memory_limit;
  /** Optional hard maximum logical size in bytes. Zero means unbounded. */
  size_t max_bytes;
  /** Optional temporary directory for named spool files. `NULL` uses the
   * platform default temporary-file mechanism. */
  const char *temp_dir;
} lonejson_spool_options;

/** Spill-backed storage used by streamed text and decoded byte fields.
 * Applications typically treat this as an opaque handle and interact through
 * the `lonejson_spooled_*` helpers. */
typedef struct lonejson_spooled {
  /** In-memory prefix retained before spill-to-disk. */
  unsigned char *memory;
  /** Number of bytes currently stored in `memory`. */
  size_t memory_len;
  /** Total logical bytes held by the handle across memory and any spill file.
   */
  size_t size;
  /** Current raw-read cursor used by `lonejson_spooled_read`. */
  size_t read_offset;
  /** Effective in-memory threshold applied to this handle. */
  size_t memory_limit;
  /** Effective hard maximum size applied to this handle. Zero means unbounded.
   */
  size_t max_bytes;
  /** Temporary directory associated with the handle, if any. */
  const char *temp_dir;
  /** Backing temporary file once the handle has spilled. */
  FILE *spill_fp;
  /** Non-zero once data has been written beyond the in-memory prefix. */
  int spilled;
  /** Named temporary path when the configured temp directory path is used.
   * Empty for anonymous temporary files. */
  char temp_path[LONEJSON_SPOOL_TEMP_PATH_CAPACITY];
} lonejson_spooled;

/** Serialize-only outbound source used by streamed text/base64 source fields.
 * Unlike `lonejson_spooled`, this handle never buffers payload bytes inside
 * lonejson; it streams directly from a file, fd, or reopenable path when a
 * mapped struct is serialized. */
typedef struct lonejson_source {
  /** Current configured source kind. `LONEJSON_SOURCE_NONE` emits `null`. */
  lonejson_source_kind kind;
  /** Caller-provided file handle when `kind == LONEJSON_SOURCE_FILE`. */
  FILE *fp;
  /** Caller-provided file descriptor when `kind == LONEJSON_SOURCE_FD`. */
  int fd;
  /** Owned filesystem path when `kind == LONEJSON_SOURCE_PATH`. */
  char *path;
} lonejson_source;

/** Dynamically sized array of NUL-terminated strings. */
typedef struct lonejson_string_array {
  /** Element storage. lonejson owns this buffer when `flags` contains
   * `LONEJSON_ARRAY_OWNS_ITEMS`. */
  char **items;
  /** Number of populated elements. */
  size_t count;
  /** Allocated or caller-supplied element capacity. */
  size_t capacity;
  /** Ownership and fixed-capacity flags maintained by lonejson. */
  unsigned flags;
} lonejson_string_array;

/** Dynamically sized array of `lonejson_int64` values. */
typedef struct lonejson_i64_array {
  lonejson_int64 *items;
  size_t count;
  size_t capacity;
  unsigned flags;
} lonejson_i64_array;

/** Dynamically sized array of `lonejson_uint64` values. */
typedef struct lonejson_u64_array {
  lonejson_uint64 *items;
  size_t count;
  size_t capacity;
  unsigned flags;
} lonejson_u64_array;

/** Dynamically sized array of `double` values. */
typedef struct lonejson_f64_array {
  double *items;
  size_t count;
  size_t capacity;
  unsigned flags;
} lonejson_f64_array;

/** Dynamically sized array of boolean values. */
typedef struct lonejson_bool_array {
  bool *items;
  size_t count;
  size_t capacity;
  unsigned flags;
} lonejson_bool_array;

/** Dynamically sized array of nested structs described by another map. */
typedef struct lonejson_object_array {
  /** Raw element storage. Cast to the mapped element type when consuming. */
  void *items;
  /** Number of populated elements. */
  size_t count;
  /** Allocated or caller-supplied element capacity. */
  size_t capacity;
  /** Size in bytes of each element struct. */
  size_t elem_size;
  /** Ownership and fixed-capacity flags maintained by lonejson. */
  unsigned flags;
} lonejson_object_array;

typedef struct lonejson_field lonejson_field;
typedef struct lonejson_map lonejson_map;
typedef struct lonejson_stream lonejson_stream;

/** Runtime metadata describing one mapped JSON field. Applications normally
 * use the `LONEJSON_FIELD_*` macros instead of writing this struct manually. */
struct lonejson_field {
  /** JSON object key matched against incoming data and emitted on serialize. */
  const char *json_key;
  /** Cached byte length of `json_key`. */
  size_t json_key_len;
  /** Cached first key byte used by the fast field matcher. */
  unsigned char json_key_first;
  /** Cached last key byte used by the fast field matcher. */
  unsigned char json_key_last;
  /** `offsetof(struct_type, member)` for the mapped destination member. */
  size_t struct_offset;
  /** Field category selected by the mapping macro. */
  lonejson_field_kind kind;
  /** Fixed versus dynamically allocated destination storage. */
  lonejson_storage_kind storage;
  /** Overflow policy for fixed-capacity strings and arrays. */
  lonejson_overflow_policy overflow_policy;
  /** Additional field flags such as `LONEJSON_FIELD_REQUIRED`. */
  unsigned flags;
  /** Fixed-capacity byte count for fixed strings, otherwise zero. */
  size_t fixed_capacity;
  /** Element size for object/number arrays when applicable. */
  size_t elem_size;
  /** Nested object map for object and object-array fields. */
  const lonejson_map *submap;
  /** Optional spool settings for streamed text and base64 fields. */
  const lonejson_spool_options *spool_options;
};

#define LONEJSON__KEY_LEN(key) (sizeof(key) - 1u)
#define LONEJSON__KEY_FIRST(key)                                               \
  ((unsigned char)((LONEJSON__KEY_LEN(key) != 0u) ? (key)[0] : '\0'))
#define LONEJSON__KEY_LAST(key)                                                \
  ((unsigned char)((LONEJSON__KEY_LEN(key) != 0u)                              \
                       ? (key)[LONEJSON__KEY_LEN(key) - 1u]                    \
                       : '\0'))

/** Top-level schema description used for mapped parse and serialize
 * operations. */
struct lonejson_map {
  /** Human-readable schema name, typically the C struct type name. */
  const char *name;
  /** Size in bytes of the mapped destination struct. */
  size_t struct_size;
  /** Field table built from `LONEJSON_FIELD_*` macro entries. */
  const lonejson_field *fields;
  /** Number of entries in `fields`. */
  size_t field_count;
};

/** Optional controls for mapped parsing and streaming. */
typedef struct lonejson_parse_options {
  /** When non-zero, lonejson initializes the destination before parsing. */
  int clear_destination;
  /** When non-zero, duplicate object keys cause
   * `LONEJSON_STATUS_DUPLICATE_FIELD`.
   */
  int reject_duplicate_keys;
  /** Maximum allowed nesting depth before parsing fails with overflow. */
  size_t max_depth;
} lonejson_parse_options;

/** Result returned by reader callbacks and `lonejson_spooled_read`. */
typedef struct lonejson_read_result {
  /** Number of bytes placed into the supplied buffer. */
  size_t bytes_read;
  /** Non-zero when the input source reached end-of-stream. */
  int eof;
  /** Non-zero when the source would block and the caller should retry later. */
  int would_block;
  /** `errno`-style failure code. Zero indicates success. */
  int error_code;
} lonejson_read_result;

/** Callback type used by reader-backed parse and stream APIs. */
typedef lonejson_read_result (*lonejson_reader_fn)(void *user,
                                                   unsigned char *buffer,
                                                   size_t capacity);

/** Options controlling serialization into buffers and sinks. */
typedef struct lonejson_write_options {
  /** Buffer overflow handling policy for fixed-capacity output buffers. */
  lonejson_overflow_policy overflow_policy;
  /** Non-zero to emit two-space-indented pretty JSON instead of compact JSON.
   */
  int pretty;
} lonejson_write_options;

/** Generic sink callback used by serializer APIs and raw spool writers. */
typedef lonejson_status (*lonejson_sink_fn)(void *user, const void *data,
                                            size_t len, lonejson_error *error);

/** Defines a static mapping table for a struct type from a separately declared
 * field array. */
#define LONEJSON_MAP_DEFINE(name, type, fields_array)                          \
  static const lonejson_map name = {#type, sizeof(type), fields_array,         \
                                    sizeof(fields_array) /                     \
                                        sizeof((fields_array)[0])}

/** Maps a JSON string field into a dynamically allocated `char *` member. */
#define LONEJSON_FIELD_STRING_ALLOC(type, member, key)                         \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING,                                                 \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a required JSON string field into a dynamically allocated `char *`
 * member. */
#define LONEJSON_FIELD_STRING_ALLOC_REQ(type, member, key)                     \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING,                                                 \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a JSON string field into a fixed-size character array member. */
#define LONEJSON_FIELD_STRING_FIXED(type, member, key, policy)                 \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING,                                                 \
   LONEJSON_STORAGE_FIXED,                                                     \
   policy,                                                                     \
   0u,                                                                         \
   sizeof(((type *)0)->member),                                                \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a required JSON string field into a fixed-size character array member.
 */
#define LONEJSON_FIELD_STRING_FIXED_REQ(type, member, key, policy)             \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING,                                                 \
   LONEJSON_STORAGE_FIXED,                                                     \
   policy,                                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   sizeof(((type *)0)->member),                                                \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a JSON string field into a `lonejson_spooled` member that keeps an
 * in-memory prefix and spills excess data to a temporary file using default
 * spool options. */
#define LONEJSON_FIELD_STRING_STREAM(type, member, key)                        \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a required JSON string field into a `lonejson_spooled` member using
 * default spool options. */
#define LONEJSON_FIELD_STRING_STREAM_REQ(type, member, key)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a JSON string field into a `lonejson_spooled` member that decodes
 * Base64 incrementally and stores the decoded bytes using default spool
 * options. */
#define LONEJSON_FIELD_BASE64_STREAM(type, member, key)                        \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a required Base64-decoded JSON string field into a `lonejson_spooled`
 * member using default spool options. */
#define LONEJSON_FIELD_BASE64_STREAM_REQ(type, member, key)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a JSON string field into a `lonejson_spooled` member using explicit
 * spool options. */
#define LONEJSON_FIELD_STRING_STREAM_OPTS(type, member, key, options_ptr)      \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   options_ptr}

/** Maps a serialize-only JSON string field into a `lonejson_source` member
 * that streams raw text bytes from a file, fd, or path at write time. */
#define LONEJSON_FIELD_STRING_SOURCE(type, member, key)                        \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_SOURCE,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a required serialize-only JSON string field into a `lonejson_source`
 * member that streams raw text bytes at write time. */
#define LONEJSON_FIELD_STRING_SOURCE_REQ(type, member, key)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_SOURCE,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a serialize-only JSON string field into a `lonejson_source` member
 * that base64-encodes raw bytes from a file, fd, or path at write time. */
#define LONEJSON_FIELD_BASE64_SOURCE(type, member, key)                        \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_SOURCE,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a required serialize-only Base64 JSON string field into a
 * `lonejson_source` member that streams raw bytes at write time. */
#define LONEJSON_FIELD_BASE64_SOURCE_REQ(type, member, key)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_SOURCE,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a Base64-decoded JSON string field into a `lonejson_spooled` member
 * using explicit spool options. */
#define LONEJSON_FIELD_BASE64_STREAM_OPTS(type, member, key, options_ptr)      \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BASE64_STREAM,                                          \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   options_ptr}

/** Maps a JSON integer field into a `lonejson_int64` member. */
#define LONEJSON_FIELD_I64(type, member, key)                                  \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_I64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a required JSON integer field into a `lonejson_int64` member. */
#define LONEJSON_FIELD_I64_REQ(type, member, key)                              \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_I64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a JSON unsigned integer field into a `lonejson_uint64` member. */
#define LONEJSON_FIELD_U64(type, member, key)                                  \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_U64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a required JSON unsigned integer field into a `lonejson_uint64` member.
 */
#define LONEJSON_FIELD_U64_REQ(type, member, key)                              \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_U64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a JSON number field into a `double` member. */
#define LONEJSON_FIELD_F64(type, member, key)                                  \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_F64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a required JSON number field into a `double` member. */
#define LONEJSON_FIELD_F64_REQ(type, member, key)                              \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_F64,                                                    \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a JSON boolean field into a `bool` member. */
#define LONEJSON_FIELD_BOOL(type, member, key)                                 \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BOOL,                                                   \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a required JSON boolean field into a `bool` member. */
#define LONEJSON_FIELD_BOOL_REQ(type, member, key)                             \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BOOL,                                                   \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a nested JSON object into a nested struct member using another mapping
 * table. */
#define LONEJSON_FIELD_OBJECT(type, member, key, submap_ptr)                   \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_OBJECT,                                                 \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   submap_ptr,                                                                 \
   NULL}

/** Maps a required nested JSON object into a nested struct member using another
 * mapping table. */
#define LONEJSON_FIELD_OBJECT_REQ(type, member, key, submap_ptr)               \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_OBJECT,                                                 \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   LONEJSON_FIELD_REQUIRED,                                                    \
   0u,                                                                         \
   0u,                                                                         \
   submap_ptr,                                                                 \
   NULL}

/** Maps a JSON array of strings into a `lonejson_string_array` member. */
#define LONEJSON_FIELD_STRING_ARRAY(type, member, key, policy)                 \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_STRING_ARRAY,                                           \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a JSON array of integers into a `lonejson_i64_array` member. */
#define LONEJSON_FIELD_I64_ARRAY(type, member, key, policy)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_I64_ARRAY,                                              \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   0u,                                                                         \
   0u,                                                                         \
   sizeof(lonejson_int64),                                                     \
   NULL,                                                                       \
   NULL}

/** Maps a JSON array of unsigned integers into a `lonejson_u64_array` member.
 */
#define LONEJSON_FIELD_U64_ARRAY(type, member, key, policy)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_U64_ARRAY,                                              \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   0u,                                                                         \
   0u,                                                                         \
   sizeof(lonejson_uint64),                                                    \
   NULL,                                                                       \
   NULL}

/** Maps a JSON array of numbers into a `lonejson_f64_array` member. */
#define LONEJSON_FIELD_F64_ARRAY(type, member, key, policy)                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_F64_ARRAY,                                              \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   0u,                                                                         \
   0u,                                                                         \
   sizeof(double),                                                             \
   NULL,                                                                       \
   NULL}

/** Maps a JSON array of booleans into a `lonejson_bool_array` member. */
#define LONEJSON_FIELD_BOOL_ARRAY(type, member, key, policy)                   \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_BOOL_ARRAY,                                             \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   0u,                                                                         \
   0u,                                                                         \
   sizeof(bool),                                                               \
   NULL,                                                                       \
   NULL}

/** Maps a JSON array of nested objects into a `lonejson_object_array` member.
 */
#define LONEJSON_FIELD_OBJECT_ARRAY(type, member, key, elem_type, submap_ptr,  \
                                    policy)                                    \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_OBJECT_ARRAY,                                           \
   LONEJSON_STORAGE_DYNAMIC,                                                   \
   policy,                                                                     \
   0u,                                                                         \
   0u,                                                                         \
   sizeof(elem_type),                                                          \
   submap_ptr,                                                                 \
   NULL}

/** Returns the library's default spool options. */
lonejson_spool_options lonejson_default_spool_options(void);
/** Initializes a spool handle to its default or caller-configured empty state.
 */
void lonejson_spooled_init(lonejson_spooled *value,
                           const lonejson_spool_options *options);
/** Clears a spool handle while preserving its configured thresholds. */
void lonejson_spooled_reset(lonejson_spooled *value);
/** Releases all resources owned by a spool handle, including any temporary
 * file. */
void lonejson_spooled_cleanup(lonejson_spooled *value);
/** Returns the logical byte size of a spooled value. */
size_t lonejson_spooled_size(const lonejson_spooled *value);
/** Returns non-zero when a spooled value has spilled beyond memory into a
 * temporary file. */
int lonejson_spooled_spilled(const lonejson_spooled *value);
/** Rewinds a spooled value's raw read cursor to the beginning. */
lonejson_status lonejson_spooled_rewind(lonejson_spooled *value,
                                        lonejson_error *error);
/** Reads raw bytes sequentially from a spooled value. */
lonejson_read_result lonejson_spooled_read(lonejson_spooled *value,
                                           unsigned char *buffer,
                                           size_t capacity);
/** Streams the raw bytes of a spooled value to a generic sink callback. */
lonejson_status lonejson_spooled_write_to_sink(const lonejson_spooled *value,
                                               lonejson_sink_fn sink,
                                               void *user,
                                               lonejson_error *error);
/** Initializes an outbound source handle to the empty `null` state. */
void lonejson_source_init(lonejson_source *value);
/** Resets an outbound source handle to the empty `null` state. */
void lonejson_source_reset(lonejson_source *value);
/** Releases any path owned by an outbound source handle and resets it. */
void lonejson_source_cleanup(lonejson_source *value);
/** Configures an outbound source handle to read from a caller-owned `FILE *`.
 */
lonejson_status lonejson_source_set_file(lonejson_source *value, FILE *fp,
                                         lonejson_error *error);
/** Configures an outbound source handle to read from a caller-owned file
 * descriptor. */
lonejson_status lonejson_source_set_fd(lonejson_source *value, int fd,
                                       lonejson_error *error);
/** Configures an outbound source handle to reopen and stream a filesystem
 * path. lonejson owns the duplicated path string and frees it on cleanup. */
lonejson_status lonejson_source_set_path(lonejson_source *value,
                                         const char *path,
                                         lonejson_error *error);
/** Streams the raw bytes of an outbound source handle to a generic sink. Path
 * sources are opened and closed by lonejson for the duration of the call. */
lonejson_status lonejson_source_write_to_sink(const lonejson_source *value,
                                              lonejson_sink_fn sink, void *user,
                                              lonejson_error *error);

/** Returns the library's default parse options. */
lonejson_parse_options lonejson_default_parse_options(void);
/** Returns the library's default write options. */
lonejson_write_options lonejson_default_write_options(void);
/** Returns a stable string name for a `lonejson_status` value. */
const char *lonejson_status_string(lonejson_status status);

/** Result produced by `lonejson_stream_next`. */
typedef enum lonejson_stream_result {
  /** One complete top-level object was parsed into the destination. */
  LONEJSON_STREAM_OBJECT = 0,
  /** No more objects remain in the underlying stream. */
  LONEJSON_STREAM_EOF,
  /** The underlying source would block before a full object was available. */
  LONEJSON_STREAM_WOULD_BLOCK,
  /** A parse, I/O, or argument error occurred. Inspect `lonejson_stream_error`.
   */
  LONEJSON_STREAM_ERROR
} lonejson_stream_result;

/** Opens an object-framed JSON stream over a caller-supplied reader callback.
 */
lonejson_stream *
lonejson_stream_open_reader(const lonejson_map *map, lonejson_reader_fn reader,
                            void *user, const lonejson_parse_options *options,
                            lonejson_error *error);
/** Opens an object-framed JSON stream over an open `FILE *`. */
lonejson_stream *
lonejson_stream_open_filep(const lonejson_map *map, FILE *fp,
                           const lonejson_parse_options *options,
                           lonejson_error *error);
/** Opens an object-framed JSON stream over a filesystem path. */
lonejson_stream *
lonejson_stream_open_path(const lonejson_map *map, const char *path,
                          const lonejson_parse_options *options,
                          lonejson_error *error);
/** Opens an object-framed JSON stream over a file descriptor, including Unix
 * domain sockets. */
lonejson_stream *lonejson_stream_open_fd(const lonejson_map *map, int fd,
                                         const lonejson_parse_options *options,
                                         lonejson_error *error);
/** Parses the next top-level JSON object from a stream into `dst`. Whitespace
 * between objects is ignored and EOF after the last object is reported
 * separately. */
lonejson_stream_result lonejson_stream_next(lonejson_stream *stream, void *dst,
                                            lonejson_error *error);
/** Returns the stream's last error state. */
const lonejson_error *lonejson_stream_error(const lonejson_stream *stream);
/** Closes a stream and releases resources it owns. */
void lonejson_stream_close(lonejson_stream *stream);

/** Parses a JSON buffer into a mapped struct. */
lonejson_status lonejson_parse_buffer(const lonejson_map *map, void *dst,
                                      const void *data, size_t len,
                                      const lonejson_parse_options *options,
                                      lonejson_error *error);
/** Parses a NUL-terminated JSON string into a mapped struct. */
lonejson_status lonejson_parse_cstr(const lonejson_map *map, void *dst,
                                    const char *json,
                                    const lonejson_parse_options *options,
                                    lonejson_error *error);
/** Parses JSON from a caller-supplied reader callback into a mapped struct. */
lonejson_status lonejson_parse_reader(const lonejson_map *map, void *dst,
                                      lonejson_reader_fn reader, void *user,
                                      const lonejson_parse_options *options,
                                      lonejson_error *error);
/** Parses JSON from an open `FILE *` stream into a mapped struct. */
lonejson_status lonejson_parse_filep(const lonejson_map *map, void *dst,
                                     FILE *fp,
                                     const lonejson_parse_options *options,
                                     lonejson_error *error);
/** Parses JSON from a filesystem path into a mapped struct. */
lonejson_status lonejson_parse_path(const lonejson_map *map, void *dst,
                                    const char *path,
                                    const lonejson_parse_options *options,
                                    lonejson_error *error);

/** Validates that a buffer contains a syntactically valid JSON document. */
lonejson_status lonejson_validate_buffer(const void *data, size_t len,
                                         lonejson_error *error);
/** Validates that a NUL-terminated string contains syntactically valid JSON. */
lonejson_status lonejson_validate_cstr(const char *json, lonejson_error *error);
/** Validates JSON produced by a caller-supplied reader callback. */
lonejson_status lonejson_validate_reader(lonejson_reader_fn reader, void *user,
                                         lonejson_error *error);
/** Validates JSON from an open `FILE *` stream. */
lonejson_status lonejson_validate_filep(FILE *fp, lonejson_error *error);
/** Validates JSON from a filesystem path. */
lonejson_status lonejson_validate_path(const char *path, lonejson_error *error);

/** Serializes a mapped struct to a generic output sink callback. */
lonejson_status lonejson_serialize_sink(const lonejson_map *map,
                                        const void *src, lonejson_sink_fn sink,
                                        void *user,
                                        const lonejson_write_options *options,
                                        lonejson_error *error);
/** Serializes a mapped struct into a caller-provided output buffer. */
lonejson_status lonejson_serialize_buffer(const lonejson_map *map,
                                          const void *src, char *buffer,
                                          size_t capacity, size_t *needed,
                                          const lonejson_write_options *options,
                                          lonejson_error *error);
/** Serializes a mapped struct into a newly allocated JSON string. */
char *lonejson_serialize_alloc(const lonejson_map *map, const void *src,
                               size_t *out_len,
                               const lonejson_write_options *options,
                               lonejson_error *error);
/** Serializes a mapped struct to an open `FILE *`. */
lonejson_status lonejson_serialize_filep(const lonejson_map *map,
                                         const void *src, FILE *fp,
                                         const lonejson_write_options *options,
                                         lonejson_error *error);
/** Serializes a mapped struct to a filesystem path. */
lonejson_status lonejson_serialize_path(const lonejson_map *map,
                                        const void *src, const char *path,
                                        const lonejson_write_options *options,
                                        lonejson_error *error);
/** Serializes an array of mapped structs as JSONL records to a generic output
 * sink callback. Uses compact output by default, or two-space pretty printing
 * when `options->pretty` is non-zero. `stride` defaults to `map->struct_size`
 * when set to zero. */
lonejson_status lonejson_serialize_jsonl_sink(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    lonejson_sink_fn sink, void *user, const lonejson_write_options *options,
    lonejson_error *error);
/** Serializes an array of mapped structs as JSONL records into a
 * caller-provided output buffer. Uses compact output by default, or two-space
 * pretty printing when `options->pretty` is non-zero. `stride` defaults to
 * `map->struct_size` when set to zero. */
lonejson_status lonejson_serialize_jsonl_buffer(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    char *buffer, size_t capacity, size_t *needed,
    const lonejson_write_options *options, lonejson_error *error);
/** Serializes an array of mapped structs as JSONL records into a newly
 * allocated string. Uses compact output by default, or two-space pretty
 * printing when `options->pretty` is non-zero. `stride` defaults to
 * `map->struct_size` when set to zero. */
char *lonejson_serialize_jsonl_alloc(const lonejson_map *map, const void *items,
                                     size_t count, size_t stride,
                                     size_t *out_len,
                                     const lonejson_write_options *options,
                                     lonejson_error *error);
/** Serializes an array of mapped structs as JSONL records to an open `FILE *`.
 * Uses compact output by default, or two-space pretty printing when
 * `options->pretty` is non-zero. `stride` defaults to `map->struct_size` when
 * set to zero. */
lonejson_status lonejson_serialize_jsonl_filep(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    FILE *fp, const lonejson_write_options *options, lonejson_error *error);
/** Serializes an array of mapped structs as JSONL records to a filesystem
 * path. Uses compact output by default, or two-space pretty printing when
 * `options->pretty` is non-zero. `stride` defaults to `map->struct_size` when
 * set to zero. */
lonejson_status
lonejson_serialize_jsonl_path(const lonejson_map *map, const void *items,
                              size_t count, size_t stride, const char *path,
                              const lonejson_write_options *options,
                              lonejson_error *error);

/** Frees dynamic storage owned by a mapped value. */
void lonejson_cleanup(const lonejson_map *map, void *value);
/** Clears a mapped value while preserving caller-owned fixed-capacity array
 * backing storage. */
void lonejson_reset(const lonejson_map *map, void *value);

#ifdef LONEJSON_WITH_CURL
#include <curl/curl.h>

typedef struct lonejson_curl_parse {
  void *parser;
  lonejson_error error;
} lonejson_curl_parse;

typedef struct lonejson_curl_upload {
  char *json;
  size_t length;
  size_t offset;
  lonejson_error error;
} lonejson_curl_upload;

/** Initializes a curl parse adapter suitable for `CURLOPT_WRITEFUNCTION`. */
lonejson_status lonejson_curl_parse_init(lonejson_curl_parse *ctx,
                                         const lonejson_map *map, void *dst,
                                         const lonejson_parse_options *options);
/** Curl write callback that incrementally feeds response bytes to lonejson. */
size_t lonejson_curl_write_callback(char *ptr, size_t size, size_t nmemb,
                                    void *userdata);
/** Finalizes a curl-backed parse adapter. */
lonejson_status lonejson_curl_parse_finish(lonejson_curl_parse *ctx);
/** Releases resources owned by a curl parse adapter. */
void lonejson_curl_parse_cleanup(lonejson_curl_parse *ctx);

/** Initializes a curl upload adapter for a mapped struct. */
lonejson_status
lonejson_curl_upload_init(lonejson_curl_upload *ctx, const lonejson_map *map,
                          const void *src,
                          const lonejson_write_options *options);
/** Curl read callback that serves serialized JSON bytes to libcurl. */
size_t lonejson_curl_read_callback(char *ptr, size_t size, size_t nmemb,
                                   void *userdata);
/** Returns the upload size reported by a curl upload adapter. */
curl_off_t lonejson_curl_upload_size(const lonejson_curl_upload *ctx);
/** Releases resources owned by a curl upload adapter. */
void lonejson_curl_upload_cleanup(lonejson_curl_upload *ctx);
#endif

#ifndef LONEJSON_DISABLE_SHORT_NAMES
/** Short-name compatibility aliases. Enabled by default. Define
 * `LONEJSON_DISABLE_SHORT_NAMES` or `LJ_DISABLE_SHORT_NAMES` before including
 * this header to expose only the long `lonejson_*` / `LONEJSON_*` names. */
#define LJ_VERSION_MAJOR LONEJSON_VERSION_MAJOR
#define LJ_VERSION_MINOR LONEJSON_VERSION_MINOR
#define LJ_VERSION_PATCH LONEJSON_VERSION_PATCH
#define LJ_FIELD_REQUIRED LONEJSON_FIELD_REQUIRED
#define LJ_ARRAY_OWNS_ITEMS LONEJSON_ARRAY_OWNS_ITEMS
#define LJ_ARRAY_FIXED_CAPACITY LONEJSON_ARRAY_FIXED_CAPACITY
#define LJ_MALLOC LONEJSON_MALLOC
#define LJ_CALLOC LONEJSON_CALLOC
#define LJ_REALLOC LONEJSON_REALLOC
#define LJ_FREE LONEJSON_FREE
#define LJ_PARSER_BUFFER_SIZE LONEJSON_PARSER_BUFFER_SIZE
#define LJ_READER_BUFFER_SIZE LONEJSON_READER_BUFFER_SIZE
#define LJ_PUSH_PARSER_BUFFER_SIZE LONEJSON_PUSH_PARSER_BUFFER_SIZE
#define LJ_STREAM_BUFFER_SIZE LONEJSON_STREAM_BUFFER_SIZE
#define LJ_SPOOL_MEMORY_LIMIT LONEJSON_SPOOL_MEMORY_LIMIT
#define LJ_SPOOL_TEMP_PATH_CAPACITY LONEJSON_SPOOL_TEMP_PATH_CAPACITY
#define LJ_TRACK_WORKSPACE_USAGE LONEJSON_TRACK_WORKSPACE_USAGE

#define LJ_STATUS_OK LONEJSON_STATUS_OK
#define LJ_STATUS_INVALID_ARGUMENT LONEJSON_STATUS_INVALID_ARGUMENT
#define LJ_STATUS_INVALID_JSON LONEJSON_STATUS_INVALID_JSON
#define LJ_STATUS_TYPE_MISMATCH LONEJSON_STATUS_TYPE_MISMATCH
#define LJ_STATUS_MISSING_REQUIRED_FIELD LONEJSON_STATUS_MISSING_REQUIRED_FIELD
#define LJ_STATUS_DUPLICATE_FIELD LONEJSON_STATUS_DUPLICATE_FIELD
#define LJ_STATUS_OVERFLOW LONEJSON_STATUS_OVERFLOW
#define LJ_STATUS_TRUNCATED LONEJSON_STATUS_TRUNCATED
#define LJ_STATUS_ALLOCATION_FAILED LONEJSON_STATUS_ALLOCATION_FAILED
#define LJ_STATUS_CALLBACK_FAILED LONEJSON_STATUS_CALLBACK_FAILED
#define LJ_STATUS_IO_ERROR LONEJSON_STATUS_IO_ERROR
#define LJ_STATUS_INTERNAL_ERROR LONEJSON_STATUS_INTERNAL_ERROR

#define LJ_FIELD_KIND_STRING LONEJSON_FIELD_KIND_STRING
#define LJ_FIELD_KIND_STRING_STREAM LONEJSON_FIELD_KIND_STRING_STREAM
#define LJ_FIELD_KIND_BASE64_STREAM LONEJSON_FIELD_KIND_BASE64_STREAM
#define LJ_FIELD_KIND_STRING_SOURCE LONEJSON_FIELD_KIND_STRING_SOURCE
#define LJ_FIELD_KIND_BASE64_SOURCE LONEJSON_FIELD_KIND_BASE64_SOURCE
#define LJ_FIELD_KIND_I64 LONEJSON_FIELD_KIND_I64
#define LJ_FIELD_KIND_U64 LONEJSON_FIELD_KIND_U64
#define LJ_FIELD_KIND_F64 LONEJSON_FIELD_KIND_F64
#define LJ_FIELD_KIND_BOOL LONEJSON_FIELD_KIND_BOOL
#define LJ_FIELD_KIND_OBJECT LONEJSON_FIELD_KIND_OBJECT
#define LJ_FIELD_KIND_STRING_ARRAY LONEJSON_FIELD_KIND_STRING_ARRAY
#define LJ_FIELD_KIND_I64_ARRAY LONEJSON_FIELD_KIND_I64_ARRAY
#define LJ_FIELD_KIND_U64_ARRAY LONEJSON_FIELD_KIND_U64_ARRAY
#define LJ_FIELD_KIND_F64_ARRAY LONEJSON_FIELD_KIND_F64_ARRAY
#define LJ_FIELD_KIND_BOOL_ARRAY LONEJSON_FIELD_KIND_BOOL_ARRAY
#define LJ_FIELD_KIND_OBJECT_ARRAY LONEJSON_FIELD_KIND_OBJECT_ARRAY

#define LJ_SOURCE_NONE LONEJSON_SOURCE_NONE
#define LJ_SOURCE_FILE LONEJSON_SOURCE_FILE
#define LJ_SOURCE_FD LONEJSON_SOURCE_FD
#define LJ_SOURCE_PATH LONEJSON_SOURCE_PATH

#define LJ_STORAGE_DYNAMIC LONEJSON_STORAGE_DYNAMIC
#define LJ_STORAGE_FIXED LONEJSON_STORAGE_FIXED

#define LJ_OVERFLOW_FAIL LONEJSON_OVERFLOW_FAIL
#define LJ_OVERFLOW_TRUNCATE LONEJSON_OVERFLOW_TRUNCATE
#define LJ_OVERFLOW_TRUNCATE_SILENT LONEJSON_OVERFLOW_TRUNCATE_SILENT

#define LJ_STREAM_OBJECT LONEJSON_STREAM_OBJECT
#define LJ_STREAM_EOF LONEJSON_STREAM_EOF
#define LJ_STREAM_WOULD_BLOCK LONEJSON_STREAM_WOULD_BLOCK
#define LJ_STREAM_ERROR LONEJSON_STREAM_ERROR

#define LJ_MAP_DEFINE LONEJSON_MAP_DEFINE
#define LJ_FIELD_STRING_ALLOC LONEJSON_FIELD_STRING_ALLOC
#define LJ_FIELD_STRING_ALLOC_REQ LONEJSON_FIELD_STRING_ALLOC_REQ
#define LJ_FIELD_STRING_FIXED LONEJSON_FIELD_STRING_FIXED
#define LJ_FIELD_STRING_FIXED_REQ LONEJSON_FIELD_STRING_FIXED_REQ
#define LJ_FIELD_STRING_STREAM LONEJSON_FIELD_STRING_STREAM
#define LJ_FIELD_STRING_STREAM_REQ LONEJSON_FIELD_STRING_STREAM_REQ
#define LJ_FIELD_BASE64_STREAM LONEJSON_FIELD_BASE64_STREAM
#define LJ_FIELD_BASE64_STREAM_REQ LONEJSON_FIELD_BASE64_STREAM_REQ
#define LJ_FIELD_STRING_STREAM_OPTS LONEJSON_FIELD_STRING_STREAM_OPTS
#define LJ_FIELD_STRING_SOURCE LONEJSON_FIELD_STRING_SOURCE
#define LJ_FIELD_STRING_SOURCE_REQ LONEJSON_FIELD_STRING_SOURCE_REQ
#define LJ_FIELD_BASE64_SOURCE LONEJSON_FIELD_BASE64_SOURCE
#define LJ_FIELD_BASE64_SOURCE_REQ LONEJSON_FIELD_BASE64_SOURCE_REQ
#define LJ_FIELD_BASE64_STREAM_OPTS LONEJSON_FIELD_BASE64_STREAM_OPTS
#define LJ_FIELD_I64 LONEJSON_FIELD_I64
#define LJ_FIELD_I64_REQ LONEJSON_FIELD_I64_REQ
#define LJ_FIELD_U64 LONEJSON_FIELD_U64
#define LJ_FIELD_U64_REQ LONEJSON_FIELD_U64_REQ
#define LJ_FIELD_F64 LONEJSON_FIELD_F64
#define LJ_FIELD_F64_REQ LONEJSON_FIELD_F64_REQ
#define LJ_FIELD_BOOL LONEJSON_FIELD_BOOL
#define LJ_FIELD_BOOL_REQ LONEJSON_FIELD_BOOL_REQ
#define LJ_FIELD_OBJECT LONEJSON_FIELD_OBJECT
#define LJ_FIELD_OBJECT_REQ LONEJSON_FIELD_OBJECT_REQ
#define LJ_FIELD_STRING_ARRAY LONEJSON_FIELD_STRING_ARRAY
#define LJ_FIELD_I64_ARRAY LONEJSON_FIELD_I64_ARRAY
#define LJ_FIELD_U64_ARRAY LONEJSON_FIELD_U64_ARRAY
#define LJ_FIELD_F64_ARRAY LONEJSON_FIELD_F64_ARRAY
#define LJ_FIELD_BOOL_ARRAY LONEJSON_FIELD_BOOL_ARRAY
#define LJ_FIELD_OBJECT_ARRAY LONEJSON_FIELD_OBJECT_ARRAY

/** Short alias of `lonejson_uint32`. */
typedef lonejson_uint32 lj_uint32;
/** Short alias of `lonejson_int64`. */
typedef lonejson_int64 lj_int64;
/** Short alias of `lonejson_uint64`. */
typedef lonejson_uint64 lj_uint64;
/** Short alias of `lonejson_status`. */
typedef lonejson_status lj_status;
/** Short alias of `lonejson_field_kind`. */
typedef lonejson_field_kind lj_field_kind;
/** Short alias of `lonejson_source_kind`. */
typedef lonejson_source_kind lj_source_kind;
/** Short alias of `lonejson_storage_kind`. */
typedef lonejson_storage_kind lj_storage_kind;
/** Short alias of `lonejson_overflow_policy`. */
typedef lonejson_overflow_policy lj_overflow_policy;
/** Short alias of `lonejson_error`. */
typedef lonejson_error lj_error;
/** Short alias of `lonejson_spool_options`. */
typedef lonejson_spool_options lj_spool_options;
/** Short alias of `lonejson_spooled`. */
typedef lonejson_spooled lj_spooled;
/** Short alias of `lonejson_source`. */
typedef lonejson_source lj_source;
/** Short alias of `lonejson_string_array`. */
typedef lonejson_string_array lj_string_array;
/** Short alias of `lonejson_i64_array`. */
typedef lonejson_i64_array lj_i64_array;
/** Short alias of `lonejson_u64_array`. */
typedef lonejson_u64_array lj_u64_array;
/** Short alias of `lonejson_f64_array`. */
typedef lonejson_f64_array lj_f64_array;
/** Short alias of `lonejson_bool_array`. */
typedef lonejson_bool_array lj_bool_array;
/** Short alias of `lonejson_object_array`. */
typedef lonejson_object_array lj_object_array;
/** Short alias of `lonejson_field`. */
typedef lonejson_field lj_field;
/** Short alias of `lonejson_map`. */
typedef lonejson_map lj_map;
/** Short alias of `lonejson_stream`. */
typedef lonejson_stream lj_stream;
/** Short alias of `lonejson_parse_options`. */
typedef lonejson_parse_options lj_parse_options;
/** Short alias of `lonejson_read_result`. */
typedef lonejson_read_result lj_read_result;
/** Short alias of `lonejson_reader_fn`. */
typedef lonejson_reader_fn lj_reader_fn;
/** Short alias of `lonejson_write_options`. */
typedef lonejson_write_options lj_write_options;
/** Short alias of `lonejson_sink_fn`. */
typedef lonejson_sink_fn lj_sink_fn;
/** Short alias of `lonejson_stream_result`. */
typedef lonejson_stream_result lj_stream_result;

/** Short alias of `lonejson_default_spool_options`. */
LONEJSON_SHORT_ALIAS_INLINE lj_spool_options lj_default_spool_options(void) {
  return lonejson_default_spool_options();
}
/** Short alias of `lonejson_spooled_init`. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_spooled_init(lj_spooled *value, const lj_spool_options *options) {
  lonejson_spooled_init(value, options);
}
/** Short alias of `lonejson_spooled_reset`. */
LONEJSON_SHORT_ALIAS_INLINE void lj_spooled_reset(lj_spooled *value) {
  lonejson_spooled_reset(value);
}
/** Short alias of `lonejson_spooled_cleanup`. */
LONEJSON_SHORT_ALIAS_INLINE void lj_spooled_cleanup(lj_spooled *value) {
  lonejson_spooled_cleanup(value);
}
/** Short alias of `lonejson_spooled_size`. */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_spooled_size(const lj_spooled *value) {
  return lonejson_spooled_size(value);
}
/** Short alias of `lonejson_spooled_spilled`. */
LONEJSON_SHORT_ALIAS_INLINE int lj_spooled_spilled(const lj_spooled *value) {
  return lonejson_spooled_spilled(value);
}
/** Short alias of `lonejson_spooled_rewind`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_spooled_rewind(lj_spooled *value,
                                                        lj_error *error) {
  return lonejson_spooled_rewind(value, error);
}
/** Short alias of `lonejson_spooled_read`. */
LONEJSON_SHORT_ALIAS_INLINE lj_read_result
lj_spooled_read(lj_spooled *value, unsigned char *buffer, size_t capacity) {
  return lonejson_spooled_read(value, buffer, capacity);
}
/** Short alias of `lonejson_spooled_write_to_sink`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_spooled_write_to_sink(
    const lj_spooled *value, lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_spooled_write_to_sink(value, sink, user, error);
}
/** Short alias of `lonejson_source_init`. */
LONEJSON_SHORT_ALIAS_INLINE void lj_source_init(lj_source *value) {
  lonejson_source_init(value);
}
/** Short alias of `lonejson_source_reset`. */
LONEJSON_SHORT_ALIAS_INLINE void lj_source_reset(lj_source *value) {
  lonejson_source_reset(value);
}
/** Short alias of `lonejson_source_cleanup`. */
LONEJSON_SHORT_ALIAS_INLINE void lj_source_cleanup(lj_source *value) {
  lonejson_source_cleanup(value);
}
/** Short alias of `lonejson_source_set_file`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_source_set_file(lj_source *value,
                                                         FILE *fp,
                                                         lj_error *error) {
  return lonejson_source_set_file(value, fp, error);
}
/** Short alias of `lonejson_source_set_fd`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_source_set_fd(lj_source *value, int fd,
                                                       lj_error *error) {
  return lonejson_source_set_fd(value, fd, error);
}
/** Short alias of `lonejson_source_set_path`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_source_set_path(lj_source *value,
                                                         const char *path,
                                                         lj_error *error) {
  return lonejson_source_set_path(value, path, error);
}
/** Short alias of `lonejson_source_write_to_sink`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_source_write_to_sink(
    const lj_source *value, lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_source_write_to_sink(value, sink, user, error);
}
/** Short alias of `lonejson_default_parse_options`. */
LONEJSON_SHORT_ALIAS_INLINE lj_parse_options lj_default_parse_options(void) {
  return lonejson_default_parse_options();
}
/** Short alias of `lonejson_default_write_options`. */
LONEJSON_SHORT_ALIAS_INLINE lj_write_options lj_default_write_options(void) {
  return lonejson_default_write_options();
}
/** Short alias of `lonejson_status_string`. */
LONEJSON_SHORT_ALIAS_INLINE const char *lj_status_string(lj_status status) {
  return lonejson_status_string(status);
}
/** Short alias of `lonejson_stream_open_reader`. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream *
lj_stream_open_reader(const lj_map *map, lj_reader_fn reader, void *user,
                      const lj_parse_options *options, lj_error *error) {
  return lonejson_stream_open_reader(map, reader, user, options, error);
}
/** Short alias of `lonejson_stream_open_filep`. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream *
lj_stream_open_filep(const lj_map *map, FILE *fp,
                     const lj_parse_options *options, lj_error *error) {
  return lonejson_stream_open_filep(map, fp, options, error);
}
/** Short alias of `lonejson_stream_open_path`. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream *
lj_stream_open_path(const lj_map *map, const char *path,
                    const lj_parse_options *options, lj_error *error) {
  return lonejson_stream_open_path(map, path, options, error);
}
/** Short alias of `lonejson_stream_open_fd`. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream *
lj_stream_open_fd(const lj_map *map, int fd, const lj_parse_options *options,
                  lj_error *error) {
  return lonejson_stream_open_fd(map, fd, options, error);
}
/** Short alias of `lonejson_stream_next`. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream_result lj_stream_next(lj_stream *stream,
                                                            void *dst,
                                                            lj_error *error) {
  return lonejson_stream_next(stream, dst, error);
}
/** Short alias of `lonejson_stream_error`. */
LONEJSON_SHORT_ALIAS_INLINE const lj_error *
lj_stream_error(const lj_stream *stream) {
  return lonejson_stream_error(stream);
}
/** Short alias of `lonejson_stream_close`. */
LONEJSON_SHORT_ALIAS_INLINE void lj_stream_close(lj_stream *stream) {
  lonejson_stream_close(stream);
}
/** Short alias of `lonejson_parse_buffer`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_parse_buffer(const lj_map *map, void *dst, const void *data, size_t len,
                const lj_parse_options *options, lj_error *error) {
  return lonejson_parse_buffer(map, dst, data, len, options, error);
}
/** Short alias of `lonejson_parse_cstr`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_parse_cstr(const lj_map *map, void *dst, const char *json,
              const lj_parse_options *options, lj_error *error) {
  return lonejson_parse_cstr(map, dst, json, options, error);
}
/** Short alias of `lonejson_parse_reader`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_parse_reader(const lj_map *map, void *dst, lj_reader_fn reader, void *user,
                const lj_parse_options *options, lj_error *error) {
  return lonejson_parse_reader(map, dst, reader, user, options, error);
}
/** Short alias of `lonejson_parse_filep`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_parse_filep(const lj_map *map, void *dst, FILE *fp,
               const lj_parse_options *options, lj_error *error) {
  return lonejson_parse_filep(map, dst, fp, options, error);
}
/** Short alias of `lonejson_parse_path`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_parse_path(const lj_map *map, void *dst, const char *path,
              const lj_parse_options *options, lj_error *error) {
  return lonejson_parse_path(map, dst, path, options, error);
}
/** Short alias of `lonejson_validate_buffer`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_buffer(const void *data,
                                                         size_t len,
                                                         lj_error *error) {
  return lonejson_validate_buffer(data, len, error);
}
/** Short alias of `lonejson_validate_cstr`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_cstr(const char *json,
                                                       lj_error *error) {
  return lonejson_validate_cstr(json, error);
}
/** Short alias of `lonejson_validate_reader`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_reader(lj_reader_fn reader,
                                                         void *user,
                                                         lj_error *error) {
  return lonejson_validate_reader(reader, user, error);
}
/** Short alias of `lonejson_validate_filep`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_filep(FILE *fp,
                                                        lj_error *error) {
  return lonejson_validate_filep(fp, error);
}
/** Short alias of `lonejson_validate_path`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_path(const char *path,
                                                       lj_error *error) {
  return lonejson_validate_path(path, error);
}
/** Short alias of `lonejson_serialize_sink`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_sink(
    const lj_map *map, const void *src, lj_sink_fn sink, void *user,
    const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_sink(map, src, sink, user, options, error);
}
/** Short alias of `lonejson_serialize_buffer`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_buffer(
    const lj_map *map, const void *src, char *buffer, size_t capacity,
    size_t *needed, const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_buffer(map, src, buffer, capacity, needed, options,
                                   error);
}
/** Short alias of `lonejson_serialize_alloc`. */
LONEJSON_SHORT_ALIAS_INLINE char *
lj_serialize_alloc(const lj_map *map, const void *src, size_t *out_len,
                   const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_alloc(map, src, out_len, options, error);
}
/** Short alias of `lonejson_serialize_filep`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_serialize_filep(const lj_map *map, const void *src, FILE *fp,
                   const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_filep(map, src, fp, options, error);
}
/** Short alias of `lonejson_serialize_path`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_serialize_path(const lj_map *map, const void *src, const char *path,
                  const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_path(map, src, path, options, error);
}
/** Short alias of `lonejson_serialize_jsonl_sink`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_serialize_jsonl_sink(const lj_map *map, const void *items, size_t count,
                        size_t stride, lj_sink_fn sink, void *user,
                        const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_jsonl_sink(map, items, count, stride, sink, user,
                                       options, error);
}
/** Short alias of `lonejson_serialize_jsonl_buffer`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_jsonl_buffer(
    const lj_map *map, const void *items, size_t count, size_t stride,
    char *buffer, size_t capacity, size_t *needed,
    const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_jsonl_buffer(map, items, count, stride, buffer,
                                         capacity, needed, options, error);
}
/** Short alias of `lonejson_serialize_jsonl_alloc`. */
LONEJSON_SHORT_ALIAS_INLINE char *
lj_serialize_jsonl_alloc(const lj_map *map, const void *items, size_t count,
                         size_t stride, size_t *out_len,
                         const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_jsonl_alloc(map, items, count, stride, out_len,
                                        options, error);
}
/** Short alias of `lonejson_serialize_jsonl_filep`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_jsonl_filep(
    const lj_map *map, const void *items, size_t count, size_t stride, FILE *fp,
    const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_jsonl_filep(map, items, count, stride, fp, options,
                                        error);
}
/** Short alias of `lonejson_serialize_jsonl_path`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_jsonl_path(
    const lj_map *map, const void *items, size_t count, size_t stride,
    const char *path, const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_jsonl_path(map, items, count, stride, path, options,
                                       error);
}
/** Short alias of `lonejson_cleanup`. */
LONEJSON_SHORT_ALIAS_INLINE void lj_cleanup(const lj_map *map, void *value) {
  lonejson_cleanup(map, value);
}
/** Short alias of `lonejson_reset`. */
LONEJSON_SHORT_ALIAS_INLINE void lj_reset(const lj_map *map, void *value) {
  lonejson_reset(map, value);
}
#ifdef LONEJSON_WITH_CURL
/** Short alias of `lonejson_curl_parse`. */
typedef lonejson_curl_parse lj_curl_parse;
/** Short alias of `lonejson_curl_upload`. */
typedef lonejson_curl_upload lj_curl_upload;
/** Short alias of `lonejson_curl_parse_init`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_curl_parse_init(lj_curl_parse *ctx, const lj_map *map, void *dst,
                   const lj_parse_options *options) {
  return lonejson_curl_parse_init(ctx, map, dst, options);
}
/** Short alias of `lonejson_curl_write_callback`. */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_curl_write_callback(char *ptr,
                                                          size_t size,
                                                          size_t nmemb,
                                                          void *userdata) {
  return lonejson_curl_write_callback(ptr, size, nmemb, userdata);
}
/** Short alias of `lonejson_curl_parse_finish`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_curl_parse_finish(lj_curl_parse *ctx) {
  return lonejson_curl_parse_finish(ctx);
}
/** Short alias of `lonejson_curl_parse_cleanup`. */
LONEJSON_SHORT_ALIAS_INLINE void lj_curl_parse_cleanup(lj_curl_parse *ctx) {
  lonejson_curl_parse_cleanup(ctx);
}
/** Short alias of `lonejson_curl_upload_init`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_curl_upload_init(lj_curl_upload *ctx, const lj_map *map, const void *src,
                    const lj_write_options *options) {
  return lonejson_curl_upload_init(ctx, map, src, options);
}
/** Short alias of `lonejson_curl_read_callback`. */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_curl_read_callback(char *ptr, size_t size,
                                                         size_t nmemb,
                                                         void *userdata) {
  return lonejson_curl_read_callback(ptr, size, nmemb, userdata);
}
/** Short alias of `lonejson_curl_upload_size`. */
LONEJSON_SHORT_ALIAS_INLINE curl_off_t
lj_curl_upload_size(const lj_curl_upload *ctx) {
  return lonejson_curl_upload_size(ctx);
}
/** Short alias of `lonejson_curl_upload_cleanup`. */
LONEJSON_SHORT_ALIAS_INLINE void lj_curl_upload_cleanup(lj_curl_upload *ctx) {
  lonejson_curl_upload_cleanup(ctx);
}
#endif
#endif

#ifdef __cplusplus
}
#endif

#ifdef LONEJSON_IMPLEMENTATION

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
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

#define LONEJSON__MAP_MASK_CACHE_SIZE 8u

#if defined(__GNUC__) || defined(__clang__)
#define LONEJSON__INLINE __inline__ __attribute__((always_inline))
#else
#define LONEJSON__INLINE
#endif

struct lonejson_parser {
  const lonejson_map *root_map;
  void *root_dst;
  lonejson_parse_options options;
  lonejson_error error;
  lonejson_frame *frames;
  unsigned char *workspace;
  size_t workspace_size;
  size_t workspace_top;
  size_t workspace_peak;
  size_t frame_count;
  int validate_only;
  int owns_self;
  int root_started;
  int root_finished;
  int failed;
  lonejson_lex_mode lex_mode;
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
  lonejson_scratch token;
  const lonejson_map *required_mask_maps[LONEJSON__MAP_MASK_CACHE_SIZE];
  lonejson_uint64 required_masks[LONEJSON__MAP_MASK_CACHE_SIZE];
};

typedef struct lonejson_buffer_sink {
  char *buffer;
  size_t capacity;
  size_t length;
  size_t needed;
  lonejson_overflow_policy policy;
  int truncated;
} lonejson_buffer_sink;

typedef enum lonejson_stream_source_kind {
  LONEJSON_STREAM_SOURCE_READER = 1,
  LONEJSON_STREAM_SOURCE_FILE = 2,
  LONEJSON_STREAM_SOURCE_FD = 3
} lonejson_stream_source_kind;

struct lonejson_stream {
  const lonejson_map *map;
  lonejson_parse_options options;
  lonejson_error error;
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
  lonejson_stream_source_kind source_kind;
  void *current_dst;
  void *prepared_dst;
  lonejson_parser *parser;
  unsigned char io_buffer[LONEJSON_STREAM_BUFFER_SIZE];
};

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

lonejson_spool_options lonejson_default_spool_options(void) {
  lonejson_spool_options options;

  options.memory_limit = LONEJSON_SPOOL_MEMORY_LIMIT;
  options.max_bytes = 0u;
  options.temp_dir = NULL;
  return options;
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
  LONEJSON_FREE(value->path);
  value->path = NULL;
  value->fp = NULL;
  value->fd = -1;
  value->kind = LONEJSON_SOURCE_NONE;
}

void lonejson_source_reset(lonejson_source *value) {
  lonejson_source_cleanup(value);
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
  copy = (char *)LONEJSON_MALLOC(len + 1u);
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
  if (value == NULL) {
    return;
  }
  memset(value, 0, sizeof(*value));
  lonejson__spooled_apply_options(value, options);
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
  LONEJSON_FREE(value->memory);
  value->memory = NULL;
  value->memory_len = 0u;
  value->size = 0u;
  value->read_offset = 0u;
  value->spilled = 0;
  lonejson__spooled_close_temp(value);
}

void lonejson_spooled_reset(lonejson_spooled *value) {
  lonejson_spool_options options;

  if (value == NULL) {
    return;
  }
  options.memory_limit = value->memory_limit;
  options.max_bytes = value->max_bytes;
  options.temp_dir = value->temp_dir;
  lonejson_spooled_cleanup(value);
  lonejson__spooled_apply_options(value, &options);
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
  next = (unsigned char *)LONEJSON_REALLOC(value->memory, need);
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

typedef struct lonejson__source_cursor {
  const lonejson_source *source;
  FILE *fp;
  int close_fp;
  int use_fd;
} lonejson__source_cursor;

static lonejson_status
lonejson__source_cursor_open(const lonejson_source *value,
                             lonejson__source_cursor *cursor,
                             lonejson_error *error) {
  memset(cursor, 0, sizeof(*cursor));
  cursor->source = value;
  if (value->kind == LONEJSON_SOURCE_NONE) {
    return LONEJSON_STATUS_OK;
  }
  if (value->kind == LONEJSON_SOURCE_PATH) {
    cursor->fp = fopen(value->path, "rb");
    if (cursor->fp == NULL) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to open source path '%s'",
                                 value->path);
    }
    cursor->close_fp = 1;
    return LONEJSON_STATUS_OK;
  }
  if (value->kind == LONEJSON_SOURCE_FILE) {
    if (fseek(value->fp, 0L, SEEK_SET) != 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to rewind source FILE*; non-seekable "
                                 "sources are unsupported");
    }
    clearerr(value->fp);
    cursor->fp = value->fp;
    return LONEJSON_STATUS_OK;
  }
  if (lseek(value->fd, 0, SEEK_SET) < 0) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(
        error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
        "failed to rewind source fd; non-seekable sources are unsupported");
  }
  cursor->use_fd = 1;
  return LONEJSON_STATUS_OK;
}

static void lonejson__source_cursor_close(lonejson__source_cursor *cursor) {
  if (cursor->close_fp && cursor->fp != NULL) {
    fclose(cursor->fp);
  }
  cursor->fp = NULL;
  cursor->close_fp = 0;
  cursor->use_fd = 0;
}

static lonejson_read_result
lonejson__source_cursor_read(lonejson__source_cursor *cursor,
                             unsigned char *buffer, size_t capacity) {
  lonejson_read_result result;

  memset(&result, 0, sizeof(result));
  if (cursor->source->kind == LONEJSON_SOURCE_NONE) {
    result.eof = 1;
    return result;
  }
  if (cursor->use_fd) {
    ssize_t got = read(cursor->source->fd, buffer, capacity);
    if (got < 0) {
      result.error_code = errno;
      return result;
    }
    result.bytes_read = (size_t)got;
    result.eof = (got == 0) ? 1 : 0;
    return result;
  }
  result.bytes_read = fread(buffer, 1u, capacity, cursor->fp);
  if (result.bytes_read < capacity) {
    if (ferror(cursor->fp)) {
      result.error_code = errno;
      return result;
    }
    result.eof = 1;
  }
  return result;
}

lonejson_status lonejson_source_write_to_sink(const lonejson_source *value,
                                              lonejson_sink_fn sink, void *user,
                                              lonejson_error *error) {
  unsigned char buffer[4096];
  lonejson__source_cursor cursor;
  lonejson_status status;
  lonejson_read_result chunk;

  if (value == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "source handle and sink are required");
  }
  status = lonejson__source_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (;;) {
    chunk = lonejson__source_cursor_read(&cursor, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      lonejson__source_cursor_close(&cursor);
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read source data");
    }
    if (chunk.bytes_read != 0u) {
      status = sink(user, buffer, chunk.bytes_read, error);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        lonejson__source_cursor_close(&cursor);
        return status;
      }
    }
    if (chunk.eof) {
      break;
    }
  }
  lonejson__source_cursor_close(&cursor);
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE int lonejson__scratch_reserve(lonejson_scratch *scratch,
                                                      size_t need) {
  if (need + 1u <= scratch->cap) {
    return 1;
  }
  return 0;
}

static LONEJSON__INLINE int lonejson__scratch_append(lonejson_scratch *scratch,
                                                     char ch) {
  if (!lonejson__scratch_reserve(scratch, scratch->len + 2u)) {
    return 0;
  }
  scratch->data[scratch->len++] = ch;
  return 1;
}

static LONEJSON__INLINE int
lonejson__scratch_append_bytes(lonejson_scratch *scratch,
                               const unsigned char *src, size_t len) {
  if (!lonejson__scratch_reserve(scratch, scratch->len + len + 1u)) {
    return 0;
  }
  memcpy(scratch->data + scratch->len, src, len);
  scratch->len += len;
  return 1;
}

static int lonejson__is_valid_json_number(const char *value, size_t len);
static int lonejson__is_json_space(int ch);
static int lonejson__is_digit(int ch);
static int lonejson__is_finite_f64(double value);
static int lonejson__is_exact_fixed_capacity(unsigned flags);
static size_t lonejson__frame_bytes(size_t frame_count);
static size_t lonejson__workspace_align(size_t size);
static unsigned char *lonejson__token_base(lonejson_parser *parser);
static size_t lonejson__token_limit(lonejson_parser *parser);
static unsigned char *lonejson__workspace_alloc_top(lonejson_parser *parser,
                                                    size_t size);
static lonejson_status lonejson__stream_value_begin(lonejson_parser *parser,
                                                    const lonejson_field *field,
                                                    void *ptr);
static void lonejson__parser_init_state(lonejson_parser *parser,
                                        const lonejson_map *map, void *dst,
                                        const lonejson_parse_options *options,
                                        int validate_only,
                                        unsigned char *workspace,
                                        size_t workspace_size);
#define lonejson__parser_peak_workspace_used(parser_ptr)                       \
  ((parser_ptr) != NULL ? (parser_ptr)->workspace_peak : 0u)
static lonejson_status lonejson__parser_feed_bytes(lonejson_parser *parser,
                                                   const unsigned char *bytes,
                                                   size_t len, size_t *consumed,
                                                   int stop_at_root);

static void lonejson__scratch_reset(lonejson_scratch *scratch) {
  scratch->len = 0;
}

static const char *lonejson__scratch_cstr(lonejson_scratch *scratch) {
  if (scratch == NULL || scratch->data == NULL) {
    return "";
  }
  scratch->data[scratch->len] = '\0';
  return scratch->data;
}

static LONEJSON__INLINE int lonejson__is_json_space(int ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static LONEJSON__INLINE int lonejson__is_digit(int ch) {
  return ch >= '0' && ch <= '9';
}

static int lonejson__is_finite_f64(double value) {
  return value == value && value <= DBL_MAX && value >= -DBL_MAX;
}

static int lonejson__is_exact_fixed_capacity(unsigned flags) {
  return flags == LONEJSON_ARRAY_FIXED_CAPACITY;
}

static int lonejson__preserve_fixed_scalar_array(const void *items,
                                                 size_t capacity,
                                                 unsigned flags) {
  return lonejson__is_exact_fixed_capacity(flags) && items != NULL &&
         capacity != 0u;
}

static int lonejson__preserve_fixed_object_array(const void *items,
                                                 size_t capacity,
                                                 size_t elem_size,
                                                 unsigned flags,
                                                 size_t expected_elem_size) {
  return lonejson__preserve_fixed_scalar_array(items, capacity, flags) &&
         elem_size == expected_elem_size;
}

static size_t lonejson__frame_bytes(size_t frame_count) {
  return frame_count * sizeof(lonejson_frame);
}

static LONEJSON__INLINE unsigned char *
lonejson__token_base(lonejson_parser *parser) {
  return parser->workspace + lonejson__frame_bytes(parser->frame_count);
}

static LONEJSON__INLINE size_t lonejson__token_limit(lonejson_parser *parser) {
  size_t frame_bytes = lonejson__frame_bytes(parser->frame_count);

  if (parser->workspace_top <= frame_bytes) {
    return 0u;
  }
  return parser->workspace_top - frame_bytes - 1u;
}

static LONEJSON__INLINE void
lonejson__parser_note_workspace_usage(lonejson_parser *parser) {
#if LONEJSON_TRACK_WORKSPACE_USAGE
  size_t frame_bytes;
  size_t reserved_top_bytes;
  size_t token_bytes;
  size_t used;

  frame_bytes = lonejson__frame_bytes(parser->frame_count);
  reserved_top_bytes = parser->workspace_size - parser->workspace_top;
  token_bytes = 0u;
  if (parser->lex_mode != LONEJSON_LEX_NONE || parser->token.len != 0u) {
    token_bytes = parser->token.len + 1u;
  }
  used = frame_bytes + reserved_top_bytes + token_bytes;
  if (used > parser->workspace_peak) {
    parser->workspace_peak = used;
  }
#else
  (void)parser;
#endif
}

static LONEJSON__INLINE unsigned char *
lonejson__workspace_alloc_top(lonejson_parser *parser, size_t size) {
  size_t frame_bytes = lonejson__frame_bytes(parser->frame_count);
  size_t aligned_size = lonejson__workspace_align(size);

  if (aligned_size > parser->workspace_top ||
      parser->workspace_top - aligned_size < frame_bytes) {
    return NULL;
  }
  parser->workspace_top -= aligned_size;
  return parser->workspace + parser->workspace_top;
}

static LONEJSON__INLINE size_t lonejson__workspace_align(size_t size) {
  return (size + (sizeof(void *) - 1u)) & ~(sizeof(void *) - 1u);
}

static LONEJSON__INLINE int lonejson__prepare_token(lonejson_parser *parser) {
  parser->token.data = (char *)lonejson__token_base(parser);
  parser->token.cap = lonejson__token_limit(parser);
  if (parser->token.cap == 0u) {
    parser->token.len = 0u;
    return 0;
  }
  lonejson__scratch_reset(&parser->token);
  return 1;
}

static int lonejson__field_is_streamed(const lonejson_field *field) {
  return field != NULL && (field->kind == LONEJSON_FIELD_KIND_STRING_STREAM ||
                           field->kind == LONEJSON_FIELD_KIND_BASE64_STREAM);
}

static LONEJSON__INLINE lonejson_status
lonejson__begin_string_lex(lonejson_parser *parser, int is_key) {
  parser->lex_mode = LONEJSON_LEX_STRING;
  parser->lex_is_key = is_key;
  parser->lex_escape = 0;
  parser->unicode_pending_high = 0u;
  parser->unicode_digits_needed = 0;
  return lonejson__prepare_token(parser)
             ? LONEJSON_STATUS_OK
             : lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
}

static LONEJSON__INLINE lonejson_status lonejson__begin_string_value_lex(
    lonejson_parser *parser, const lonejson_field *field, void *ptr) {
  lonejson_status status;

  parser->lex_mode = LONEJSON_LEX_STRING;
  parser->lex_is_key = 0;
  parser->lex_escape = 0;
  parser->unicode_pending_high = 0u;
  parser->unicode_digits_needed = 0;
  if (lonejson__field_is_streamed(field)) {
    status = lonejson__stream_value_begin(parser, field, ptr);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    return LONEJSON_STATUS_OK;
  }
  return lonejson__prepare_token(parser)
             ? LONEJSON_STATUS_OK
             : lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
}

static LONEJSON__INLINE lonejson_status
lonejson__begin_number_lex(lonejson_parser *parser, unsigned char ch) {
  parser->lex_mode = LONEJSON_LEX_NUMBER;
  parser->lex_is_key = 0;
  if (!lonejson__prepare_token(parser)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "parser workspace exhausted by token");
  }
  return lonejson__scratch_append(&parser->token, (char)ch)
             ? LONEJSON_STATUS_OK
             : lonejson__set_error(
                   &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                   parser->error.offset, parser->error.line,
                   parser->error.column, "failed to append number");
}

static LONEJSON__INLINE lonejson_status lonejson__begin_literal_lex(
    lonejson_parser *parser, lonejson_lex_mode mode, unsigned char ch) {
  parser->lex_mode = mode;
  parser->lex_is_key = 0;
  if (!lonejson__prepare_token(parser)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "parser workspace exhausted by token");
  }
  return lonejson__scratch_append(&parser->token, (char)ch)
             ? LONEJSON_STATUS_OK
             : lonejson__set_error(
                   &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                   parser->error.offset, parser->error.line,
                   parser->error.column, "failed to append literal");
}

static int lonejson__base64_value(int ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return ch - 'A';
  }
  if (ch >= 'a' && ch <= 'z') {
    return 26 + (ch - 'a');
  }
  if (ch >= '0' && ch <= '9') {
    return 52 + (ch - '0');
  }
  if (ch == '+') {
    return 62;
  }
  if (ch == '/') {
    return 63;
  }
  if (ch == '=') {
    return -2;
  }
  return -1;
}

static lonejson_status
lonejson__stream_value_append_decoded(lonejson_parser *parser,
                                      const unsigned char *data, size_t len) {
  if (!parser->stream_value_active || parser->stream_ptr == NULL) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "streamed field is not active");
  }
  return lonejson__spooled_append((lonejson_spooled *)parser->stream_ptr, data,
                                  len, &parser->error);
}

static lonejson_status
lonejson__stream_value_append_base64_char(lonejson_parser *parser,
                                          unsigned char ch) {
  int value;

  value = lonejson__base64_value(ch);
  if (value == -1) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "invalid base64 character in streamed field");
  }
  if (parser->stream_base64_saw_padding && ch != '=') {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "non-padding base64 data after padding");
  }
  parser->stream_base64_quad[parser->stream_base64_quad_len++] = ch;
  if (ch == '=') {
    parser->stream_base64_saw_padding = 1;
  }
  if (parser->stream_base64_quad_len == 4u) {
    unsigned char out[3];
    int a;
    int b;
    int c;
    int d;
    size_t out_len;
    lonejson_status status;

    a = lonejson__base64_value(parser->stream_base64_quad[0]);
    b = lonejson__base64_value(parser->stream_base64_quad[1]);
    c = lonejson__base64_value(parser->stream_base64_quad[2]);
    d = lonejson__base64_value(parser->stream_base64_quad[3]);
    if (a < 0 || b < 0 || c == -1 || d == -1) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid base64 quartet in streamed field");
    }
    out[0] = (unsigned char)((a << 2) | ((b & 0x30) >> 4));
    out_len = 1u;
    if (c != -2) {
      out[1] = (unsigned char)(((b & 0x0Fu) << 4) | ((c & 0x3Cu) >> 2));
      out_len = 2u;
      if (d != -2) {
        out[2] = (unsigned char)(((c & 0x03u) << 6) | d);
        out_len = 3u;
      }
    } else if (d != -2) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid base64 padding in streamed field");
    }
    status = lonejson__stream_value_append_decoded(parser, out, out_len);
    parser->stream_base64_quad_len = 0u;
    return status;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__stream_value_append(lonejson_parser *parser,
                                                     const unsigned char *data,
                                                     size_t len) {
  if (parser->stream_field->kind == LONEJSON_FIELD_KIND_STRING_STREAM) {
    return lonejson__stream_value_append_decoded(parser, data, len);
  }
  {
    size_t i;
    lonejson_status status;

    for (i = 0u; i < len; ++i) {
      status = lonejson__stream_value_append_base64_char(parser, data[i]);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__stream_value_begin(lonejson_parser *parser,
                                                    const lonejson_field *field,
                                                    void *ptr) {
  lonejson_spooled_reset((lonejson_spooled *)ptr);
  parser->stream_value_active = 1;
  parser->stream_field = field;
  parser->stream_ptr = ptr;
  parser->stream_base64_quad_len = 0u;
  parser->stream_base64_saw_padding = 0;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__stream_value_finish(lonejson_parser *parser) {
  lonejson_status status;

  status = LONEJSON_STATUS_OK;
  if (parser->stream_value_active &&
      parser->stream_field->kind == LONEJSON_FIELD_KIND_BASE64_STREAM &&
      parser->stream_base64_quad_len != 0u) {
    status = lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "incomplete base64 quartet in streamed field");
  }
  parser->stream_value_active = 0;
  parser->stream_field = NULL;
  parser->stream_ptr = NULL;
  parser->stream_base64_quad_len = 0u;
  parser->stream_base64_saw_padding = 0;
  return status;
}

static lonejson_uint64
lonejson__required_mask_for_map(lonejson_parser *parser,
                                const lonejson_map *map) {
  size_t i;
  size_t slot;
  lonejson_uint64 mask;

  if (parser == NULL || map == NULL || map->field_count > 64u) {
    return 0u;
  }
  slot = ((size_t)((const void *)map) >> 4u) % LONEJSON__MAP_MASK_CACHE_SIZE;
  if (parser->required_mask_maps[slot] == map) {
    return parser->required_masks[slot];
  }
  mask = 0u;
  for (i = 0u; i < map->field_count; ++i) {
    if ((map->fields[i].flags & LONEJSON_FIELD_REQUIRED) != 0u) {
      mask |= ((lonejson_uint64)1u) << i;
    }
  }
  parser->required_mask_maps[slot] = map;
  parser->required_masks[slot] = mask;
  return mask;
}

static size_t lonejson__required_count_for_map(lonejson_parser *parser,
                                               const lonejson_map *map) {
  size_t count;
  size_t i;

  if (parser == NULL || map == NULL) {
    return 0u;
  }
  if (map->field_count <= 64u) {
    lonejson_uint64 mask = lonejson__required_mask_for_map(parser, map);
    size_t count = 0u;

    while (mask != 0u) {
      count += (size_t)(mask & 1u);
      mask >>= 1u;
    }
    return count;
  }
  count = 0u;
  for (i = 0u; i < map->field_count; ++i) {
    if ((map->fields[i].flags & LONEJSON_FIELD_REQUIRED) != 0u) {
      count++;
    }
  }
  return count;
}

static void lonejson__parser_init_state(lonejson_parser *parser,
                                        const lonejson_map *map, void *dst,
                                        const lonejson_parse_options *options,
                                        int validate_only,
                                        unsigned char *workspace,
                                        size_t workspace_size) {
  memset(parser, 0, sizeof(*parser));
  parser->root_map = map;
  parser->root_dst = dst;
  parser->validate_only = validate_only;
  parser->options = options ? *options : lonejson_default_parse_options();
  parser->error.line = 1u;
  parser->error.column = 0u;
  parser->error.code = LONEJSON_STATUS_OK;
  parser->workspace = workspace;
  parser->workspace_size = workspace_size;
  parser->workspace_top = workspace_size;
  parser->frames = (lonejson_frame *)workspace;
  parser->stream_value_active = 0;
  parser->stream_field = NULL;
  parser->stream_ptr = NULL;
  parser->stream_base64_quad_len = 0u;
  parser->stream_base64_saw_padding = 0;
  parser->token.data = (char *)lonejson__token_base(parser);
  parser->token.cap = lonejson__token_limit(parser);
  parser->token.data[0] = '\0';
  lonejson__parser_note_workspace_usage(parser);
}

static void lonejson__parser_restart_stream(lonejson_parser *parser,
                                            void *dst) {
  parser->root_dst = dst;
  memset(&parser->error, 0, sizeof(parser->error));
  parser->error.line = 1u;
  parser->error.column = 0u;
  parser->error.code = LONEJSON_STATUS_OK;
  parser->workspace_top = parser->workspace_size;
  parser->frame_count = 0u;
  parser->workspace_peak = 0u;
  parser->root_started = 0;
  parser->root_finished = 0;
  parser->failed = 0;
  parser->lex_mode = LONEJSON_LEX_NONE;
  parser->lex_is_key = 0;
  parser->lex_escape = 0;
  parser->unicode_pending_high = 0u;
  parser->unicode_digits_needed = 0;
  parser->unicode_accum = 0u;
  parser->stream_value_active = 0;
  parser->stream_field = NULL;
  parser->stream_ptr = NULL;
  parser->stream_base64_quad_len = 0u;
  parser->stream_base64_saw_padding = 0;
  parser->token.len = 0u;
  parser->token.data = (char *)lonejson__token_base(parser);
  parser->token.cap = lonejson__token_limit(parser);
  parser->token.data[0] = '\0';
  lonejson__parser_note_workspace_usage(parser);
}

static LONEJSON__INLINE int lonejson__utf8_append(lonejson_scratch *scratch,
                                                  lonejson_uint32 cp) {
  char *dst;
  size_t len;

  if (scratch == NULL) {
    return 0;
  }
  if (cp <= 0x7Fu) {
    len = 1u;
  } else if (cp <= 0x7FFu) {
    len = 2u;
  } else if (cp <= 0xFFFFu) {
    len = 3u;
  } else if (cp <= 0x10FFFFu) {
    len = 4u;
  } else {
    return 0;
  }
  if (!lonejson__scratch_reserve(scratch, scratch->len + len + 1u)) {
    return 0;
  }
  dst = scratch->data + scratch->len;
  if (len == 1u) {
    dst[0] = (char)cp;
  } else if (len == 2u) {
    dst[0] = (char)(0xC0u | (cp >> 6u));
    dst[1] = (char)(0x80u | (cp & 0x3Fu));
  } else if (len == 3u) {
    dst[0] = (char)(0xE0u | (cp >> 12u));
    dst[1] = (char)(0x80u | ((cp >> 6u) & 0x3Fu));
    dst[2] = (char)(0x80u | (cp & 0x3Fu));
  } else {
    dst[0] = (char)(0xF0u | (cp >> 18u));
    dst[1] = (char)(0x80u | ((cp >> 12u) & 0x3Fu));
    dst[2] = (char)(0x80u | ((cp >> 6u) & 0x3Fu));
    dst[3] = (char)(0x80u | (cp & 0x3Fu));
  }
  scratch->len += len;
  return 1;
}

static LONEJSON__INLINE int
lonejson__field_key_matches(const lonejson_field *field, const char *key,
                            size_t key_len) {
  if (field == NULL || field->json_key_len != key_len) {
    return 0;
  }
  if (key_len == 0u) {
    return 1;
  }
  if (field->json_key_first != (unsigned char)key[0] ||
      field->json_key_last != (unsigned char)key[key_len - 1u]) {
    return 0;
  }
  switch (key_len) {
  case 1u:
  case 2u:
    return 1;
  case 3u:
    return field->json_key[1] == key[1];
  case 4u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2];
  case 5u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3];
  case 6u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4];
  case 7u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5];
  case 8u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6];
  case 9u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7];
  case 10u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8];
  case 11u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8] &&
           field->json_key[9] == key[9];
  case 12u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8] &&
           field->json_key[9] == key[9] && field->json_key[10] == key[10];
  case 13u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8] &&
           field->json_key[9] == key[9] && field->json_key[10] == key[10] &&
           field->json_key[11] == key[11];
  case 14u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8] &&
           field->json_key[9] == key[9] && field->json_key[10] == key[10] &&
           field->json_key[11] == key[11] && field->json_key[12] == key[12];
  case 15u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8] &&
           field->json_key[9] == key[9] && field->json_key[10] == key[10] &&
           field->json_key[11] == key[11] && field->json_key[12] == key[12] &&
           field->json_key[13] == key[13];
  case 16u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8] &&
           field->json_key[9] == key[9] && field->json_key[10] == key[10] &&
           field->json_key[11] == key[11] && field->json_key[12] == key[12] &&
           field->json_key[13] == key[13] && field->json_key[14] == key[14];
  default:
    return memcmp(field->json_key + 1u, key + 1u, key_len - 2u) == 0;
  }
}

static LONEJSON__INLINE size_t
lonejson__field_index(const lonejson_map *map, const lonejson_field *field) {
  if (map == NULL || field == NULL || field < map->fields ||
      field >= map->fields + map->field_count) {
    return SIZE_MAX;
  }
  return (size_t)(field - map->fields);
}

static const lonejson_field *lonejson__find_field(const lonejson_map *map,
                                                  lonejson_frame *frame,
                                                  const char *key,
                                                  size_t key_len) {
  size_t i;

  if (map == NULL) {
    return NULL;
  }
  if (key_len != 0u && frame != NULL &&
      frame->next_field_hint < map->field_count &&
      lonejson__field_key_matches(&map->fields[frame->next_field_hint], key,
                                  key_len)) {
    const lonejson_field *field = &map->fields[frame->next_field_hint];
    frame->next_field_hint++;
    return field;
  }
  for (i = 0; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];
    if (lonejson__field_key_matches(field, key, key_len)) {
      if (frame != NULL) {
        frame->next_field_hint = i + 1u;
      }
      return field;
    }
  }
  return NULL;
}

static LONEJSON__INLINE void *lonejson__field_ptr(void *base,
                                                  const lonejson_field *field) {
  return (unsigned char *)base + field->struct_offset;
}

static LONEJSON__INLINE const void *
lonejson__field_cptr(const void *base, const lonejson_field *field) {
  return (const unsigned char *)base + field->struct_offset;
}

static lonejson_frame *lonejson__push_frame(lonejson_parser *parser,
                                            lonejson_container_kind kind) {
  lonejson_frame *frame;

  if (parser->options.max_depth != 0 &&
      parser->frame_count >= parser->options.max_depth) {
    lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                        parser->error.offset, parser->error.line,
                        parser->error.column, "maximum nesting depth exceeded");
    parser->failed = 1;
    return NULL;
  }
  if (lonejson__frame_bytes(parser->frame_count + 1u) > parser->workspace_top) {
    lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                        parser->error.offset, parser->error.line,
                        parser->error.column,
                        "parser workspace exhausted by nesting depth");
    parser->failed = 1;
    return NULL;
  }
  frame = &parser->frames[parser->frame_count++];
  memset(frame, 0, sizeof(*frame));
  frame->kind = kind;
  frame->state = (kind == LONEJSON_CONTAINER_OBJECT)
                     ? LONEJSON_FRAME_OBJECT_KEY_OR_END
                     : LONEJSON_FRAME_ARRAY_VALUE_OR_END;
  return frame;
}

static void lonejson__pop_frame(lonejson_parser *parser) {
  lonejson_frame *frame;

  if (parser->frame_count == 0) {
    return;
  }
  frame = &parser->frames[parser->frame_count - 1u];
  if (frame->seen_fields != NULL) {
    parser->workspace_top += frame->seen_bytes;
    frame->seen_fields = NULL;
    frame->seen_bytes = 0u;
  }
  parser->frame_count--;
}

static int lonejson__array_ensure_bytes(void **items_ptr, size_t *cap_ptr,
                                        size_t elem_size, unsigned *flags,
                                        size_t want,
                                        lonejson_overflow_policy policy,
                                        lonejson_error *error) {
  void *next;
  size_t cap;

  if (*cap_ptr >= want) {
    return 1;
  }
  if ((*flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
    if (policy == LONEJSON_OVERFLOW_FAIL) {
      lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, error->offset,
                          error->line, error->column,
                          "array capacity exceeded");
      return 0;
    }
    error->truncated = 1;
    return 0;
  }
  cap = (*cap_ptr != 0u) ? *cap_ptr : 4u;
  while (cap < want) {
    if (cap > (SIZE_MAX / 2u)) {
      lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, error->offset,
                          error->line, error->column, "array too large");
      return 0;
    }
    cap *= 2u;
  }
  next = LONEJSON_REALLOC(*items_ptr, cap * elem_size);
  if (next == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, error->offset,
                        error->line, error->column, "failed to grow array");
    return 0;
  }
  *items_ptr = next;
  *cap_ptr = cap;
  *flags |= LONEJSON_ARRAY_OWNS_ITEMS;
  return 1;
}

static void lonejson__cleanup_value(const lonejson_field *field, void *ptr);
static void lonejson__reset_map(const lonejson_map *map, void *value);
static void lonejson__init_map(const lonejson_map *map, void *value);

static int lonejson__map_may_allocate(const lonejson_map *map) {
  size_t i;

  if (map == NULL) {
    return 0;
  }
  for (i = 0u; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];

    switch (field->kind) {
    case LONEJSON_FIELD_KIND_STRING:
      if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
        return 1;
      }
      break;
    case LONEJSON_FIELD_KIND_STRING_STREAM:
    case LONEJSON_FIELD_KIND_BASE64_STREAM:
    case LONEJSON_FIELD_KIND_STRING_SOURCE:
    case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    case LONEJSON_FIELD_KIND_STRING_ARRAY:
      return 1;
    case LONEJSON_FIELD_KIND_OBJECT:
      if (lonejson__map_may_allocate(field->submap)) {
        return 1;
      }
      break;
    case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
      if (field->storage != LONEJSON_STORAGE_FIXED ||
          lonejson__map_may_allocate(field->submap)) {
        return 1;
      }
      break;
    case LONEJSON_FIELD_KIND_I64_ARRAY:
    case LONEJSON_FIELD_KIND_U64_ARRAY:
    case LONEJSON_FIELD_KIND_F64_ARRAY:
    case LONEJSON_FIELD_KIND_BOOL_ARRAY:
      if (field->storage != LONEJSON_STORAGE_FIXED) {
        return 1;
      }
      break;
    default:
      break;
    }
  }
  return 0;
}

static int lonejson__map_is_flat_zeroable(const lonejson_map *map) {
  size_t i;

  if (map == NULL) {
    return 0;
  }
  for (i = 0u; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];

    switch (field->kind) {
    case LONEJSON_FIELD_KIND_I64:
    case LONEJSON_FIELD_KIND_U64:
    case LONEJSON_FIELD_KIND_F64:
    case LONEJSON_FIELD_KIND_BOOL:
      break;
    case LONEJSON_FIELD_KIND_STRING:
      if (field->storage != LONEJSON_STORAGE_FIXED) {
        return 0;
      }
      break;
    default:
      return 0;
    }
  }
  return 1;
}

static void lonejson__cleanup_map(const lonejson_map *map, void *value) {
  size_t i;

  if (map == NULL || value == NULL) {
    return;
  }
  for (i = 0; i < map->field_count; ++i) {
    lonejson__cleanup_value(&map->fields[i],
                            lonejson__field_ptr(value, &map->fields[i]));
  }
}

static void lonejson__reset_scalar_field(const lonejson_field *field,
                                         void *ptr) {
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_I64:
    *(lonejson_int64 *)ptr = 0;
    break;
  case LONEJSON_FIELD_KIND_U64:
    *(lonejson_uint64 *)ptr = 0u;
    break;
  case LONEJSON_FIELD_KIND_F64:
    *(double *)ptr = 0.0;
    break;
  case LONEJSON_FIELD_KIND_BOOL:
    *(bool *)ptr = false;
    break;
  default:
    break;
  }
}

static void lonejson__reset_map(const lonejson_map *map, void *value) {
  size_t i;

  if (map == NULL || value == NULL) {
    return;
  }
  for (i = 0; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];
    void *ptr = lonejson__field_ptr(value, field);
    switch (field->kind) {
    case LONEJSON_FIELD_KIND_I64:
    case LONEJSON_FIELD_KIND_U64:
    case LONEJSON_FIELD_KIND_F64:
    case LONEJSON_FIELD_KIND_BOOL:
      lonejson__reset_scalar_field(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_STRING:
      if (field->storage == LONEJSON_STORAGE_FIXED &&
          field->fixed_capacity != 0u) {
        ((char *)ptr)[0] = '\0';
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_OBJECT:
      if (field->submap != NULL && !lonejson__map_may_allocate(field->submap)) {
        lonejson__reset_map(field->submap, ptr);
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_I64_ARRAY:
      if (lonejson__is_exact_fixed_capacity(
              ((lonejson_i64_array *)ptr)->flags)) {
        ((lonejson_i64_array *)ptr)->count = 0u;
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_U64_ARRAY:
      if (lonejson__is_exact_fixed_capacity(
              ((lonejson_u64_array *)ptr)->flags)) {
        ((lonejson_u64_array *)ptr)->count = 0u;
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_F64_ARRAY:
      if (lonejson__is_exact_fixed_capacity(
              ((lonejson_f64_array *)ptr)->flags)) {
        ((lonejson_f64_array *)ptr)->count = 0u;
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_BOOL_ARRAY:
      if (lonejson__is_exact_fixed_capacity(
              ((lonejson_bool_array *)ptr)->flags)) {
        ((lonejson_bool_array *)ptr)->count = 0u;
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
      if (lonejson__is_exact_fixed_capacity(
              ((lonejson_object_array *)ptr)->flags) &&
          !lonejson__map_may_allocate(field->submap)) {
        ((lonejson_object_array *)ptr)->count = 0u;
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    default:
      lonejson__cleanup_value(field, ptr);
      break;
    }
  }
}

static void lonejson__init_value(const lonejson_field *field, void *ptr) {
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      *(char **)ptr = NULL;
    } else if (field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
    }
    break;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    lonejson_spooled_init((lonejson_spooled *)ptr, field->spool_options);
    break;
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    lonejson_source_init((lonejson_source *)ptr);
    break;
  case LONEJSON_FIELD_KIND_I64:
  case LONEJSON_FIELD_KIND_U64:
  case LONEJSON_FIELD_KIND_F64:
  case LONEJSON_FIELD_KIND_BOOL:
    lonejson__reset_scalar_field(field, ptr);
    break;
  case LONEJSON_FIELD_KIND_OBJECT:
    if (lonejson__map_is_flat_zeroable(field->submap)) {
      memset(ptr, 0, field->submap->struct_size);
      break;
    }
    lonejson__init_map(field->submap, ptr);
    break;
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    char **items = NULL;
    size_t capacity = 0u;
    unsigned flags = 0u;
    if (lonejson__preserve_fixed_scalar_array(arr->items, arr->capacity,
                                              arr->flags)) {
      arr->count = 0u;
      break;
    }
    memset(arr, 0, sizeof(*arr));
    arr->items = items;
    arr->capacity = capacity;
    arr->flags = flags;
    break;
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    lonejson_int64 *items = NULL;
    size_t capacity = 0u;
    unsigned flags = 0u;
    if (lonejson__preserve_fixed_scalar_array(arr->items, arr->capacity,
                                              arr->flags)) {
      arr->count = 0u;
      break;
    }
    memset(arr, 0, sizeof(*arr));
    arr->items = items;
    arr->capacity = capacity;
    arr->flags = flags;
    break;
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    lonejson_uint64 *items = NULL;
    size_t capacity = 0u;
    unsigned flags = 0u;
    if (lonejson__preserve_fixed_scalar_array(arr->items, arr->capacity,
                                              arr->flags)) {
      arr->count = 0u;
      break;
    }
    memset(arr, 0, sizeof(*arr));
    arr->items = items;
    arr->capacity = capacity;
    arr->flags = flags;
    break;
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    double *items = NULL;
    size_t capacity = 0u;
    unsigned flags = 0u;
    if (lonejson__preserve_fixed_scalar_array(arr->items, arr->capacity,
                                              arr->flags)) {
      arr->count = 0u;
      break;
    }
    memset(arr, 0, sizeof(*arr));
    arr->items = items;
    arr->capacity = capacity;
    arr->flags = flags;
    break;
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    bool *items = NULL;
    size_t capacity = 0u;
    unsigned flags = 0u;
    if (lonejson__preserve_fixed_scalar_array(arr->items, arr->capacity,
                                              arr->flags)) {
      arr->count = 0u;
      break;
    }
    memset(arr, 0, sizeof(*arr));
    arr->items = items;
    arr->capacity = capacity;
    arr->flags = flags;
    break;
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    void *items = NULL;
    size_t capacity = 0u;
    size_t elem_size = field->elem_size;
    unsigned flags = 0u;
    if (lonejson__preserve_fixed_object_array(arr->items, arr->capacity,
                                              arr->elem_size, arr->flags,
                                              field->elem_size)) {
      arr->count = 0u;
      break;
    }
    memset(arr, 0, sizeof(*arr));
    arr->items = items;
    arr->capacity = capacity;
    arr->elem_size = elem_size;
    arr->flags = flags;
    break;
  }
  default:
    break;
  }
}

static void lonejson__init_map(const lonejson_map *map, void *value) {
  size_t i;

  if (map == NULL || value == NULL) {
    return;
  }
  for (i = 0; i < map->field_count; ++i) {
    lonejson__init_value(&map->fields[i],
                         lonejson__field_ptr(value, &map->fields[i]));
  }
}

static void lonejson__cleanup_value(const lonejson_field *field, void *ptr) {
  size_t i;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      char **p = (char **)ptr;
      LONEJSON_FREE(*p);
      *p = NULL;
    } else if (field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
    }
    break;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    lonejson_spooled_cleanup((lonejson_spooled *)ptr);
    lonejson_spooled_init((lonejson_spooled *)ptr, field->spool_options);
    break;
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    lonejson_source_cleanup((lonejson_source *)ptr);
    lonejson_source_init((lonejson_source *)ptr);
    break;
  case LONEJSON_FIELD_KIND_OBJECT:
    lonejson__cleanup_map(field->submap, ptr);
    memset(ptr, 0, field->submap ? field->submap->struct_size : 0u);
    break;
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    char **items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    for (i = 0; i < arr->count; ++i) {
      LONEJSON_FREE(arr->items[i]);
    }
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      LONEJSON_FREE(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    lonejson_int64 *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      LONEJSON_FREE(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    lonejson_uint64 *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      LONEJSON_FREE(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    double *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      LONEJSON_FREE(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    bool *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      LONEJSON_FREE(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    void *items = arr->items;
    size_t capacity = arr->capacity;
    size_t elem_size = arr->elem_size;
    unsigned flags = arr->flags;
    if (field->submap != NULL && arr->items != NULL) {
      for (i = 0; i < arr->count; ++i) {
        void *elem = (unsigned char *)arr->items + (i * field->elem_size);
        lonejson__cleanup_map(field->submap, elem);
      }
    }
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      LONEJSON_FREE(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->elem_size = elem_size;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  default:
    break;
  }
}

static lonejson_status lonejson__assign_string(lonejson_parser *parser,
                                               const lonejson_field *field,
                                               void *ptr, const char *value,
                                               size_t len) {
  if (memchr(value, '\0', len) != NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "field '%s' contains embedded NUL unsupported by C strings",
        field->json_key);
  }
  if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
    char **dst = (char **)ptr;
    char *copy = (char *)LONEJSON_MALLOC(len + 1u);
    if (copy == NULL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
          parser->error.offset, parser->error.line, parser->error.column,
          "failed to allocate string");
    }
    memcpy(copy, value, len);
    copy[len] = '\0';
    LONEJSON_FREE(*dst);
    *dst = copy;
    return LONEJSON_STATUS_OK;
  }
  if (field->fixed_capacity == 0u) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_ARGUMENT,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "fixed string field has zero capacity");
  }
  if (len + 1u > field->fixed_capacity) {
    if (field->overflow_policy == LONEJSON_OVERFLOW_FAIL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
          parser->error.line, parser->error.column,
          "string field '%s' exceeds fixed capacity", field->json_key);
    }
    memcpy(ptr, value, field->fixed_capacity - 1u);
    ((char *)ptr)[field->fixed_capacity - 1u] = '\0';
    parser->error.truncated = 1;
    return (field->overflow_policy == LONEJSON_OVERFLOW_TRUNCATE)
               ? LONEJSON_STATUS_TRUNCATED
               : LONEJSON_STATUS_OK;
  }
  memcpy(ptr, value, len);
  ((char *)ptr)[len] = '\0';
  return LONEJSON_STATUS_OK;
}

static int lonejson__parse_i64_token(const char *value, lonejson_int64 *out) {
  const unsigned char *p;
  lonejson_uint64 max_u64;
  lonejson_uint64 limit;
  lonejson_uint64 acc;
  int negative;

  if (value == NULL || out == NULL || *value == '\0') {
    return 0;
  }
  p = (const unsigned char *)value;
  negative = (*p == '-');
  if (negative) {
    p++;
    if (*p == '\0') {
      return 0;
    }
  }
  if (*p == '0' && p[1] != '\0') {
    return 0;
  }
  max_u64 = (lonejson_uint64) ~(lonejson_uint64)0u;
  limit = negative ? ((max_u64 >> 1u) + 1u) : (max_u64 >> 1u);
  acc = 0u;
  while (*p != '\0') {
    lonejson_uint64 digit;

    if (*p < '0' || *p > '9') {
      return 0;
    }
    digit = (lonejson_uint64)(*p - '0');
    if (acc > (limit - digit) / 10u) {
      return 0;
    }
    acc = (acc * 10u) + digit;
    p++;
  }
  if (negative) {
    if (acc == ((max_u64 >> 1u) + 1u)) {
      *out = (lonejson_int64)(-((lonejson_int64)(acc - 1u)) - 1);
    } else {
      *out = (lonejson_int64)(-((lonejson_int64)acc));
    }
  } else {
    *out = (lonejson_int64)acc;
  }
  return 1;
}

static int lonejson__parse_u64_token(const char *value, lonejson_uint64 *out) {
  const unsigned char *p;
  lonejson_uint64 max_u64;
  lonejson_uint64 acc;

  if (value == NULL || out == NULL || *value == '\0' || *value == '-') {
    return 0;
  }
  p = (const unsigned char *)value;
  if (*p == '0' && p[1] != '\0') {
    return 0;
  }
  max_u64 = (lonejson_uint64) ~(lonejson_uint64)0u;
  acc = 0u;
  while (*p != '\0') {
    lonejson_uint64 digit;

    if (*p < '0' || *p > '9') {
      return 0;
    }
    digit = (lonejson_uint64)(*p - '0');
    if (acc > (max_u64 - digit) / 10u) {
      return 0;
    }
    acc = (acc * 10u) + digit;
    p++;
  }
  *out = acc;
  return 1;
}

static int lonejson__parse_f64_fast(const char *value, size_t len,
                                    double *out) {
  const unsigned char *p;
  const unsigned char *endp;
  double int_part;
  double frac_part;
  double scale;
  unsigned exponent;
  int exponent_negative;
  int negative;

  if (value == NULL || out == NULL || len == 0u || *value == '\0') {
    return 0;
  }
  p = (const unsigned char *)value;
  endp = p + len;
  negative = (*p == '-');
  if (negative) {
    p++;
    if (p == endp || *p == '\0') {
      return 0;
    }
  }
  if (p == endp) {
    return 0;
  }
  if (!lonejson__is_digit(*p)) {
    return 0;
  }
  if (*p == '0' && p + 1u < endp && p[1] != '.' && p[1] != 'e' && p[1] != 'E') {
    return 0;
  }
  int_part = 0.0;
  while (p < endp && lonejson__is_digit(*p)) {
    int_part = (int_part * 10.0) + (double)(*p - '0');
    p++;
  }
  frac_part = 0.0;
  if (p < endp && *p == '.') {
    unsigned frac_digits;

    p++;
    frac_digits = 0u;
    if (p == endp || !lonejson__is_digit(*p)) {
      return 0;
    }
    while (p < endp && lonejson__is_digit(*p)) {
      frac_part = (frac_part * 10.0) + (double)(*p - '0');
      frac_digits++;
      p++;
      if (frac_digits >= 17u && p < endp && lonejson__is_digit(*p)) {
        return 0;
      }
    }
    while (frac_digits-- != 0u) {
      frac_part /= 10.0;
    }
  }
  exponent = 0u;
  exponent_negative = 0;
  if (p < endp && (*p == 'e' || *p == 'E')) {
    p++;
    if (p == endp) {
      return 0;
    }
    if (*p == '+' || *p == '-') {
      exponent_negative = (*p == '-');
      p++;
      if (p == endp) {
        return 0;
      }
    }
    if (!lonejson__is_digit(*p)) {
      return 0;
    }
    while (p < endp && lonejson__is_digit(*p)) {
      if (exponent > 10000u) {
        return 0;
      }
      exponent = (exponent * 10u) + (unsigned)(*p - '0');
      p++;
    }
  }
  if (p != endp || *p != '\0') {
    return 0;
  }
  scale = int_part + frac_part;
  if (exponent != 0u) {
    double base;

    base = exponent_negative ? 0.1 : 10.0;
    while (exponent != 0u) {
      if ((exponent & 1u) != 0u) {
        scale *= base;
      }
      exponent >>= 1u;
      if (exponent != 0u) {
        base *= base;
      }
    }
    if (!lonejson__is_finite_f64(scale)) {
      return 0;
    }
  }
  *out = negative ? -scale : scale;
  return 1;
}

static lonejson_status lonejson__assign_i64(lonejson_parser *parser,
                                            const lonejson_field *field,
                                            void *ptr, const char *value) {
  lonejson_int64 parsed;

  (void)field;
  if (!lonejson__parse_i64_token(value, &parsed)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "expected integer for '%s'", field->json_key);
  }
  *(lonejson_int64 *)ptr = (lonejson_int64)parsed;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__assign_u64(lonejson_parser *parser,
                                            const lonejson_field *field,
                                            void *ptr, const char *value) {
  lonejson_uint64 parsed;

  if (!lonejson__parse_u64_token(value, &parsed)) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "expected unsigned integer for '%s'", field->json_key);
  }
  *(lonejson_uint64 *)ptr = (lonejson_uint64)parsed;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__assign_f64(lonejson_parser *parser,
                                            const lonejson_field *field,
                                            void *ptr, const char *value,
                                            size_t len) {
  char *end = NULL;
  double parsed;

  if (!lonejson__parse_f64_fast(value, len, &parsed)) {
    if (!lonejson__is_valid_json_number(value, len)) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected number for '%s'", field->json_key);
    }
    errno = 0;
    parsed = strtod(value, &end);
    if (errno != 0 || end == value || *end != '\0') {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected number for '%s'", field->json_key);
    }
  }
  *(double *)ptr = parsed;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__assign_bool(lonejson_parser *parser,
                                             const lonejson_field *field,
                                             void *ptr, int value) {
  (void)parser;
  (void)field;
  *(bool *)ptr = value ? true : false;
  return LONEJSON_STATUS_OK;
}

static int lonejson__mark_field_seen(lonejson_parser *parser,
                                     lonejson_frame *frame,
                                     const lonejson_field *field) {
  size_t idx;
  lonejson_uint64 bit;
  if (frame == NULL || frame->map == NULL || field == NULL) {
    return 1;
  }
  idx = lonejson__field_index(frame->map, field);
  if (idx == SIZE_MAX) {
    return 1;
  }
  if (frame->map->field_count <= 64u) {
    bit = ((lonejson_uint64)1u) << idx;
    if (parser->options.reject_duplicate_keys &&
        (frame->seen_mask & bit) != 0u) {
      lonejson__set_error(&parser->error, LONEJSON_STATUS_DUPLICATE_FIELD,
                          parser->error.offset, parser->error.line,
                          parser->error.column, "duplicate field '%s'",
                          field->json_key);
      parser->failed = 1;
      return 0;
    }
    if ((frame->seen_mask & bit) == 0u &&
        (field->flags & LONEJSON_FIELD_REQUIRED) != 0u &&
        frame->required_remaining != 0u) {
      frame->required_remaining--;
    }
    frame->seen_mask |= bit;
    return 1;
  }
  if (frame->seen_fields == NULL && frame->map->field_count != 0u) {
    frame->seen_fields =
        lonejson__workspace_alloc_top(parser, frame->map->field_count);
    if (frame->seen_fields == NULL) {
      lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                          parser->error.offset, parser->error.line,
                          parser->error.column,
                          "parser workspace exhausted by object bookkeeping");
      parser->failed = 1;
      return 0;
    }
    frame->seen_bytes = lonejson__workspace_align(frame->map->field_count);
    memset(frame->seen_fields, 0, frame->map->field_count);
  }
  if (parser->options.reject_duplicate_keys && frame->seen_fields[idx] != 0u) {
    lonejson__set_error(&parser->error, LONEJSON_STATUS_DUPLICATE_FIELD,
                        parser->error.offset, parser->error.line,
                        parser->error.column, "duplicate field '%s'",
                        field->json_key);
    parser->failed = 1;
    return 0;
  }
  if (frame->seen_fields[idx] == 0u &&
      (field->flags & LONEJSON_FIELD_REQUIRED) != 0u &&
      frame->required_remaining != 0u) {
    frame->required_remaining--;
  }
  frame->seen_fields[idx] = 1u;
  return 1;
}

static lonejson_status lonejson__assign_null(lonejson_parser *parser,
                                             const lonejson_field *field,
                                             void *ptr) {
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      char **dst = (char **)ptr;
      LONEJSON_FREE(*dst);
      *dst = NULL;
    } else if (field->kind == LONEJSON_FIELD_KIND_STRING &&
               field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
    } else {
      lonejson_spooled_reset((lonejson_spooled *)ptr);
    }
    return LONEJSON_STATUS_OK;
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    lonejson_source_reset((lonejson_source *)ptr);
    return LONEJSON_STATUS_OK;
  case LONEJSON_FIELD_KIND_OBJECT:
    if (field->submap != NULL) {
      lonejson__cleanup_map(field->submap, ptr);
      memset(ptr, 0, field->submap->struct_size);
    }
    return LONEJSON_STATUS_OK;
  case LONEJSON_FIELD_KIND_STRING_ARRAY:
  case LONEJSON_FIELD_KIND_I64_ARRAY:
  case LONEJSON_FIELD_KIND_U64_ARRAY:
  case LONEJSON_FIELD_KIND_F64_ARRAY:
  case LONEJSON_FIELD_KIND_BOOL_ARRAY:
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
    lonejson__cleanup_value(field, ptr);
    return LONEJSON_STATUS_OK;
  default:
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "null is not allowed for '%s'", field->json_key);
  }
}

static lonejson_status lonejson__array_append_string(
    lonejson_parser *parser, const lonejson_field *field,
    lonejson_string_array *arr, const char *value, size_t len) {
  char *copy;

  if (memchr(value, '\0', len) != NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "array '%s' contains embedded NUL unsupported by C strings",
        field->json_key);
  }
  if (!lonejson__array_ensure_bytes(
          (void **)&arr->items, &arr->capacity, sizeof(char *), &arr->flags,
          arr->count + 1u, field->overflow_policy, &parser->error)) {
    return (field->overflow_policy == LONEJSON_OVERFLOW_FAIL)
               ? parser->error.code
               : LONEJSON_STATUS_OK;
  }
  if (arr->count >= arr->capacity) {
    parser->error.truncated = 1;
    return (field->overflow_policy == LONEJSON_OVERFLOW_TRUNCATE)
               ? LONEJSON_STATUS_TRUNCATED
               : LONEJSON_STATUS_OK;
  }
  copy = (char *)LONEJSON_MALLOC(len + 1u);
  if (copy == NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED, parser->error.offset,
        parser->error.line, parser->error.column,
        "failed to allocate array string");
  }
  memcpy(copy, value, len);
  copy[len] = '\0';
  arr->items[arr->count++] = copy;
  return LONEJSON_STATUS_OK;
}

#define LONEJSON__DEFINE_ARRAY_APPEND(name, array_type, elem_type)             \
  static lonejson_status name(lonejson_parser *parser,                         \
                              const lonejson_field *field, array_type *arr,    \
                              elem_type value) {                               \
    if (!lonejson__array_ensure_bytes((void **)&arr->items, &arr->capacity,    \
                                      sizeof(elem_type), &arr->flags,          \
                                      arr->count + 1u, field->overflow_policy, \
                                      &parser->error)) {                       \
      return (field->overflow_policy == LONEJSON_OVERFLOW_FAIL)                \
                 ? parser->error.code                                          \
                 : LONEJSON_STATUS_OK;                                         \
    }                                                                          \
    if (arr->count >= arr->capacity) {                                         \
      parser->error.truncated = 1;                                             \
      return (field->overflow_policy == LONEJSON_OVERFLOW_TRUNCATE)            \
                 ? LONEJSON_STATUS_TRUNCATED                                   \
                 : LONEJSON_STATUS_OK;                                         \
    }                                                                          \
    arr->items[arr->count++] = value;                                          \
    return LONEJSON_STATUS_OK;                                                 \
  }

LONEJSON__DEFINE_ARRAY_APPEND(lonejson__array_append_i64, lonejson_i64_array,
                              lonejson_int64)
LONEJSON__DEFINE_ARRAY_APPEND(lonejson__array_append_u64, lonejson_u64_array,
                              lonejson_uint64)
LONEJSON__DEFINE_ARRAY_APPEND(lonejson__array_append_f64, lonejson_f64_array,
                              double)
LONEJSON__DEFINE_ARRAY_APPEND(lonejson__array_append_bool, lonejson_bool_array,
                              bool)

static void *lonejson__object_array_append_slot(lonejson_parser *parser,
                                                const lonejson_field *field,
                                                lonejson_object_array *arr) {
  void *slot;

  if (arr->elem_size == 0u) {
    arr->elem_size = field->elem_size;
  }
  if (!lonejson__array_ensure_bytes(&arr->items, &arr->capacity, arr->elem_size,
                                    &arr->flags, arr->count + 1u,
                                    field->overflow_policy, &parser->error)) {
    return NULL;
  }
  if (arr->count >= arr->capacity) {
    parser->error.truncated = 1;
    return NULL;
  }
  slot = (unsigned char *)arr->items + (arr->count * arr->elem_size);
  if (field->submap != NULL) {
    lonejson__init_map(field->submap, slot);
  } else {
    memset(slot, 0, arr->elem_size);
  }
  arr->count++;
  return slot;
}

static lonejson_status
lonejson__complete_parent_after_value(lonejson_parser *parser) {
  lonejson_frame *parent;

  if (parser->frame_count == 0u) {
    parser->root_finished = 1;
    return LONEJSON_STATUS_OK;
  }
  parent = &parser->frames[parser->frame_count - 1u];
  if (parent->kind == LONEJSON_CONTAINER_OBJECT) {
    parent->state = LONEJSON_FRAME_OBJECT_COMMA_OR_END;
    parent->pending_field = NULL;
  } else {
    parent->state = LONEJSON_FRAME_ARRAY_COMMA_OR_END;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__handle_scalar_for_field(
    lonejson_parser *parser, lonejson_frame *frame, const lonejson_field *field,
    const char *value, size_t len, lonejson_lex_mode mode) {
  void *ptr;

  if (field == NULL) {
    return lonejson__complete_parent_after_value(parser);
  }
  if (frame != NULL && !lonejson__mark_field_seen(parser, frame, field)) {
    return parser->error.code;
  }
  ptr =
      lonejson__field_ptr(frame ? frame->object_ptr : parser->root_dst, field);
  switch (mode) {
  case LONEJSON_LEX_STRING:
    if (field->kind == LONEJSON_FIELD_KIND_STRING_STREAM ||
        field->kind == LONEJSON_FIELD_KIND_BASE64_STREAM) {
      return LONEJSON_STATUS_OK;
    }
    if (field->kind == LONEJSON_FIELD_KIND_STRING_SOURCE ||
        field->kind == LONEJSON_FIELD_KIND_BASE64_SOURCE) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "field '%s' is serialize-only and cannot be parsed from JSON",
          field->json_key);
    }
    if (field->kind != LONEJSON_FIELD_KIND_STRING) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected string field '%s'", field->json_key);
    }
    return lonejson__assign_string(parser, field, ptr, value, len);
  case LONEJSON_LEX_NUMBER:
    if (field->kind == LONEJSON_FIELD_KIND_I64) {
      return lonejson__assign_i64(parser, field, ptr, value);
    }
    if (field->kind == LONEJSON_FIELD_KIND_U64) {
      return lonejson__assign_u64(parser, field, ptr, value);
    }
    if (field->kind == LONEJSON_FIELD_KIND_F64) {
      return lonejson__assign_f64(parser, field, ptr, value, len);
    }
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "numeric value does not match field '%s'", field->json_key);
  case LONEJSON_LEX_TRUE:
  case LONEJSON_LEX_FALSE:
    if (field->kind != LONEJSON_FIELD_KIND_BOOL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "boolean value does not match field '%s'", field->json_key);
    }
    return lonejson__assign_bool(parser, field, ptr, mode == LONEJSON_LEX_TRUE);
  case LONEJSON_LEX_NULL:
    return lonejson__assign_null(parser, field, ptr);
  default:
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "unexpected scalar mode");
  }
}

static lonejson_status
lonejson__handle_array_scalar(lonejson_parser *parser,
                              lonejson_frame *array_frame, const char *value,
                              size_t len, lonejson_lex_mode mode) {
  void *ptr = lonejson__field_ptr(array_frame->object_ptr, array_frame->field);

  switch (array_frame->field->kind) {
  case LONEJSON_FIELD_KIND_STRING_ARRAY:
    if (mode == LONEJSON_LEX_NULL) {
      return lonejson__array_append_string(
          parser, array_frame->field, (lonejson_string_array *)ptr, "", 0u);
    }
    if (mode != LONEJSON_LEX_STRING) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects strings", array_frame->field->json_key);
    }
    return lonejson__array_append_string(
        parser, array_frame->field, (lonejson_string_array *)ptr, value, len);
  case LONEJSON_FIELD_KIND_I64_ARRAY:
    if (mode != LONEJSON_LEX_NUMBER) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects integers", array_frame->field->json_key);
    } else {
      lonejson_int64 parsed = 0;
      lonejson_status status =
          lonejson__assign_i64(parser, array_frame->field, &parsed, value);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__array_append_i64(parser, array_frame->field,
                                        (lonejson_i64_array *)ptr, parsed);
    }
  case LONEJSON_FIELD_KIND_U64_ARRAY:
    if (mode != LONEJSON_LEX_NUMBER) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects unsigned integers", array_frame->field->json_key);
    } else {
      lonejson_uint64 parsed = 0;
      lonejson_status status =
          lonejson__assign_u64(parser, array_frame->field, &parsed, value);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__array_append_u64(parser, array_frame->field,
                                        (lonejson_u64_array *)ptr, parsed);
    }
  case LONEJSON_FIELD_KIND_F64_ARRAY:
    if (mode != LONEJSON_LEX_NUMBER) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects numbers", array_frame->field->json_key);
    } else {
      double parsed = 0.0;
      lonejson_status status =
          lonejson__assign_f64(parser, array_frame->field, &parsed, value, len);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__array_append_f64(parser, array_frame->field,
                                        (lonejson_f64_array *)ptr, parsed);
    }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY:
    if (mode != LONEJSON_LEX_TRUE && mode != LONEJSON_LEX_FALSE) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects booleans", array_frame->field->json_key);
    }
    return lonejson__array_append_bool(parser, array_frame->field,
                                       (lonejson_bool_array *)ptr,
                                       mode == LONEJSON_LEX_TRUE);
  default:
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "field '%s' does not accept scalar array values",
                               array_frame->field->json_key);
  }
}

static lonejson_status lonejson__begin_object_value(lonejson_parser *parser) {
  lonejson_frame *parent = (parser->frame_count != 0u)
                               ? &parser->frames[parser->frame_count - 1u]
                               : NULL;
  const lonejson_field *field = NULL;
  void *object_ptr = NULL;
  const lonejson_map *map = NULL;
  lonejson_frame *frame;

  if (!parser->root_started) {
    parser->root_started = 1;
    map = parser->validate_only ? NULL : parser->root_map;
    object_ptr = parser->validate_only ? NULL : parser->root_dst;
  } else if (parent != NULL && parent->kind == LONEJSON_CONTAINER_OBJECT) {
    parent->after_comma = 0;
    field = parent->pending_field;
    if (field == NULL) {
      map = NULL;
      object_ptr = NULL;
    } else {
      if (!lonejson__mark_field_seen(parser, parent, field)) {
        return parser->error.code;
      }
      if (field->kind != LONEJSON_FIELD_KIND_OBJECT) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
            parser->error.line, parser->error.column,
            "field '%s' expects non-object value", field->json_key);
      }
      map = field->submap;
      object_ptr = lonejson__field_ptr(parent->object_ptr, field);
      lonejson__init_map(map, object_ptr);
    }
  } else if (parent != NULL && parent->kind == LONEJSON_CONTAINER_ARRAY) {
    parent->after_comma = 0;
    if (parent->field == NULL) {
      map = NULL;
      object_ptr = NULL;
    } else {
      lonejson_object_array *arr;
      if (parent->field->kind != LONEJSON_FIELD_KIND_OBJECT_ARRAY) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
            parser->error.line, parser->error.column,
            "array '%s' expects scalar elements", parent->field->json_key);
      }
      arr = (lonejson_object_array *)lonejson__field_ptr(parent->object_ptr,
                                                         parent->field);
      object_ptr =
          lonejson__object_array_append_slot(parser, parent->field, arr);
      if (object_ptr == NULL) {
        if (parent->field->overflow_policy == LONEJSON_OVERFLOW_FAIL) {
          return parser->error.code;
        }
        parser->error.truncated = 1;
        map = NULL;
      } else {
        map = parent->field->submap;
      }
    }
  }

  frame = lonejson__push_frame(parser, LONEJSON_CONTAINER_OBJECT);
  if (frame == NULL) {
    return parser->error.code;
  }
  frame->map = map;
  frame->object_ptr = object_ptr;
  frame->field = field;
  frame->required_remaining = lonejson__required_count_for_map(parser, map);
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__begin_array_value(lonejson_parser *parser) {
  lonejson_frame *parent = (parser->frame_count != 0u)
                               ? &parser->frames[parser->frame_count - 1u]
                               : NULL;
  const lonejson_field *field = NULL;
  void *object_ptr = NULL;
  lonejson_frame *frame;

  if (!parser->root_started) {
    if (!parser->validate_only) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "root value must be an object");
    }
    parser->root_started = 1;
  }
  if (parent != NULL && parent->kind == LONEJSON_CONTAINER_OBJECT) {
    parent->after_comma = 0;
    field = parent->pending_field;
    object_ptr = parent->object_ptr;
    if (field != NULL) {
      if (!lonejson__mark_field_seen(parser, parent, field)) {
        return parser->error.code;
      }
      switch (field->kind) {
      case LONEJSON_FIELD_KIND_STRING_ARRAY:
      case LONEJSON_FIELD_KIND_I64_ARRAY:
      case LONEJSON_FIELD_KIND_U64_ARRAY:
      case LONEJSON_FIELD_KIND_F64_ARRAY:
      case LONEJSON_FIELD_KIND_BOOL_ARRAY:
      case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
        break;
      default:
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
            parser->error.line, parser->error.column,
            "field '%s' is not an array", field->json_key);
      }
    }
  } else if (parent != NULL && parent->kind == LONEJSON_CONTAINER_ARRAY) {
    parent->after_comma = 0;
    if (parent->field != NULL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "field '%s' does not accept nested arrays", parent->field->json_key);
    }
    field = NULL;
    object_ptr = NULL;
  }

  frame = lonejson__push_frame(parser, LONEJSON_CONTAINER_ARRAY);
  if (frame == NULL) {
    return parser->error.code;
  }
  frame->map = NULL;
  frame->object_ptr = object_ptr;
  frame->field = field;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__finalize_object(lonejson_parser *parser) {
  lonejson_frame *frame = &parser->frames[parser->frame_count - 1u];
  size_t i;
  lonejson_uint64 bit;
  lonejson_uint64 required_mask;

  if (frame->map != NULL && frame->required_remaining == 0u) {
    lonejson__pop_frame(parser);
    return lonejson__complete_parent_after_value(parser);
  }

  if (frame->map != NULL && frame->map->field_count <= 64u) {
    required_mask = lonejson__required_mask_for_map(parser, frame->map);
    if ((frame->seen_mask & required_mask) != required_mask) {
      for (i = 0u; i < frame->map->field_count; ++i) {
        bit = ((lonejson_uint64)1u) << i;
        if ((required_mask & bit) != 0u && (frame->seen_mask & bit) == 0u) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
              parser->error.offset, parser->error.line, parser->error.column,
              "missing required field '%s'", frame->map->fields[i].json_key);
        }
      }
    }
  } else if (frame->map != NULL && frame->seen_fields != NULL) {
    for (i = 0u; i < frame->map->field_count; ++i) {
      if ((frame->map->fields[i].flags & LONEJSON_FIELD_REQUIRED) != 0u &&
          frame->seen_fields[i] == 0u) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
            parser->error.offset, parser->error.line, parser->error.column,
            "missing required field '%s'", frame->map->fields[i].json_key);
      }
    }
  } else if (frame->map != NULL) {
    for (i = 0u; i < frame->map->field_count; ++i) {
      if ((frame->map->fields[i].flags & LONEJSON_FIELD_REQUIRED) != 0u) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
            parser->error.offset, parser->error.line, parser->error.column,
            "missing required field '%s'", frame->map->fields[i].json_key);
      }
    }
  }

  lonejson__pop_frame(parser);
  return lonejson__complete_parent_after_value(parser);
}

static lonejson_status lonejson__finalize_array(lonejson_parser *parser) {
  lonejson__pop_frame(parser);
  return lonejson__complete_parent_after_value(parser);
}

static int lonejson__mapped_numeric_target(const lonejson_frame *frame) {
  const lonejson_field *field;

  if (frame == NULL) {
    return 0;
  }
  if (frame->kind == LONEJSON_CONTAINER_OBJECT) {
    field = frame->pending_field;
    return field != NULL && (field->kind == LONEJSON_FIELD_KIND_I64 ||
                             field->kind == LONEJSON_FIELD_KIND_U64 ||
                             field->kind == LONEJSON_FIELD_KIND_F64);
  }
  if (frame->kind == LONEJSON_CONTAINER_ARRAY) {
    field = frame->field;
    return field != NULL && (field->kind == LONEJSON_FIELD_KIND_I64_ARRAY ||
                             field->kind == LONEJSON_FIELD_KIND_U64_ARRAY ||
                             field->kind == LONEJSON_FIELD_KIND_F64_ARRAY);
  }
  return 0;
}

static lonejson_status lonejson__deliver_token(lonejson_parser *parser,
                                               lonejson_lex_mode mode) {
  lonejson_frame *frame = (parser->frame_count != 0u)
                              ? &parser->frames[parser->frame_count - 1u]
                              : NULL;
  const char *token_text;

  token_text = lonejson__scratch_cstr(&parser->token);

  if (parser->lex_is_key) {
    if (frame == NULL || frame->kind != LONEJSON_CONTAINER_OBJECT ||
        frame->state != LONEJSON_FRAME_OBJECT_KEY_OR_END) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "object key outside object context");
    }
    frame->pending_field =
        lonejson__find_field(frame->map, frame, token_text, parser->token.len);
    frame->state = LONEJSON_FRAME_OBJECT_COLON;
    return LONEJSON_STATUS_OK;
  }

  if (frame == NULL) {
    if (!parser->root_started && parser->validate_only) {
      parser->root_started = 1;
      parser->root_finished = 1;
      return LONEJSON_STATUS_OK;
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "unexpected scalar at document root");
  }

  if (mode == LONEJSON_LEX_NUMBER &&
      (parser->validate_only || !lonejson__mapped_numeric_target(frame)) &&
      !lonejson__is_valid_json_number(token_text, parser->token.len)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid JSON number");
  }

  if (frame->kind == LONEJSON_CONTAINER_OBJECT) {
    lonejson_status status =
        lonejson__handle_scalar_for_field(parser, frame, frame->pending_field,
                                          token_text, parser->token.len, mode);
    if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
      return lonejson__complete_parent_after_value(parser);
    }
    return status;
  }

  if (frame->kind == LONEJSON_CONTAINER_ARRAY) {
    lonejson_status status;
    if (frame->field == NULL) {
      frame->after_comma = 0;
      frame->state = LONEJSON_FRAME_ARRAY_COMMA_OR_END;
      return LONEJSON_STATUS_OK;
    }
    status = lonejson__handle_array_scalar(parser, frame, token_text,
                                           parser->token.len, mode);
    if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
      frame->state = LONEJSON_FRAME_ARRAY_COMMA_OR_END;
    }
    return status;
  }

  return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                             parser->error.offset, parser->error.line,
                             parser->error.column, "invalid parser state");
}

static int lonejson__hex_value(int ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return 10 + (ch - 'a');
  }
  if (ch >= 'A' && ch <= 'F') {
    return 10 + (ch - 'A');
  }
  return -1;
}

static LONEJSON__INLINE int
lonejson__decode_unicode_quad(const unsigned char *src, lonejson_uint32 *out) {
  lonejson_uint32 cp;
  int hv;

  cp = 0u;
  hv = lonejson__hex_value(src[0]);
  if (hv < 0) {
    return 0;
  }
  cp = (cp << 4u) | (lonejson_uint32)hv;
  hv = lonejson__hex_value(src[1]);
  if (hv < 0) {
    return 0;
  }
  cp = (cp << 4u) | (lonejson_uint32)hv;
  hv = lonejson__hex_value(src[2]);
  if (hv < 0) {
    return 0;
  }
  cp = (cp << 4u) | (lonejson_uint32)hv;
  hv = lonejson__hex_value(src[3]);
  if (hv < 0) {
    return 0;
  }
  *out = (cp << 4u) | (lonejson_uint32)hv;
  return 1;
}

static LONEJSON__INLINE lonejson_status lonejson__append_unicode_codepoint(
    lonejson_parser *parser, lonejson_uint32 cp) {
  if (parser->stream_value_active) {
    unsigned char utf8[4];
    lonejson_scratch scratch;
    lonejson_status status;

    memset(&scratch, 0, sizeof(scratch));
    scratch.data = (char *)utf8;
    scratch.cap = sizeof(utf8);
    if (!lonejson__utf8_append(&scratch, cp)) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "failed to encode unicode escape");
    }
    status = lonejson__stream_value_append(parser, (const unsigned char *)utf8,
                                           scratch.len);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    return LONEJSON_STATUS_OK;
  }
  if (!lonejson__utf8_append(&parser->token, cp)) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED, parser->error.offset,
        parser->error.line, parser->error.column,
        "failed to append unicode escape");
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status
lonejson__append_string_byte(lonejson_parser *parser, unsigned char ch) {
  if (parser->stream_value_active) {
    return lonejson__stream_value_append(parser, &ch, 1u);
  }
  return lonejson__scratch_append(&parser->token, (char)ch)
             ? LONEJSON_STATUS_OK
             : lonejson__set_error(
                   &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                   parser->error.offset, parser->error.line,
                   parser->error.column, "failed to append string");
}

static LONEJSON__INLINE lonejson_status lonejson__consume_simple_escape_fast(
    lonejson_parser *parser, const unsigned char *bytes, size_t avail,
    size_t *used) {
  unsigned char ch;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL || avail < 2u || bytes[0] != '\\' ||
      parser->unicode_pending_high != 0u) {
    return LONEJSON_STATUS_OK;
  }
  switch (bytes[1]) {
  case '"':
    ch = '"';
    break;
  case '\\':
    ch = '\\';
    break;
  case '/':
    ch = '/';
    break;
  case 'b':
    ch = '\b';
    break;
  case 'f':
    ch = '\f';
    break;
  case 'n':
    ch = '\n';
    break;
  case 'r':
    ch = '\r';
    break;
  case 't':
    ch = '\t';
    break;
  default:
    return LONEJSON_STATUS_OK;
  }
  if (used != NULL) {
    *used = 2u;
  }
  parser->error.offset += 2u;
  parser->error.column += 2u;
  lonejson__parser_note_workspace_usage(parser);
  return lonejson__append_string_byte(parser, ch);
}

static LONEJSON__INLINE lonejson_status lonejson__consume_unicode_escape_fast(
    lonejson_parser *parser, const unsigned char *bytes, size_t avail,
    size_t *used) {
  lonejson_uint32 cp;
  size_t consume;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL || avail < 6u || bytes[0] != '\\' ||
      bytes[1] != 'u') {
    return LONEJSON_STATUS_OK;
  }
  if (!lonejson__decode_unicode_quad(bytes + 2u, &cp)) {
    return LONEJSON_STATUS_OK;
  }
  consume = 6u;
  if (parser->unicode_pending_high != 0u) {
    if (cp < 0xDC00u || cp > 0xDFFFu) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid unicode surrogate pair");
    }
    cp = 0x10000u +
         (((parser->unicode_pending_high - 0xD800u) << 10u) | (cp - 0xDC00u));
    parser->unicode_pending_high = 0u;
  } else if (cp >= 0xD800u && cp <= 0xDBFFu) {
    lonejson_uint32 low;

    if (avail < 12u || bytes[6] != '\\' || bytes[7] != 'u' ||
        !lonejson__decode_unicode_quad(bytes + 8u, &low)) {
      return LONEJSON_STATUS_OK;
    }
    if (low < 0xDC00u || low > 0xDFFFu) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid unicode surrogate pair");
    }
    cp = 0x10000u + (((cp - 0xD800u) << 10u) | (low - 0xDC00u));
    consume = 12u;
  } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "unexpected low surrogate");
  }
  if (used != NULL) {
    *used = consume;
  }
  parser->error.offset += consume;
  parser->error.column += consume;
  lonejson__parser_note_workspace_usage(parser);
  return lonejson__append_unicode_codepoint(parser, cp);
}

static LONEJSON__INLINE lonejson_status lonejson__consume_string_fast(
    lonejson_parser *parser, const unsigned char *bytes, size_t avail,
    size_t *used) {
  size_t pos;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  pos = 0u;
  while (pos < avail) {
    size_t start;

    start = pos;
    while (pos < avail && bytes[pos] != '"' && bytes[pos] != '\\' &&
           bytes[pos] >= 0x20u) {
      pos++;
    }
    if (pos != start) {
      if (parser->stream_value_active) {
        lonejson_status status =
            lonejson__stream_value_append(parser, bytes + start, pos - start);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      } else if (!lonejson__scratch_append_bytes(&parser->token, bytes + start,
                                                 pos - start)) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
            parser->error.offset, parser->error.line, parser->error.column,
            "failed to append string");
      }
      parser->error.offset += pos - start;
      parser->error.column += pos - start;
      lonejson__parser_note_workspace_usage(parser);
      if (pos == avail) {
        break;
      }
    }

    if (bytes[pos] == '"') {
      lonejson_status status;

      if (parser->unicode_pending_high != 0u) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "unterminated unicode surrogate pair");
      }
      parser->error.offset++;
      parser->error.column++;
      parser->lex_mode = LONEJSON_LEX_NONE;
      if (parser->stream_value_active) {
        status = lonejson__stream_value_finish(parser);
        if (status == LONEJSON_STATUS_OK) {
          status = lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
        }
      } else {
        status = lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
      }
      if (used != NULL) {
        *used = pos + 1u;
      }
      return status;
    }

    if (bytes[pos] == '\\') {
      size_t consumed;
      lonejson_status status;

      status = lonejson__consume_simple_escape_fast(parser, bytes + pos,
                                                    avail - pos, &consumed);
      if (status != LONEJSON_STATUS_OK) {
        if (used != NULL) {
          *used = pos + consumed;
        }
        return status;
      }
      if (consumed != 0u) {
        pos += consumed;
        continue;
      }
      status = lonejson__consume_unicode_escape_fast(parser, bytes + pos,
                                                     avail - pos, &consumed);
      if (status != LONEJSON_STATUS_OK) {
        if (used != NULL) {
          *used = pos + consumed;
        }
        return status;
      }
      if (consumed != 0u) {
        pos += consumed;
        continue;
      }
      break;
    }

    if (bytes[pos] < 0x20u) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "control character in string literal");
    }
  }
  if (used != NULL) {
    *used = pos;
  }
  return LONEJSON_STATUS_OK;
}

static int lonejson__is_valid_json_number(const char *value, size_t len) {
  size_t i = 0u;

  if (len == 0u) {
    return 0;
  }
  if (value[i] == '-') {
    i++;
    if (i == len) {
      return 0;
    }
  }
  if (value[i] == '0') {
    i++;
    if (i < len && lonejson__is_digit((unsigned char)value[i])) {
      return 0;
    }
  } else {
    if (!lonejson__is_digit((unsigned char)value[i])) {
      return 0;
    }
    while (i < len && lonejson__is_digit((unsigned char)value[i])) {
      i++;
    }
  }
  if (i < len && value[i] == '.') {
    i++;
    if (i == len || !lonejson__is_digit((unsigned char)value[i])) {
      return 0;
    }
    while (i < len && lonejson__is_digit((unsigned char)value[i])) {
      i++;
    }
  }
  if (i < len && (value[i] == 'e' || value[i] == 'E')) {
    i++;
    if (i < len && (value[i] == '+' || value[i] == '-')) {
      i++;
    }
    if (i == len || !lonejson__is_digit((unsigned char)value[i])) {
      return 0;
    }
    while (i < len && lonejson__is_digit((unsigned char)value[i])) {
      i++;
    }
  }
  return i == len;
}

static lonejson_status lonejson__parser_consume_char(lonejson_parser *parser,
                                                     unsigned char ch) {
  lonejson_frame *frame = (parser->frame_count != 0u)
                              ? &parser->frames[parser->frame_count - 1u]
                              : NULL;

  parser->error.offset++;
  if (ch == '\n') {
    parser->error.line++;
    parser->error.column = 0u;
  } else {
    parser->error.column++;
  }

  if (parser->lex_mode == LONEJSON_LEX_STRING) {
    if (parser->unicode_digits_needed != 0) {
      int hv = lonejson__hex_value(ch);
      if (hv < 0) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
            parser->error.line, parser->error.column, "invalid unicode escape");
      }
      parser->unicode_accum =
          (parser->unicode_accum << 4u) | (lonejson_uint32)hv;
      parser->unicode_digits_needed--;
      if (parser->unicode_digits_needed == 0) {
        lonejson_uint32 cp = parser->unicode_accum;
        if (parser->unicode_pending_high != 0u) {
          if (cp < 0xDC00u || cp > 0xDFFFu) {
            return lonejson__set_error(
                &parser->error, LONEJSON_STATUS_INVALID_JSON,
                parser->error.offset, parser->error.line, parser->error.column,
                "invalid unicode surrogate pair");
          }
          cp = 0x10000u + (((parser->unicode_pending_high - 0xD800u) << 10u) |
                           (cp - 0xDC00u));
          parser->unicode_pending_high = 0u;
          if (parser->stream_value_active) {
            unsigned char utf8[4];
            lonejson_scratch scratch;
            lonejson_status status;

            memset(&scratch, 0, sizeof(scratch));
            scratch.data = (char *)utf8;
            scratch.cap = sizeof(utf8);
            if (!lonejson__utf8_append(&scratch, cp)) {
              return lonejson__set_error(
                  &parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                  parser->error.offset, parser->error.line,
                  parser->error.column, "failed to encode unicode escape");
            }
            status = lonejson__stream_value_append(
                parser, (const unsigned char *)utf8, scratch.len);
            if (status != LONEJSON_STATUS_OK) {
              return status;
            }
          } else if (!lonejson__utf8_append(&parser->token, cp)) {
            return lonejson__set_error(
                &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                parser->error.offset, parser->error.line, parser->error.column,
                "failed to append unicode escape");
          }
        } else if (cp >= 0xD800u && cp <= 0xDBFFu) {
          parser->unicode_pending_high = cp;
        } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_INVALID_JSON,
              parser->error.offset, parser->error.line, parser->error.column,
              "unexpected low surrogate");
        } else if (parser->stream_value_active) {
          unsigned char utf8[4];
          lonejson_scratch scratch;
          lonejson_status status;

          memset(&scratch, 0, sizeof(scratch));
          scratch.data = (char *)utf8;
          scratch.cap = sizeof(utf8);
          if (!lonejson__utf8_append(&scratch, cp)) {
            return lonejson__set_error(
                &parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                parser->error.offset, parser->error.line, parser->error.column,
                "failed to encode unicode escape");
          }
          status = lonejson__stream_value_append(
              parser, (const unsigned char *)utf8, scratch.len);
          if (status != LONEJSON_STATUS_OK) {
            return status;
          }
        } else if (!lonejson__utf8_append(&parser->token, cp)) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
              parser->error.offset, parser->error.line, parser->error.column,
              "failed to append unicode escape");
        }
      }
      return LONEJSON_STATUS_OK;
    }
    if (parser->lex_escape) {
      parser->lex_escape = 0;
      switch (ch) {
      case '"':
        if (parser->stream_value_active) {
          return lonejson__stream_value_append(parser,
                                               (const unsigned char *)"\"", 1u);
        }
        return lonejson__scratch_append(&parser->token, '"')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append string");
      case '\\':
        if (parser->stream_value_active) {
          return lonejson__stream_value_append(parser,
                                               (const unsigned char *)"\\", 1u);
        }
        return lonejson__scratch_append(&parser->token, '\\')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append string");
      case '/':
        if (parser->stream_value_active) {
          return lonejson__stream_value_append(parser,
                                               (const unsigned char *)"/", 1u);
        }
        return lonejson__scratch_append(&parser->token, '/')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append string");
      case 'b':
        if (parser->stream_value_active) {
          static const unsigned char backspace = '\b';
          return lonejson__stream_value_append(parser, &backspace, 1u);
        }
        return lonejson__scratch_append(&parser->token, '\b')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append string");
      case 'f':
        if (parser->stream_value_active) {
          static const unsigned char formfeed = '\f';
          return lonejson__stream_value_append(parser, &formfeed, 1u);
        }
        return lonejson__scratch_append(&parser->token, '\f')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append string");
      case 'n':
        if (parser->stream_value_active) {
          static const unsigned char newline = '\n';
          return lonejson__stream_value_append(parser, &newline, 1u);
        }
        return lonejson__scratch_append(&parser->token, '\n')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append string");
      case 'r':
        if (parser->stream_value_active) {
          static const unsigned char carriage = '\r';
          return lonejson__stream_value_append(parser, &carriage, 1u);
        }
        return lonejson__scratch_append(&parser->token, '\r')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append string");
      case 't':
        if (parser->stream_value_active) {
          static const unsigned char tab = '\t';
          return lonejson__stream_value_append(parser, &tab, 1u);
        }
        return lonejson__scratch_append(&parser->token, '\t')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append string");
      case 'u':
        parser->unicode_digits_needed = 4;
        parser->unicode_accum = 0u;
        return LONEJSON_STATUS_OK;
      default:
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "invalid escape sequence");
      }
    }
    if (ch == '\\') {
      parser->lex_escape = 1;
      return LONEJSON_STATUS_OK;
    }
    if (ch == '"') {
      parser->lex_mode = LONEJSON_LEX_NONE;
      if (parser->unicode_pending_high != 0u) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "unterminated unicode surrogate pair");
      }
      if (parser->stream_value_active) {
        return lonejson__stream_value_finish(parser) == LONEJSON_STATUS_OK
                   ? lonejson__deliver_token(parser, LONEJSON_LEX_STRING)
                   : parser->error.code;
      }
      return lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
    }
    if (ch < 0x20u) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "control character in string");
    }
    if (parser->stream_value_active) {
      return lonejson__stream_value_append(parser, &ch, 1u);
    }
    return lonejson__scratch_append(&parser->token, (char)ch)
               ? LONEJSON_STATUS_OK
               : lonejson__set_error(
                     &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                     parser->error.offset, parser->error.line,
                     parser->error.column, "failed to append string");
  }

  if (parser->lex_mode == LONEJSON_LEX_NUMBER) {
    if (lonejson__is_digit(ch) || ch == '+' || ch == '-' || ch == '.' ||
        ch == 'e' || ch == 'E') {
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append number");
    }
    parser->lex_mode = LONEJSON_LEX_NONE;
    if (lonejson__deliver_token(parser, LONEJSON_LEX_NUMBER) !=
        LONEJSON_STATUS_OK) {
      return parser->error.code;
    }
    return lonejson__parser_consume_char(parser, ch);
  }

  if (parser->lex_mode == LONEJSON_LEX_TRUE) {
    const char *literal = "true";
    size_t pos = parser->token.len;
    if (pos < 4u && ch == (unsigned char)literal[pos]) {
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? ((parser->token.len == 4u)
                        ? (parser->lex_mode = LONEJSON_LEX_NONE,
                           lonejson__deliver_token(parser, LONEJSON_LEX_TRUE))
                        : LONEJSON_STATUS_OK)
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid literal");
  }

  if (parser->lex_mode == LONEJSON_LEX_FALSE) {
    const char *literal = "false";
    size_t pos = parser->token.len;
    if (pos < 5u && ch == (unsigned char)literal[pos]) {
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? ((parser->token.len == 5u)
                        ? (parser->lex_mode = LONEJSON_LEX_NONE,
                           lonejson__deliver_token(parser, LONEJSON_LEX_FALSE))
                        : LONEJSON_STATUS_OK)
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid literal");
  }

  if (parser->lex_mode == LONEJSON_LEX_NULL) {
    const char *literal = "null";
    size_t pos = parser->token.len;
    if (pos < 4u && ch == (unsigned char)literal[pos]) {
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? ((parser->token.len == 4u)
                        ? (parser->lex_mode = LONEJSON_LEX_NONE,
                           lonejson__deliver_token(parser, LONEJSON_LEX_NULL))
                        : LONEJSON_STATUS_OK)
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid literal");
  }

  if (lonejson__is_json_space(ch)) {
    return LONEJSON_STATUS_OK;
  }

  if (!parser->root_started) {
    if (ch == '{') {
      return lonejson__begin_object_value(parser);
    }
    if (ch == '[') {
      return lonejson__begin_array_value(parser);
    }
    if (ch == '"') {
      parser->lex_mode = LONEJSON_LEX_STRING;
      parser->lex_is_key = 0;
      parser->lex_escape = 0;
      parser->unicode_pending_high = 0u;
      parser->unicode_digits_needed = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return parser->validate_only
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "root value must be an object");
    }
    if (ch == '-' || lonejson__is_digit(ch)) {
      parser->lex_mode = LONEJSON_LEX_NUMBER;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      if (!lonejson__scratch_append(&parser->token, (char)ch)) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
            parser->error.offset, parser->error.line, parser->error.column,
            "failed to append number");
      }
      return parser->validate_only
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "root value must be an object");
    }
    if (ch == 't' || ch == 'f' || ch == 'n') {
      parser->lex_mode = (ch == 't')   ? LONEJSON_LEX_TRUE
                         : (ch == 'f') ? LONEJSON_LEX_FALSE
                                       : LONEJSON_LEX_NULL;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      if (!lonejson__scratch_append(&parser->token, (char)ch)) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
            parser->error.offset, parser->error.line, parser->error.column,
            "failed to append literal");
      }
      return parser->validate_only
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "root value must be an object");
    }
    return lonejson__set_error(
        &parser->error,
        parser->validate_only ? LONEJSON_STATUS_INVALID_JSON
                              : LONEJSON_STATUS_TYPE_MISMATCH,
        parser->error.offset, parser->error.line, parser->error.column,
        parser->validate_only ? "expected JSON value"
                              : "root value must be an object");
  }

  if (frame == NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "unexpected trailing data");
  }

  if (frame->kind == LONEJSON_CONTAINER_OBJECT) {
    switch (frame->state) {
    case LONEJSON_FRAME_OBJECT_KEY_OR_END:
      if (ch == '}') {
        if (frame->after_comma) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_INVALID_JSON,
              parser->error.offset, parser->error.line, parser->error.column,
              "trailing comma in object");
        }
        return lonejson__finalize_object(parser);
      }
      if (ch != '"') {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column, "expected object key");
      }
      frame->after_comma = 0;
      parser->lex_mode = LONEJSON_LEX_STRING;
      parser->lex_is_key = 1;
      parser->lex_escape = 0;
      parser->unicode_pending_high = 0u;
      parser->unicode_digits_needed = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return LONEJSON_STATUS_OK;
    case LONEJSON_FRAME_OBJECT_COLON:
      if (ch != ':') {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "expected ':' after object key");
      }
      frame->state = LONEJSON_FRAME_OBJECT_VALUE;
      return LONEJSON_STATUS_OK;
    case LONEJSON_FRAME_OBJECT_VALUE:
      if (ch == '"') {
        lonejson_status status;

        parser->lex_mode = LONEJSON_LEX_STRING;
        parser->lex_is_key = 0;
        parser->lex_escape = 0;
        parser->unicode_pending_high = 0u;
        parser->unicode_digits_needed = 0;
        if (lonejson__field_is_streamed(frame->pending_field)) {
          void *ptr =
              lonejson__field_ptr(frame->object_ptr, frame->pending_field);
          status =
              lonejson__stream_value_begin(parser, frame->pending_field, ptr);
          if (status != LONEJSON_STATUS_OK) {
            return status;
          }
        } else if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return LONEJSON_STATUS_OK;
      }
      if (ch == '{') {
        return lonejson__begin_object_value(parser);
      }
      if (ch == '[') {
        return lonejson__begin_array_value(parser);
      }
      if (ch == '-' || lonejson__is_digit(ch)) {
        parser->lex_mode = LONEJSON_LEX_NUMBER;
        parser->lex_is_key = 0;
        if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return lonejson__scratch_append(&parser->token, (char)ch)
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append number");
      }
      if (ch == 't') {
        parser->lex_mode = LONEJSON_LEX_TRUE;
        parser->lex_is_key = 0;
        if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return lonejson__scratch_append(&parser->token, 't')
                   ? ((parser->token.len == 4u)
                          ? (parser->lex_mode = LONEJSON_LEX_NONE,
                             lonejson__deliver_token(parser, LONEJSON_LEX_TRUE))
                          : LONEJSON_STATUS_OK)
                   : LONEJSON_STATUS_ALLOCATION_FAILED;
      }
      if (ch == 'f') {
        parser->lex_mode = LONEJSON_LEX_FALSE;
        parser->lex_is_key = 0;
        if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return lonejson__scratch_append(&parser->token, 'f')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append literal");
      }
      if (ch == 'n') {
        parser->lex_mode = LONEJSON_LEX_NULL;
        parser->lex_is_key = 0;
        if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return lonejson__scratch_append(&parser->token, 'n')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append literal");
      }
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column, "expected JSON value");
    case LONEJSON_FRAME_OBJECT_COMMA_OR_END:
      if (ch == ',') {
        frame->state = LONEJSON_FRAME_OBJECT_KEY_OR_END;
        frame->after_comma = 1;
        return LONEJSON_STATUS_OK;
      }
      if (ch == '}') {
        return lonejson__finalize_object(parser);
      }
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected ',' or '}' in object");
    default:
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column, "invalid object state");
    }
  }

  switch (frame->state) {
  case LONEJSON_FRAME_ARRAY_VALUE_OR_END:
    if (ch == ']') {
      if (frame->after_comma) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "trailing comma in array");
      }
      return lonejson__finalize_array(parser);
    }
    frame->after_comma = 0;
    if (ch == '"') {
      parser->lex_mode = LONEJSON_LEX_STRING;
      parser->lex_is_key = 0;
      parser->lex_escape = 0;
      parser->unicode_pending_high = 0u;
      parser->unicode_digits_needed = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return LONEJSON_STATUS_OK;
    }
    if (ch == '{') {
      return lonejson__begin_object_value(parser);
    }
    if (ch == '[') {
      return lonejson__begin_array_value(parser);
    }
    if (ch == '-' || lonejson__is_digit(ch)) {
      parser->lex_mode = LONEJSON_LEX_NUMBER;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append number");
    }
    if (ch == 't') {
      parser->lex_mode = LONEJSON_LEX_TRUE;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return lonejson__scratch_append(&parser->token, 't')
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    if (ch == 'f') {
      parser->lex_mode = LONEJSON_LEX_FALSE;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return lonejson__scratch_append(&parser->token, 'f')
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    if (ch == 'n') {
      parser->lex_mode = LONEJSON_LEX_NULL;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return lonejson__scratch_append(&parser->token, 'n')
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "expected array value");
  case LONEJSON_FRAME_ARRAY_COMMA_OR_END:
    if (ch == ',') {
      frame->state = LONEJSON_FRAME_ARRAY_VALUE_OR_END;
      frame->after_comma = 1;
      return LONEJSON_STATUS_OK;
    }
    if (ch == ']') {
      return lonejson__finalize_array(parser);
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "expected ',' or ']' in array");
  default:
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid array state");
  }
}

static int lonejson__parser_root_complete(const lonejson_parser *parser) {
  return parser->root_finished && parser->frame_count == 0u &&
         parser->lex_mode == LONEJSON_LEX_NONE;
}

static lonejson_status lonejson__parser_feed_bytes(lonejson_parser *parser,
                                                   const unsigned char *bytes,
                                                   size_t len, size_t *consumed,
                                                   int stop_at_root) {
  size_t i;

  if (parser == NULL || (bytes == NULL && len != 0u)) {
    return lonejson__set_error(parser ? &parser->error : NULL,
                               LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                               "invalid parser feed arguments");
  }
  if (parser->failed) {
    return parser->error.code;
  }

  i = 0u;
  while (i < len) {
    lonejson_status status;

    if (stop_at_root && lonejson__parser_root_complete(parser)) {
      break;
    }

    if (parser->lex_mode == LONEJSON_LEX_STRING &&
        parser->unicode_digits_needed == 0 && !parser->lex_escape) {
      size_t used = 0u;

      status = lonejson__consume_string_fast(parser, bytes + i, len - i, &used);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        parser->failed = 1;
        if (consumed != NULL) {
          *consumed = i + used;
        }
        return status;
      }
      i += used;
      if (parser->lex_mode == LONEJSON_LEX_NONE) {
        lonejson__parser_note_workspace_usage(parser);
        continue;
      }
      if (used != 0u) {
        continue;
      }
    } else if (parser->lex_mode == LONEJSON_LEX_NUMBER) {
      size_t start;

      start = i;
      while (i < len && (lonejson__is_digit(bytes[i]) || bytes[i] == '+' ||
                         bytes[i] == '-' || bytes[i] == '.' ||
                         bytes[i] == 'e' || bytes[i] == 'E')) {
        i++;
      }
      if (i != start) {
        if (!lonejson__scratch_append_bytes(&parser->token, bytes + start,
                                            i - start)) {
          parser->failed = 1;
          status = lonejson__set_error(
              &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
              parser->error.offset, parser->error.line, parser->error.column,
              "failed to append number");
          if (consumed != NULL) {
            *consumed = start;
          }
          return status;
        }
        parser->error.offset += i - start;
        parser->error.column += i - start;
        lonejson__parser_note_workspace_usage(parser);
        if (i == len) {
          break;
        }
        parser->lex_mode = LONEJSON_LEX_NONE;
        status = lonejson__deliver_token(parser, LONEJSON_LEX_NUMBER);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          parser->failed = 1;
          if (consumed != NULL) {
            *consumed = i;
          }
          return status;
        }
        lonejson__parser_note_workspace_usage(parser);
        continue;
      }
    }

    if (parser->lex_mode == LONEJSON_LEX_NONE && parser->frame_count != 0u) {
      lonejson_frame *active = &parser->frames[parser->frame_count - 1u];

      parser->error.offset++;
      parser->error.column++;
      if (active->kind == LONEJSON_CONTAINER_OBJECT) {
        if (active->state == LONEJSON_FRAME_OBJECT_KEY_OR_END &&
            bytes[i] == '"') {
          status = lonejson__begin_string_lex(parser, 1);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          i++;
          continue;
        }
        if (active->state == LONEJSON_FRAME_OBJECT_COLON && bytes[i] == ':') {
          active->state = LONEJSON_FRAME_OBJECT_VALUE;
          i++;
          continue;
        }
        if (active->state == LONEJSON_FRAME_OBJECT_VALUE) {
          if (bytes[i] == '"') {
            status = lonejson__begin_string_value_lex(
                parser, active->pending_field,
                active->pending_field != NULL
                    ? lonejson__field_ptr(active->object_ptr,
                                          active->pending_field)
                    : NULL);
            if (status != LONEJSON_STATUS_OK &&
                status != LONEJSON_STATUS_TRUNCATED) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
            i++;
            continue;
          }
          if (bytes[i] == '-' || lonejson__is_digit(bytes[i])) {
            status = lonejson__begin_number_lex(parser, bytes[i]);
            if (status != LONEJSON_STATUS_OK &&
                status != LONEJSON_STATUS_TRUNCATED) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
            i++;
            continue;
          }
          if (bytes[i] == 't' || bytes[i] == 'f' || bytes[i] == 'n') {
            status = lonejson__begin_literal_lex(
                parser,
                (bytes[i] == 't')   ? LONEJSON_LEX_TRUE
                : (bytes[i] == 'f') ? LONEJSON_LEX_FALSE
                                    : LONEJSON_LEX_NULL,
                bytes[i]);
            if (status != LONEJSON_STATUS_OK &&
                status != LONEJSON_STATUS_TRUNCATED) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
            i++;
            continue;
          }
        }
        if (active->state == LONEJSON_FRAME_OBJECT_COMMA_OR_END) {
          if (bytes[i] == ',') {
            active->state = LONEJSON_FRAME_OBJECT_KEY_OR_END;
            active->after_comma = 1;
            i++;
            continue;
          }
          if (bytes[i] == '}') {
            status = lonejson__finalize_object(parser);
            if (status != LONEJSON_STATUS_OK &&
                status != LONEJSON_STATUS_TRUNCATED) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
            lonejson__parser_note_workspace_usage(parser);
            i++;
            continue;
          }
        }
      } else if (active->kind == LONEJSON_CONTAINER_ARRAY &&
                 active->state == LONEJSON_FRAME_ARRAY_VALUE_OR_END) {
        if (bytes[i] == '"') {
          status = lonejson__begin_string_value_lex(
              parser, active->field,
              active->field != NULL
                  ? lonejson__field_ptr(active->object_ptr, active->field)
                  : NULL);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          i++;
          continue;
        }
        if (bytes[i] == '-' || lonejson__is_digit(bytes[i])) {
          status = lonejson__begin_number_lex(parser, bytes[i]);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          i++;
          continue;
        }
        if (bytes[i] == 't' || bytes[i] == 'f' || bytes[i] == 'n') {
          status = lonejson__begin_literal_lex(
              parser,
              (bytes[i] == 't')   ? LONEJSON_LEX_TRUE
              : (bytes[i] == 'f') ? LONEJSON_LEX_FALSE
                                  : LONEJSON_LEX_NULL,
              bytes[i]);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          i++;
          continue;
        }
      } else if (active->kind == LONEJSON_CONTAINER_ARRAY &&
                 active->state == LONEJSON_FRAME_ARRAY_COMMA_OR_END) {
        if (bytes[i] == ',') {
          active->state = LONEJSON_FRAME_ARRAY_VALUE_OR_END;
          active->after_comma = 1;
          i++;
          continue;
        }
        if (bytes[i] == ']') {
          status = lonejson__finalize_array(parser);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          lonejson__parser_note_workspace_usage(parser);
          i++;
          continue;
        }
      }
      parser->error.offset--;
      parser->error.column--;
    }

    status = lonejson__parser_consume_char(parser, bytes[i]);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      parser->failed = 1;
      if (consumed != NULL) {
        *consumed = i + 1u;
      }
      return status;
    }
    lonejson__parser_note_workspace_usage(parser);
    i++;
  }

  if (consumed != NULL) {
    *consumed = i;
  }
  return parser->error.truncated ? LONEJSON_STATUS_TRUNCATED
                                 : LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__sink_buffer(void *user, const void *data,
                                             size_t len,
                                             lonejson_error *error) {
  lonejson_buffer_sink *sink = (lonejson_buffer_sink *)user;
  size_t available =
      (sink->capacity > sink->length) ? (sink->capacity - sink->length) : 0u;
  size_t writable = (available > 0u) ? (available - 1u) : 0u;

  sink->needed += len;
  if (available > 0u) {
    size_t copy_len = (len < writable) ? len : writable;
    if (copy_len > 0u) {
      memcpy(sink->buffer + sink->length, data, copy_len);
      sink->length += copy_len;
    }
  }
  if (len > writable) {
    sink->truncated = 1;
    if (sink->policy == LONEJSON_OVERFLOW_FAIL) {
      return lonejson__set_error(
          error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
          error ? error->line : 0u, error ? error->column : 0u,
          "output buffer too small");
    }
    return (sink->policy == LONEJSON_OVERFLOW_TRUNCATE)
               ? LONEJSON_STATUS_TRUNCATED
               : LONEJSON_STATUS_OK;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__sink_buffer_exact(void *user, const void *data,
                                                   size_t len,
                                                   lonejson_error *error) {
  lonejson_buffer_sink *sink = (lonejson_buffer_sink *)user;
  size_t writable = (sink->capacity > sink->length + 1u)
                        ? (sink->capacity - sink->length - 1u)
                        : 0u;

  sink->needed += len;
  if (len > writable) {
    sink->truncated = 1;
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "output buffer too small");
  }
  if (len != 0u) {
    memcpy(sink->buffer + sink->length, data, len);
    sink->length += len;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__sink_count(void *user, const void *data,
                                            size_t len, lonejson_error *error) {
  lonejson_buffer_sink *sink = (lonejson_buffer_sink *)user;
  (void)data;
  (void)error;
  sink->needed += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__sink_file(void *user, const void *data,
                                           size_t len, lonejson_error *error) {
  FILE *fp = (FILE *)user;
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (fwrite(data, 1u, len, fp) != len) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(
        error, LONEJSON_STATUS_IO_ERROR, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "failed to write JSON output");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__emit(lonejson_sink_fn sink, void *user,
                                      lonejson_error *error, const char *data,
                                      size_t len) {
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  return sink(user, data, len, error);
}

static lonejson_status lonejson__emit_cstr(lonejson_sink_fn sink, void *user,
                                           lonejson_error *error,
                                           const char *text) {
  return lonejson__emit(sink, user, error, text, strlen(text));
}

static lonejson_status
lonejson__emit_escaped_fragment(lonejson_sink_fn sink, void *user,
                                lonejson_error *error,
                                const unsigned char *text, size_t len) {
  static const char hex[] = "0123456789abcdef";
  size_t i;
  size_t start;
  lonejson_status status;

  start = 0u;
  for (i = 0u; i < len; ++i) {
    char escape_buf[7];
    if (text[i] >= 0x20u && text[i] != '"' && text[i] != '\\') {
      continue;
    }
    if (start != i) {
      status = lonejson__emit(sink, user, error, (const char *)(text + start),
                              i - start);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    switch (text[i]) {
    case '"':
      status = lonejson__emit_cstr(sink, user, error, "\\\"");
      break;
    case '\\':
      status = lonejson__emit_cstr(sink, user, error, "\\\\");
      break;
    case '\b':
      status = lonejson__emit_cstr(sink, user, error, "\\b");
      break;
    case '\f':
      status = lonejson__emit_cstr(sink, user, error, "\\f");
      break;
    case '\n':
      status = lonejson__emit_cstr(sink, user, error, "\\n");
      break;
    case '\r':
      status = lonejson__emit_cstr(sink, user, error, "\\r");
      break;
    case '\t':
      status = lonejson__emit_cstr(sink, user, error, "\\t");
      break;
    default:
      if (text[i] < 0x20u) {
        escape_buf[0] = '\\';
        escape_buf[1] = 'u';
        escape_buf[2] = '0';
        escape_buf[3] = '0';
        escape_buf[4] = hex[(text[i] >> 4u) & 0x0Fu];
        escape_buf[5] = hex[text[i] & 0x0Fu];
        escape_buf[6] = '\0';
        status = lonejson__emit(sink, user, error, escape_buf, 6u);
      } else {
        status = LONEJSON_STATUS_OK;
      }
      break;
    }
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    start = i + 1u;
  }
  if (start != len) {
    status = lonejson__emit(sink, user, error, (const char *)(text + start),
                            len - start);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__emit_escaped_bytes(lonejson_sink_fn sink,
                                                    void *user,
                                                    lonejson_error *error,
                                                    const unsigned char *text,
                                                    size_t len) {
  lonejson_status status;

  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = lonejson__emit_escaped_fragment(sink, user, error, text, len);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status lonejson__emit_escaped_string(lonejson_sink_fn sink,
                                                     void *user,
                                                     lonejson_error *error,
                                                     const char *text) {
  return lonejson__emit_escaped_bytes(
      sink, user, error, (const unsigned char *)text, strlen(text));
}

typedef struct lonejson__write_state {
  lonejson_sink_fn sink;
  void *user;
  lonejson_error *error;
  int pretty;
  size_t depth;
} lonejson__write_state;

static lonejson_status
lonejson__emit_pretty_indent(const lonejson__write_state *state, size_t depth) {
  size_t i;
  lonejson_status status;

  if (!state->pretty) {
    return LONEJSON_STATUS_OK;
  }
  status = lonejson__emit_cstr(state->sink, state->user, state->error, "\n");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (i = 0; i < depth; ++i) {
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "  ");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__serialize_value_pretty(const lonejson_field *field, const void *ptr,
                                 lonejson__write_state *state);

static lonejson_status
lonejson__serialize_value_compact(const lonejson_field *field, const void *ptr,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error);

static lonejson_status lonejson__serialize_map_compact(const lonejson_map *map,
                                                       const void *src,
                                                       lonejson_sink_fn sink,
                                                       void *user,
                                                       lonejson_error *error);

static lonejson_status
lonejson__serialize_map_pretty(const lonejson_map *map, const void *src,
                               lonejson__write_state *state) {
  size_t i;
  lonejson_status status;

  status = lonejson__emit_cstr(state->sink, state->user, state->error, "{");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  if (map->field_count == 0u) {
    return lonejson__emit_cstr(state->sink, state->user, state->error, "}");
  }
  for (i = 0; i < map->field_count; ++i) {
    if (i != 0u) {
      status = lonejson__emit_cstr(state->sink, state->user, state->error, ",");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth + 1u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    status = lonejson__emit_escaped_string(
        state->sink, state->user, state->error, map->fields[i].json_key);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = lonejson__emit_cstr(state->sink, state->user, state->error,
                                 state->pretty ? ": " : ":");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    ++state->depth;
    status = lonejson__serialize_value_pretty(
        &map->fields[i], lonejson__field_cptr(src, &map->fields[i]), state);
    --state->depth;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  if (state->pretty) {
    status = lonejson__emit_pretty_indent(state, state->depth);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return lonejson__emit_cstr(state->sink, state->user, state->error, "}");
}

static lonejson_status
lonejson__serialize_jsonl_records(const lonejson_map *map, const void *items,
                                  size_t count, size_t stride,
                                  lonejson__write_state *state) {
  const unsigned char *base = (const unsigned char *)items;
  size_t i;
  lonejson_status status;

  for (i = 0; i < count; ++i) {
    const void *record = (const void *)(base + (i * stride));
    status = lonejson__serialize_map_pretty(map, record, state);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "\n");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__serialize_jsonl_records_compact(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    lonejson_sink_fn sink, void *user, lonejson_error *error) {
  const unsigned char *base = (const unsigned char *)items;
  size_t i;
  lonejson_status status;

  for (i = 0; i < count; ++i) {
    const void *record = (const void *)(base + (i * stride));
    status = lonejson__serialize_map_compact(map, record, sink, user, error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = lonejson__emit_cstr(sink, user, error, "\n");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__emit_number_text(lonejson_sink_fn sink,
                                                  void *user,
                                                  lonejson_error *error,
                                                  const char *fmt, ...) {
  char buf[64];
  va_list ap;
  int written;

  va_start(ap, fmt);
  written = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (written < 0) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INTERNAL_ERROR, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "failed to format number");
  }
  return lonejson__emit(sink, user, error, buf, (size_t)written);
}

static lonejson_status lonejson__emit_u64_value(lonejson_sink_fn sink,
                                                void *user,
                                                lonejson_error *error,
                                                lonejson_uint64 value) {
  char buf[32];
  size_t idx;

  idx = sizeof(buf);
  do {
    lonejson_uint64 digit;
    digit = value % 10u;
    value /= 10u;
    --idx;
    buf[idx] = (char)('0' + (int)digit);
  } while (value != 0u);
  return lonejson__emit(sink, user, error, buf + idx, sizeof(buf) - idx);
}

static lonejson_status lonejson__emit_i64_value(lonejson_sink_fn sink,
                                                void *user,
                                                lonejson_error *error,
                                                lonejson_int64 value) {
  lonejson_status status;
  lonejson_uint64 magnitude;

  if (value < 0) {
    status = lonejson__emit_cstr(sink, user, error, "-");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    magnitude = (lonejson_uint64)(-(value + 1)) + 1u;
  } else {
    magnitude = (lonejson_uint64)value;
  }
  return lonejson__emit_u64_value(sink, user, error, magnitude);
}

static lonejson_status lonejson__emit_base64_bytes(lonejson_sink_fn sink,
                                                   void *user,
                                                   lonejson_error *error,
                                                   const unsigned char *data,
                                                   size_t len) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char out[4];

  out[0] = alphabet[(data[0] >> 2u) & 0x3Fu];
  out[1] =
      alphabet[((data[0] & 0x03u) << 4u) | (((len > 1u) ? data[1] : 0u) >> 4u)];
  out[2] = (len > 1u) ? alphabet[((data[1] & 0x0Fu) << 2u) |
                                 (((len > 2u) ? data[2] : 0u) >> 6u)]
                      : '=';
  out[3] = (len > 2u) ? alphabet[data[2] & 0x3Fu] : '=';
  return lonejson__emit(sink, user, error, out, sizeof(out));
}

static lonejson_status
lonejson__serialize_spooled_text(const lonejson_spooled *value,
                                 lonejson_sink_fn sink, void *user,
                                 lonejson_error *error) {
  unsigned char buffer[4096];
  lonejson_spooled cursor;
  lonejson_status status;

  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
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
                                 "failed to read spooled text");
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson__emit_escaped_fragment(sink, user, error, buffer,
                                               chunk.bytes_read);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (chunk.eof) {
      break;
    }
  }
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status
lonejson__serialize_spooled_base64(const lonejson_spooled *value,
                                   lonejson_sink_fn sink, void *user,
                                   lonejson_error *error) {
  unsigned char buffer[4096];
  unsigned char carry[3];
  size_t carry_len;
  lonejson_spooled cursor;
  lonejson_status status;

  carry_len = 0u;
  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  cursor = *value;
  status = lonejson_spooled_rewind(&cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (;;) {
    lonejson_read_result chunk =
        lonejson_spooled_read(&cursor, buffer, sizeof(buffer));
    size_t offset;

    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read spooled bytes");
    }
    offset = 0u;
    if (carry_len != 0u) {
      while (carry_len < 3u && offset < chunk.bytes_read) {
        carry[carry_len++] = buffer[offset++];
      }
      if (carry_len == 3u) {
        status = lonejson__emit_base64_bytes(sink, user, error, carry, 3u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        carry_len = 0u;
      }
    }
    while (offset + 3u <= chunk.bytes_read) {
      status =
          lonejson__emit_base64_bytes(sink, user, error, buffer + offset, 3u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      offset += 3u;
    }
    while (offset < chunk.bytes_read) {
      carry[carry_len++] = buffer[offset++];
    }
    if (chunk.eof) {
      break;
    }
  }
  if (carry_len != 0u) {
    status = lonejson__emit_base64_bytes(sink, user, error, carry, carry_len);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status
lonejson__serialize_source_text(const lonejson_source *value,
                                lonejson_sink_fn sink, void *user,
                                lonejson_error *error) {
  unsigned char buffer[4096];
  lonejson__source_cursor cursor;
  lonejson_read_result chunk;
  lonejson_status status;

  if (value->kind == LONEJSON_SOURCE_NONE) {
    return lonejson__emit_cstr(sink, user, error, "null");
  }
  status = lonejson__source_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__source_cursor_close(&cursor);
    return status;
  }
  for (;;) {
    chunk = lonejson__source_cursor_read(&cursor, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      lonejson__source_cursor_close(&cursor);
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read source text");
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson__emit_escaped_fragment(sink, user, error, buffer,
                                               chunk.bytes_read);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        lonejson__source_cursor_close(&cursor);
        return status;
      }
    }
    if (chunk.eof) {
      break;
    }
  }
  lonejson__source_cursor_close(&cursor);
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status
lonejson__serialize_source_base64(const lonejson_source *value,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error) {
  unsigned char buffer[4096];
  unsigned char carry[3];
  size_t carry_len;
  lonejson__source_cursor cursor;
  lonejson_read_result chunk;
  lonejson_status status;

  if (value->kind == LONEJSON_SOURCE_NONE) {
    return lonejson__emit_cstr(sink, user, error, "null");
  }
  carry_len = 0u;
  status = lonejson__source_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__source_cursor_close(&cursor);
    return status;
  }
  for (;;) {
    size_t offset;

    chunk = lonejson__source_cursor_read(&cursor, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      lonejson__source_cursor_close(&cursor);
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read source bytes");
    }
    offset = 0u;
    if (carry_len != 0u) {
      while (carry_len < 3u && offset < chunk.bytes_read) {
        carry[carry_len++] = buffer[offset++];
      }
      if (carry_len == 3u) {
        status = lonejson__emit_base64_bytes(sink, user, error, carry, 3u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          lonejson__source_cursor_close(&cursor);
          return status;
        }
        carry_len = 0u;
      }
    }
    while (offset + 3u <= chunk.bytes_read) {
      status =
          lonejson__emit_base64_bytes(sink, user, error, buffer + offset, 3u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        lonejson__source_cursor_close(&cursor);
        return status;
      }
      offset += 3u;
    }
    while (offset < chunk.bytes_read) {
      carry[carry_len++] = buffer[offset++];
    }
    if (chunk.eof) {
      break;
    }
  }
  lonejson__source_cursor_close(&cursor);
  if (carry_len != 0u) {
    status = lonejson__emit_base64_bytes(sink, user, error, carry, carry_len);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status
lonejson__serialize_value_pretty(const lonejson_field *field, const void *ptr,
                                 lonejson__write_state *state) {
  size_t i;
  lonejson_status status;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      const char *text = *(const char *const *)ptr;
      if (text == NULL) {
        return lonejson__emit_cstr(state->sink, state->user, state->error,
                                   "null");
      }
      return lonejson__emit_escaped_string(state->sink, state->user,
                                           state->error, text);
    }
    return lonejson__emit_escaped_string(state->sink, state->user, state->error,
                                         (const char *)ptr);
  case LONEJSON_FIELD_KIND_STRING_STREAM:
    return lonejson__serialize_spooled_text(
        (const lonejson_spooled *)ptr, state->sink, state->user, state->error);
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    return lonejson__serialize_spooled_base64(
        (const lonejson_spooled *)ptr, state->sink, state->user, state->error);
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
    return lonejson__serialize_source_text(
        (const lonejson_source *)ptr, state->sink, state->user, state->error);
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    return lonejson__serialize_source_base64(
        (const lonejson_source *)ptr, state->sink, state->user, state->error);
  case LONEJSON_FIELD_KIND_I64:
    return lonejson__emit_i64_value(state->sink, state->user, state->error,
                                    *(const lonejson_int64 *)ptr);
  case LONEJSON_FIELD_KIND_U64:
    return lonejson__emit_u64_value(state->sink, state->user, state->error,
                                    *(const lonejson_uint64 *)ptr);
  case LONEJSON_FIELD_KIND_F64:
    if (!lonejson__is_finite_f64(*(const double *)ptr)) {
      return lonejson__set_error(state->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 state->error ? state->error->offset : 0u,
                                 state->error ? state->error->line : 0u,
                                 state->error ? state->error->column : 0u,
                                 "non-finite double cannot be serialized");
    }
    return lonejson__emit_number_text(state->sink, state->user, state->error,
                                      "%.17g", *(const double *)ptr);
  case LONEJSON_FIELD_KIND_BOOL:
    return lonejson__emit_cstr(state->sink, state->user, state->error,
                               *(const bool *)ptr ? "true" : "false");
  case LONEJSON_FIELD_KIND_OBJECT:
    return lonejson__serialize_map_pretty(field->submap, ptr, state);
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    const lonejson_string_array *arr = (const lonejson_string_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status =
          lonejson__emit_escaped_string(state->sink, state->user, state->error,
                                        arr->items[i] ? arr->items[i] : "");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    const lonejson_i64_array *arr = (const lonejson_i64_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_i64_value(state->sink, state->user, state->error,
                                        arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    const lonejson_u64_array *arr = (const lonejson_u64_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_u64_value(state->sink, state->user, state->error,
                                        arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    const lonejson_f64_array *arr = (const lonejson_f64_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (!lonejson__is_finite_f64(arr->items[i])) {
        return lonejson__set_error(
            state->error, LONEJSON_STATUS_TYPE_MISMATCH,
            state->error ? state->error->offset : 0u,
            state->error ? state->error->line : 0u,
            state->error ? state->error->column : 0u,
            "non-finite double array element cannot be serialized");
      }
      status = lonejson__emit_number_text(state->sink, state->user,
                                          state->error, "%.17g", arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    const lonejson_bool_array *arr = (const lonejson_bool_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_cstr(state->sink, state->user, state->error,
                                   arr->items[i] ? "true" : "false");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    const lonejson_object_array *arr = (const lonejson_object_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      const void *elem =
          (const unsigned char *)arr->items + (i * field->elem_size);
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      ++state->depth;
      status = lonejson__serialize_map_pretty(field->submap, elem, state);
      --state->depth;
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  default:
    return lonejson__set_error(state->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               state->error ? state->error->offset : 0u,
                               state->error ? state->error->line : 0u,
                               state->error ? state->error->column : 0u,
                               "unsupported field kind");
  }
}

static lonejson_status lonejson__serialize_map_compact(const lonejson_map *map,
                                                       const void *src,
                                                       lonejson_sink_fn sink,
                                                       void *user,
                                                       lonejson_error *error) {
  size_t i;
  lonejson_status status;

  status = lonejson__emit_cstr(sink, user, error, "{");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (i = 0; i < map->field_count; ++i) {
    if (i != 0u) {
      status = lonejson__emit_cstr(sink, user, error, ",");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    status = lonejson__emit_escaped_string(sink, user, error,
                                           map->fields[i].json_key);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = lonejson__emit_cstr(sink, user, error, ":");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = lonejson__serialize_value_compact(
        &map->fields[i], lonejson__field_cptr(src, &map->fields[i]), sink, user,
        error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return lonejson__emit_cstr(sink, user, error, "}");
}

static lonejson_status
lonejson__serialize_value_compact(const lonejson_field *field, const void *ptr,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error) {
  size_t i;
  lonejson_status status;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      const char *text = *(const char *const *)ptr;
      if (text == NULL) {
        return lonejson__emit_cstr(sink, user, error, "null");
      }
      return lonejson__emit_escaped_string(sink, user, error, text);
    }
    return lonejson__emit_escaped_string(sink, user, error, (const char *)ptr);
  case LONEJSON_FIELD_KIND_STRING_STREAM:
    return lonejson__serialize_spooled_text((const lonejson_spooled *)ptr, sink,
                                            user, error);
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    return lonejson__serialize_spooled_base64((const lonejson_spooled *)ptr,
                                              sink, user, error);
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
    return lonejson__serialize_source_text((const lonejson_source *)ptr, sink,
                                           user, error);
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    return lonejson__serialize_source_base64((const lonejson_source *)ptr, sink,
                                             user, error);
  case LONEJSON_FIELD_KIND_I64:
    return lonejson__emit_i64_value(sink, user, error,
                                    *(const lonejson_int64 *)ptr);
  case LONEJSON_FIELD_KIND_U64:
    return lonejson__emit_u64_value(sink, user, error,
                                    *(const lonejson_uint64 *)ptr);
  case LONEJSON_FIELD_KIND_F64:
    if (!lonejson__is_finite_f64(*(const double *)ptr)) {
      return lonejson__set_error(
          error, LONEJSON_STATUS_TYPE_MISMATCH, error ? error->offset : 0u,
          error ? error->line : 0u, error ? error->column : 0u,
          "non-finite double cannot be serialized");
    }
    return lonejson__emit_number_text(sink, user, error, "%.17g",
                                      *(const double *)ptr);
  case LONEJSON_FIELD_KIND_BOOL:
    return lonejson__emit_cstr(sink, user, error,
                               *(const bool *)ptr ? "true" : "false");
  case LONEJSON_FIELD_KIND_OBJECT:
    return lonejson__serialize_map_compact(field->submap, ptr, sink, user,
                                           error);
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    const lonejson_string_array *arr = (const lonejson_string_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_escaped_string(
          sink, user, error, arr->items[i] ? arr->items[i] : "");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    const lonejson_i64_array *arr = (const lonejson_i64_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_i64_value(sink, user, error, arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    const lonejson_u64_array *arr = (const lonejson_u64_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_u64_value(sink, user, error, arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    const lonejson_f64_array *arr = (const lonejson_f64_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (!lonejson__is_finite_f64(arr->items[i])) {
        return lonejson__set_error(
            error, LONEJSON_STATUS_TYPE_MISMATCH, error ? error->offset : 0u,
            error ? error->line : 0u, error ? error->column : 0u,
            "non-finite double array element cannot be serialized");
      }
      status =
          lonejson__emit_number_text(sink, user, error, "%.17g", arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    const lonejson_bool_array *arr = (const lonejson_bool_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_cstr(sink, user, error,
                                   arr->items[i] ? "true" : "false");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    const lonejson_object_array *arr = (const lonejson_object_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      const void *elem =
          (const unsigned char *)arr->items + (i * field->elem_size);
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__serialize_map_compact(field->submap, elem, sink, user,
                                               error);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  default:
    return lonejson__set_error(
        error, LONEJSON_STATUS_INTERNAL_ERROR, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "unsupported field kind");
  }
}

lonejson_parse_options lonejson_default_parse_options(void) {
  lonejson_parse_options options;
  options.clear_destination = 1;
  options.reject_duplicate_keys = 1;
  options.max_depth = 64u;
  return options;
}

lonejson_write_options lonejson_default_write_options(void) {
  lonejson_write_options options;
  options.overflow_policy = LONEJSON_OVERFLOW_FAIL;
  options.pretty = 0;
  return options;
}

const char *lonejson_status_string(lonejson_status status) {
  switch (status) {
  case LONEJSON_STATUS_OK:
    return "ok";
  case LONEJSON_STATUS_INVALID_ARGUMENT:
    return "invalid_argument";
  case LONEJSON_STATUS_INVALID_JSON:
    return "invalid_json";
  case LONEJSON_STATUS_TYPE_MISMATCH:
    return "type_mismatch";
  case LONEJSON_STATUS_MISSING_REQUIRED_FIELD:
    return "missing_required_field";
  case LONEJSON_STATUS_DUPLICATE_FIELD:
    return "duplicate_field";
  case LONEJSON_STATUS_OVERFLOW:
    return "overflow";
  case LONEJSON_STATUS_TRUNCATED:
    return "truncated";
  case LONEJSON_STATUS_ALLOCATION_FAILED:
    return "allocation_failed";
  case LONEJSON_STATUS_CALLBACK_FAILED:
    return "callback_failed";
  case LONEJSON_STATUS_IO_ERROR:
    return "io_error";
  case LONEJSON_STATUS_INTERNAL_ERROR:
    return "internal_error";
  default:
    return "unknown";
  }
}

#ifdef LONEJSON_WITH_CURL
static lonejson_parser *
lonejson__parser_create_ex(const lonejson_map *map, void *dst,
                           const lonejson_parse_options *options,
                           lonejson_error *error, int validate_only) {
  lonejson_parser *parser;
  unsigned char *workspace;

  if ((!validate_only && (map == NULL || dst == NULL)) ||
      (validate_only && (map != NULL || dst != NULL))) {
    lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 1u, 0u,
        validate_only ? "validation parser does not accept mapping arguments"
                      : "map and destination are required");
    return NULL;
  }
  parser = (lonejson_parser *)LONEJSON_MALLOC(sizeof(*parser) +
                                              LONEJSON_PUSH_PARSER_BUFFER_SIZE);
  if (parser == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 1u, 0u,
                        "failed to allocate parser");
    return NULL;
  }
  workspace = ((unsigned char *)parser) + sizeof(*parser);
  lonejson__parser_init_state(parser, map, dst, options, validate_only,
                              workspace, LONEJSON_PUSH_PARSER_BUFFER_SIZE);
  parser->owns_self = 1;
  if (!validate_only && parser->options.clear_destination) {
    lonejson__init_map(map, dst);
  }
  lonejson__clear_error(error);
  return parser;
}
#endif

static lonejson_status lonejson_parser_feed(lonejson_parser *parser,
                                            const void *data, size_t len) {
  size_t consumed;

  return lonejson__parser_feed_bytes(parser, (const unsigned char *)data, len,
                                     &consumed, 0);
}

static lonejson_status lonejson_parser_finish(lonejson_parser *parser) {
  if (parser == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (parser->failed) {
    return parser->error.code;
  }
  if (parser->lex_mode == LONEJSON_LEX_NUMBER) {
    parser->lex_mode = LONEJSON_LEX_NONE;
    if (lonejson__deliver_token(parser, LONEJSON_LEX_NUMBER) !=
        LONEJSON_STATUS_OK) {
      return parser->error.code;
    }
  }
  if (parser->lex_mode != LONEJSON_LEX_NONE) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "incomplete JSON token at end of input");
  }
  if (!parser->root_started || !parser->root_finished ||
      parser->frame_count != 0u) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "incomplete JSON document");
  }
  return parser->error.truncated ? LONEJSON_STATUS_TRUNCATED
                                 : LONEJSON_STATUS_OK;
}

static void lonejson_parser_destroy(lonejson_parser *parser) {
  if (parser == NULL) {
    return;
  }
  while (parser->frame_count != 0u) {
    lonejson__pop_frame(parser);
  }
  if (parser->owns_self) {
    LONEJSON_FREE(parser);
  }
}

lonejson_status lonejson_parse_buffer(const lonejson_map *map, void *dst,
                                      const void *data, size_t len,
                                      const lonejson_parse_options *options,
                                      lonejson_error *error) {
  lonejson_parser *parser;
  lonejson_status status;
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE];

  if (map == NULL || dst == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map and destination are required");
  }
  if (data == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "buffer is required");
  }
  parser = &parser_storage;
  lonejson__parser_init_state(parser, map, dst, options, 0, parser_workspace,
                              sizeof(parser_workspace));
  if (parser->options.clear_destination) {
    lonejson__init_map(map, dst);
  }
  lonejson__clear_error(error);
  status = lonejson_parser_feed(parser, data, len);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    status = lonejson_parser_finish(parser);
  }
  if (error != NULL) {
    *error = parser->error;
  }
  return status;
}

lonejson_status lonejson_parse_cstr(const lonejson_map *map, void *dst,
                                    const char *json,
                                    const lonejson_parse_options *options,
                                    lonejson_error *error) {
  if (json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "json string is required");
  }
  return lonejson_parse_buffer(map, dst, json, strlen(json), options, error);
}

static lonejson_read_result
lonejson__file_reader(void *user, unsigned char *buffer, size_t capacity) {
  FILE *fp = (FILE *)user;
  lonejson_read_result result;

  memset(&result, 0, sizeof(result));
  result.bytes_read = fread(buffer, 1u, capacity, fp);
  result.eof = feof(fp) ? 1 : 0;
  result.error_code = ferror(fp) ? errno : 0;
  return result;
}

lonejson_status lonejson_parse_reader(const lonejson_map *map, void *dst,
                                      lonejson_reader_fn reader, void *user,
                                      const lonejson_parse_options *options,
                                      lonejson_error *error) {
  lonejson_parser *parser;
  unsigned char buffer[LONEJSON_READER_BUFFER_SIZE];
  lonejson_status status = LONEJSON_STATUS_OK;
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE];

  if (reader == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "reader callback is required");
  }
  if (map == NULL || dst == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map and destination are required");
  }
  parser = &parser_storage;
  lonejson__parser_init_state(parser, map, dst, options, 0, parser_workspace,
                              sizeof(parser_workspace));
  if (parser->options.clear_destination) {
    lonejson__init_map(map, dst);
  }
  lonejson__clear_error(error);
  for (;;) {
    lonejson_read_result chunk = reader(user, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      parser->error.system_errno = chunk.error_code;
      status = lonejson__set_error(
          &parser->error, LONEJSON_STATUS_IO_ERROR, parser->error.offset,
          parser->error.line, parser->error.column, "reader callback failed");
      break;
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson_parser_feed(parser, buffer, chunk.bytes_read);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        break;
      }
    }
    if (chunk.eof) {
      status = lonejson_parser_finish(parser);
      break;
    }
  }
  if (error != NULL) {
    *error = parser->error;
  }
  return status;
}

lonejson_status lonejson_parse_filep(const lonejson_map *map, void *dst,
                                     FILE *fp,
                                     const lonejson_parse_options *options,
                                     lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer is required");
  }
  return lonejson_parse_reader(map, dst, lonejson__file_reader, fp, options,
                               error);
}

lonejson_status lonejson_parse_path(const lonejson_map *map, void *dst,
                                    const char *path,
                                    const lonejson_parse_options *options,
                                    lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path is required");
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open '%s'", path);
  }
  status = lonejson_parse_filep(map, dst, fp, options, error);
  fclose(fp);
  return status;
}

static void lonejson__stream_prepare_parser(lonejson_stream *stream,
                                            void *dst) {
  lonejson__parser_restart_stream(stream->parser, dst);
  if (stream->parser->options.clear_destination) {
    if (stream->prepared_dst == dst) {
      lonejson__reset_map(stream->map, dst);
    } else {
      lonejson__init_map(stream->map, dst);
      stream->prepared_dst = dst;
    }
  }
  stream->current_dst = dst;
  stream->object_in_progress = 1;
}

static lonejson_read_result lonejson__stream_read(lonejson_stream *stream) {
  lonejson_read_result result;

  memset(&result, 0, sizeof(result));
  switch (stream->source_kind) {
  case LONEJSON_STREAM_SOURCE_READER:
    return stream->reader(stream->reader_user, stream->io_buffer,
                          sizeof(stream->io_buffer));
  case LONEJSON_STREAM_SOURCE_FILE:
    return lonejson__file_reader(stream->fp, stream->io_buffer,
                                 sizeof(stream->io_buffer));
  case LONEJSON_STREAM_SOURCE_FD: {
    ssize_t rc = read(stream->fd, stream->io_buffer, sizeof(stream->io_buffer));
    if (rc > 0) {
      result.bytes_read = (size_t)rc;
    } else if (rc == 0) {
      result.eof = 1;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      result.would_block = 1;
    } else {
      result.error_code = errno;
    }
    return result;
  }
  default:
    result.error_code = EINVAL;
    return result;
  }
}

static lonejson_stream *
lonejson__stream_open_common(const lonejson_map *map,
                             const lonejson_parse_options *options,
                             lonejson_error *error) {
  lonejson_stream *stream;
  size_t parser_bytes;
  unsigned char *workspace;

  if (map == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "map is required");
    return NULL;
  }
  parser_bytes = sizeof(*stream->parser) + LONEJSON_PUSH_PARSER_BUFFER_SIZE;
  stream = (lonejson_stream *)LONEJSON_MALLOC(sizeof(*stream) + parser_bytes);
  if (stream == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate stream");
    return NULL;
  }
  stream->map = map;
  stream->options = options ? *options : lonejson_default_parse_options();
  lonejson__clear_error(&stream->error);
  stream->reader = NULL;
  stream->reader_user = NULL;
  stream->fp = NULL;
  stream->fd = -1;
  stream->owns_fp = 0;
  stream->owns_fd = 0;
  stream->saw_eof = 0;
  stream->object_in_progress = 0;
  stream->buffered_start = 0u;
  stream->buffered_end = 0u;
  stream->source_kind = 0;
  stream->current_dst = NULL;
  stream->prepared_dst = NULL;
  stream->parser =
      (lonejson_parser *)(((unsigned char *)stream) + sizeof(*stream));
  workspace = ((unsigned char *)stream->parser) + sizeof(*stream->parser);
  lonejson__parser_init_state(stream->parser, map, NULL, &stream->options, 0,
                              workspace, LONEJSON_PUSH_PARSER_BUFFER_SIZE);
  stream->parser->owns_self = 0;
  lonejson__clear_error(error);
  return stream;
}

lonejson_stream *
lonejson_stream_open_reader(const lonejson_map *map, lonejson_reader_fn reader,
                            void *user, const lonejson_parse_options *options,
                            lonejson_error *error) {
  lonejson_stream *stream;

  if (reader == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "reader callback is required");
    return NULL;
  }
  stream = lonejson__stream_open_common(map, options, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_STREAM_SOURCE_READER;
  stream->reader = reader;
  stream->reader_user = user;
  return stream;
}

lonejson_stream *
lonejson_stream_open_filep(const lonejson_map *map, FILE *fp,
                           const lonejson_parse_options *options,
                           lonejson_error *error) {
  lonejson_stream *stream;

  if (fp == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "file pointer is required");
    return NULL;
  }
  stream = lonejson__stream_open_common(map, options, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_STREAM_SOURCE_FILE;
  stream->fp = fp;
  return stream;
}

lonejson_stream *
lonejson_stream_open_path(const lonejson_map *map, const char *path,
                          const lonejson_parse_options *options,
                          lonejson_error *error) {
  lonejson_stream *stream;
  FILE *fp;

  if (path == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "path is required");
    return NULL;
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 1u, 0u,
                        "failed to open '%s'", path);
    return NULL;
  }
  stream = lonejson_stream_open_filep(map, fp, options, error);
  if (stream == NULL) {
    fclose(fp);
    return NULL;
  }
  stream->owns_fp = 1;
  return stream;
}

lonejson_stream *lonejson_stream_open_fd(const lonejson_map *map, int fd,
                                         const lonejson_parse_options *options,
                                         lonejson_error *error) {
  lonejson_stream *stream;

  if (fd < 0) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "fd must be non-negative");
    return NULL;
  }
  stream = lonejson__stream_open_common(map, options, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_STREAM_SOURCE_FD;
  stream->fd = fd;
  return stream;
}

lonejson_stream_result lonejson_stream_next(lonejson_stream *stream, void *dst,
                                            lonejson_error *error) {
  lonejson_status status;

  if (stream == NULL || dst == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "stream and destination are required");
    return LONEJSON_STREAM_ERROR;
  }
  if (!stream->object_in_progress) {
    lonejson__stream_prepare_parser(stream, dst);
  } else if (stream->current_dst != dst) {
    lonejson__set_error(&stream->error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                        0u, 0u, "resume the stream with the same destination");
    if (error != NULL) {
      *error = stream->error;
    }
    return LONEJSON_STREAM_ERROR;
  }

  for (;;) {
    if (lonejson__parser_root_complete(stream->parser)) {
      if (stream->parser->error.code == LONEJSON_STATUS_OK &&
          !stream->parser->error.truncated) {
        stream->error.code = LONEJSON_STATUS_OK;
        stream->error.line = 0u;
        stream->error.column = 0u;
        stream->error.offset = 0u;
        stream->error.system_errno = 0;
        stream->error.truncated = 0;
        stream->error.message[0] = '\0';
      } else {
        stream->error = stream->parser->error;
      }
      stream->object_in_progress = 0;
      stream->current_dst = NULL;
      if (error != NULL) {
        *error = stream->error;
      }
      return LONEJSON_STREAM_OBJECT;
    }

    if (stream->buffered_start == stream->buffered_end) {
      lonejson_read_result chunk = lonejson__stream_read(stream);
      if (chunk.error_code != 0) {
        stream->error.system_errno = chunk.error_code;
        lonejson__set_error(&stream->error, LONEJSON_STATUS_IO_ERROR, 0u, 0u,
                            0u, "stream read failed");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_ERROR;
      }
      if (chunk.would_block) {
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_WOULD_BLOCK;
      }
      if (chunk.bytes_read == 0u && chunk.eof) {
        stream->saw_eof = 1;
        if (!stream->parser->root_started) {
          if (error != NULL) {
            *error = stream->error;
          }
          return LONEJSON_STREAM_EOF;
        }
        stream->error.code = lonejson_parser_finish(stream->parser);
        stream->error = stream->parser->error;
        stream->object_in_progress = 0;
        stream->current_dst = NULL;
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_ERROR;
      }
      stream->buffered_start = 0u;
      stream->buffered_end = chunk.bytes_read;
      continue;
    }

    if (!stream->parser->root_started) {
      size_t start = stream->buffered_start;
      while (start < stream->buffered_end &&
             lonejson__is_json_space(stream->io_buffer[start])) {
        start++;
      }
      stream->buffered_start = start;
      if (stream->buffered_start == stream->buffered_end) {
        continue;
      }
      if (stream->io_buffer[stream->buffered_start] != '{') {
        lonejson__set_error(&stream->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                            0u, 0u, "expected top-level object");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_ERROR;
      }
    }

    {
      size_t consumed;

      status = lonejson__parser_feed_bytes(
          stream->parser, stream->io_buffer + stream->buffered_start,
          stream->buffered_end - stream->buffered_start, &consumed, 1);
      stream->buffered_start += consumed;
    }
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      stream->error = stream->parser->error;
      stream->object_in_progress = 0;
      stream->current_dst = NULL;
      if (error != NULL) {
        *error = stream->error;
      }
      return LONEJSON_STREAM_ERROR;
    }
    if (lonejson__parser_root_complete(stream->parser)) {
      if (stream->parser->error.code == LONEJSON_STATUS_OK &&
          !stream->parser->error.truncated) {
        stream->error.code = LONEJSON_STATUS_OK;
        stream->error.line = 0u;
        stream->error.column = 0u;
        stream->error.offset = 0u;
        stream->error.system_errno = 0;
        stream->error.truncated = 0;
        stream->error.message[0] = '\0';
      } else {
        stream->error = stream->parser->error;
      }
      stream->object_in_progress = 0;
      stream->current_dst = NULL;
      if (error != NULL) {
        *error = stream->error;
      }
      return LONEJSON_STREAM_OBJECT;
    }
  }
}

const lonejson_error *lonejson_stream_error(const lonejson_stream *stream) {
  return stream ? &stream->error : NULL;
}

void lonejson_stream_close(lonejson_stream *stream) {
  if (stream == NULL) {
    return;
  }
  lonejson_parser_destroy(stream->parser);
  if (stream->owns_fp && stream->fp != NULL) {
    fclose(stream->fp);
  }
  if (stream->owns_fd && stream->fd >= 0) {
    close(stream->fd);
  }
  LONEJSON_FREE(stream);
}

lonejson_status lonejson_validate_buffer(const void *data, size_t len,
                                         lonejson_error *error) {
  lonejson_parser *parser;
  lonejson_status status;
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE];

  if (data == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "buffer is required");
  }
  parser = &parser_storage;
  lonejson__parser_init_state(parser, NULL, NULL, NULL, 1, parser_workspace,
                              sizeof(parser_workspace));
  lonejson__clear_error(error);
  status = lonejson_parser_feed(parser, data, len);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    status = lonejson_parser_finish(parser);
  }
  if (error != NULL) {
    *error = parser->error;
  }
  return status;
}

lonejson_status lonejson_validate_cstr(const char *json,
                                       lonejson_error *error) {
  if (json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "json string is required");
  }
  return lonejson_validate_buffer(json, strlen(json), error);
}

lonejson_status lonejson_validate_reader(lonejson_reader_fn reader, void *user,
                                         lonejson_error *error) {
  lonejson_parser *parser;
  unsigned char buffer[LONEJSON_READER_BUFFER_SIZE];
  lonejson_status status = LONEJSON_STATUS_OK;
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE];

  if (reader == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "reader callback is required");
  }
  parser = &parser_storage;
  lonejson__parser_init_state(parser, NULL, NULL, NULL, 1, parser_workspace,
                              sizeof(parser_workspace));
  lonejson__clear_error(error);
  for (;;) {
    lonejson_read_result chunk = reader(user, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      parser->error.system_errno = chunk.error_code;
      status = lonejson__set_error(
          &parser->error, LONEJSON_STATUS_IO_ERROR, parser->error.offset,
          parser->error.line, parser->error.column, "reader callback failed");
      break;
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson_parser_feed(parser, buffer, chunk.bytes_read);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        break;
      }
    }
    if (chunk.eof) {
      status = lonejson_parser_finish(parser);
      break;
    }
  }
  if (error != NULL) {
    *error = parser->error;
  }
  return status;
}

lonejson_status lonejson_validate_filep(FILE *fp, lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer is required");
  }
  return lonejson_validate_reader(lonejson__file_reader, fp, error);
}

lonejson_status lonejson_validate_path(const char *path,
                                       lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path is required");
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 1u, 0u,
                               "failed to open '%s'", path);
  }
  status = lonejson_validate_filep(fp, error);
  fclose(fp);
  return status;
}

lonejson_status lonejson_serialize_sink(const lonejson_map *map,
                                        const void *src, lonejson_sink_fn sink,
                                        void *user,
                                        const lonejson_write_options *options,
                                        lonejson_error *error) {
  lonejson_status status;
  lonejson__write_state state;

  if (map == NULL || src == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map, source, and sink are required");
  }
  lonejson__clear_error(error);
  if (options != NULL && options->pretty) {
    state.sink = sink;
    state.user = user;
    state.error = error;
    state.pretty = 1;
    state.depth = 0u;
    status = lonejson__serialize_map_pretty(map, src, &state);
  } else {
    status = lonejson__serialize_map_compact(map, src, sink, user, error);
  }
  return status;
}

lonejson_status lonejson_serialize_buffer(const lonejson_map *map,
                                          const void *src, char *buffer,
                                          size_t capacity, size_t *needed,
                                          const lonejson_write_options *options,
                                          lonejson_error *error) {
  lonejson_buffer_sink sink;
  lonejson_write_options local_options;
  lonejson_status status;

  if (buffer == NULL || capacity == 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output buffer and capacity are required");
  }
  local_options = options ? *options : lonejson_default_write_options();
  sink.buffer = buffer;
  sink.capacity = capacity;
  sink.length = 0u;
  sink.needed = 0u;
  sink.policy = local_options.overflow_policy;
  sink.truncated = 0;
  buffer[0] = '\0';
  status = lonejson_serialize_sink(
      map, src,
      (local_options.overflow_policy == LONEJSON_OVERFLOW_FAIL)
          ? lonejson__sink_buffer_exact
          : lonejson__sink_buffer,
      &sink, &local_options, error);
  sink.buffer[(sink.length < sink.capacity) ? sink.length
                                            : (sink.capacity - 1u)] = '\0';
  if (needed != NULL) {
    *needed = sink.needed;
  }
  if (status == LONEJSON_STATUS_OK && sink.truncated &&
      local_options.overflow_policy == LONEJSON_OVERFLOW_TRUNCATE) {
    status = LONEJSON_STATUS_TRUNCATED;
  }
  return status;
}

char *lonejson_serialize_alloc(const lonejson_map *map, const void *src,
                               size_t *out_len,
                               const lonejson_write_options *options,
                               lonejson_error *error) {
  lonejson_write_options local_options =
      options ? *options : lonejson_default_write_options();
  lonejson_buffer_sink sink;
  lonejson_status status;
  char *buffer;

  sink.buffer = NULL;
  sink.capacity = 0u;
  sink.length = 0u;
  sink.needed = 0u;
  sink.policy = local_options.overflow_policy;
  sink.truncated = 0;
  status = lonejson_serialize_sink(map, src, lonejson__sink_count, &sink,
                                   &local_options, error);
  if (status != LONEJSON_STATUS_OK && sink.needed == 0u) {
    return NULL;
  }
  buffer = (char *)LONEJSON_MALLOC(sink.needed + 1u);
  if (buffer == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate output buffer");
    return NULL;
  }
  local_options.overflow_policy = LONEJSON_OVERFLOW_FAIL;
  if (lonejson_serialize_buffer(map, src, buffer, sink.needed + 1u,
                                &sink.needed, &local_options,
                                error) != LONEJSON_STATUS_OK) {
    LONEJSON_FREE(buffer);
    return NULL;
  }
  if (out_len != NULL) {
    *out_len = sink.needed;
  }
  return buffer;
}

lonejson_status lonejson_serialize_filep(const lonejson_map *map,
                                         const void *src, FILE *fp,
                                         const lonejson_write_options *options,
                                         lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer is required");
  }
  return lonejson_serialize_sink(map, src, lonejson__sink_file, fp, options,
                                 error);
}

lonejson_status lonejson_serialize_path(const lonejson_map *map,
                                        const void *src, const char *path,
                                        const lonejson_write_options *options,
                                        lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path is required");
  }
  fp = fopen(path, "wb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open '%s' for writing", path);
  }
  status = lonejson_serialize_filep(map, src, fp, options, error);
  fclose(fp);
  return status;
}

lonejson_status lonejson_serialize_jsonl_sink(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    lonejson_sink_fn sink, void *user, const lonejson_write_options *options,
    lonejson_error *error) {
  lonejson_status status;
  lonejson__write_state state;

  if (map == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map and sink are required");
  }
  if (count != 0u && items == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "items are required when count is non-zero");
  }
  if (stride == 0u) {
    stride = map->struct_size;
  }
  if (stride < map->struct_size) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "stride must be at least map->struct_size");
  }
  lonejson__clear_error(error);
  if (options != NULL && options->pretty) {
    state.sink = sink;
    state.user = user;
    state.error = error;
    state.pretty = 1;
    state.depth = 0u;
    status =
        lonejson__serialize_jsonl_records(map, items, count, stride, &state);
  } else {
    status = lonejson__serialize_jsonl_records_compact(
        map, items, count, stride, sink, user, error);
  }
  return status;
}

lonejson_status lonejson_serialize_jsonl_buffer(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    char *buffer, size_t capacity, size_t *needed,
    const lonejson_write_options *options, lonejson_error *error) {
  lonejson_buffer_sink sink;
  lonejson_write_options local_options;
  lonejson_status status;

  if (buffer == NULL || capacity == 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output buffer and capacity are required");
  }
  local_options = options ? *options : lonejson_default_write_options();
  sink.buffer = buffer;
  sink.capacity = capacity;
  sink.length = 0u;
  sink.needed = 0u;
  sink.policy = local_options.overflow_policy;
  sink.truncated = 0;
  buffer[0] = '\0';
  status = lonejson_serialize_jsonl_sink(
      map, items, count, stride,
      (local_options.overflow_policy == LONEJSON_OVERFLOW_FAIL)
          ? lonejson__sink_buffer_exact
          : lonejson__sink_buffer,
      &sink, &local_options, error);
  sink.buffer[(sink.length < sink.capacity) ? sink.length
                                            : (sink.capacity - 1u)] = '\0';
  if (needed != NULL) {
    *needed = sink.needed;
  }
  if (status == LONEJSON_STATUS_OK && sink.truncated &&
      local_options.overflow_policy == LONEJSON_OVERFLOW_TRUNCATE) {
    status = LONEJSON_STATUS_TRUNCATED;
  }
  return status;
}

char *lonejson_serialize_jsonl_alloc(const lonejson_map *map, const void *items,
                                     size_t count, size_t stride,
                                     size_t *out_len,
                                     const lonejson_write_options *options,
                                     lonejson_error *error) {
  lonejson_write_options local_options =
      options ? *options : lonejson_default_write_options();
  lonejson_buffer_sink sink;
  lonejson_status status;
  char *buffer;

  sink.buffer = NULL;
  sink.capacity = 0u;
  sink.length = 0u;
  sink.needed = 0u;
  sink.policy = local_options.overflow_policy;
  sink.truncated = 0;
  status = lonejson_serialize_jsonl_sink(map, items, count, stride,
                                         lonejson__sink_count, &sink,
                                         &local_options, error);
  if (status != LONEJSON_STATUS_OK && sink.needed == 0u) {
    return NULL;
  }
  buffer = (char *)LONEJSON_MALLOC(sink.needed + 1u);
  if (buffer == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate JSONL output buffer");
    return NULL;
  }
  local_options.overflow_policy = LONEJSON_OVERFLOW_FAIL;
  if (lonejson_serialize_jsonl_buffer(
          map, items, count, stride, buffer, sink.needed + 1u, &sink.needed,
          &local_options, error) != LONEJSON_STATUS_OK) {
    LONEJSON_FREE(buffer);
    return NULL;
  }
  if (out_len != NULL) {
    *out_len = sink.needed;
  }
  return buffer;
}

lonejson_status lonejson_serialize_jsonl_filep(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    FILE *fp, const lonejson_write_options *options, lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer is required");
  }
  return lonejson_serialize_jsonl_sink(map, items, count, stride,
                                       lonejson__sink_file, fp, options, error);
}

lonejson_status
lonejson_serialize_jsonl_path(const lonejson_map *map, const void *items,
                              size_t count, size_t stride, const char *path,
                              const lonejson_write_options *options,
                              lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path is required");
  }
  fp = fopen(path, "wb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open '%s' for writing", path);
  }
  status = lonejson_serialize_jsonl_filep(map, items, count, stride, fp,
                                          options, error);
  fclose(fp);
  return status;
}

void lonejson_cleanup(const lonejson_map *map, void *value) {
  lonejson__cleanup_map(map, value);
}

void lonejson_reset(const lonejson_map *map, void *value) {
  lonejson__reset_map(map, value);
}

#ifdef LONEJSON_WITH_CURL
lonejson_status
lonejson_curl_parse_init(lonejson_curl_parse *ctx, const lonejson_map *map,
                         void *dst, const lonejson_parse_options *options) {
  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->parser = lonejson__parser_create_ex(map, dst, options, &ctx->error, 0);
  return ctx->parser ? LONEJSON_STATUS_OK : ctx->error.code;
}

size_t lonejson_curl_write_callback(char *ptr, size_t size, size_t nmemb,
                                    void *userdata) {
  lonejson_curl_parse *ctx = (lonejson_curl_parse *)userdata;
  size_t bytes = size * nmemb;
  lonejson_status status;
  if (ctx == NULL || ctx->parser == NULL) {
    return 0u;
  }
  status = lonejson_parser_feed(ctx->parser, ptr, bytes);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    return bytes;
  }
  ctx->error = ((lonejson_parser *)ctx->parser)->error;
  return 0u;
}

lonejson_status lonejson_curl_parse_finish(lonejson_curl_parse *ctx) {
  if (ctx == NULL || ctx->parser == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  ctx->error.code = lonejson_parser_finish(ctx->parser);
  ctx->error = ((lonejson_parser *)ctx->parser)->error;
  return ctx->error.code;
}

void lonejson_curl_parse_cleanup(lonejson_curl_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  lonejson_parser_destroy(ctx->parser);
  ctx->parser = NULL;
}

lonejson_status
lonejson_curl_upload_init(lonejson_curl_upload *ctx, const lonejson_map *map,
                          const void *src,
                          const lonejson_write_options *options) {
  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->json =
      lonejson_serialize_alloc(map, src, &ctx->length, options, &ctx->error);
  return ctx->json ? LONEJSON_STATUS_OK : ctx->error.code;
}

size_t lonejson_curl_read_callback(char *ptr, size_t size, size_t nmemb,
                                   void *userdata) {
  lonejson_curl_upload *ctx = (lonejson_curl_upload *)userdata;
  size_t capacity = size * nmemb;
  size_t remaining;
  size_t chunk;

  if (ctx == NULL || ptr == NULL) {
    return CURL_READFUNC_ABORT;
  }
  if (ctx->offset >= ctx->length) {
    return 0u;
  }
  remaining = ctx->length - ctx->offset;
  chunk = (remaining < capacity) ? remaining : capacity;
  memcpy(ptr, ctx->json + ctx->offset, chunk);
  ctx->offset += chunk;
  return chunk;
}

curl_off_t lonejson_curl_upload_size(const lonejson_curl_upload *ctx) {
  return ctx ? (curl_off_t)ctx->length : 0;
}

void lonejson_curl_upload_cleanup(lonejson_curl_upload *ctx) {
  if (ctx == NULL) {
    return;
  }
  LONEJSON_FREE(ctx->json);
  ctx->json = NULL;
  ctx->length = 0u;
  ctx->offset = 0u;
}
#endif

#endif

#endif
