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
 * lonejson is a schema-guided JSON library for C. In the source tree and in
 * the binary release archives, this header is the public declarations-only
 * interface used with the compiled `liblonejson` library. The release process
 * also produces a separate standalone single-header artifact for projects that
 * prefer to vendor one file directly. lonejson is meant for programs that
 * already know the shape of the data they care about and want to move between
 * JSON objects and C structs without dragging a generic DOM through the middle
 * of the program. The library can parse from memory, files, file descriptors,
 * and object-framed streams; it can serialize to buffers, sinks, files, and
 * JSON Lines; and it can keep very large field values out of memory by
 * spilling inbound values to temporary files or by serializing outbound values
 * directly from paths, `FILE *`, or file descriptors.
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
 * In the normal linked-library build, include this header and link
 * `liblonejson`:
 *
 * ```c
 * #include "lonejson.h"
 * ```
 *
 * If you are using the generated standalone single-header release artifact,
 * define `LONEJSON_IMPLEMENTATION` in exactly one translation unit before
 * including that generated header:
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
 * #include "lonejson.h"
 *
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

#endif
