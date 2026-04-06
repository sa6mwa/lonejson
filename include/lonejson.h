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
#define LONEJSON_SHORT_ALIAS_INLINE                                            \
  static __inline__ __attribute__((unused, always_inline))
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
 * values by `lonejson_source`, and arbitrary embedded JSON values by
 * `lonejson_json_value`. One arbitrary JSON value can also be consumed without
 * a schema through `lonejson_value_visitor` and `lonejson_visit_value_*`.
 * Behavior is configured through `lonejson_parse_options`,
 * `lonejson_spool_options`, and `lonejson_write_options`.
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
 * Embedded arbitrary JSON values are stream-first on parse. The caller
 * configures whether the nested value should be streamed to a raw sink,
 * streamed as structured visitor callbacks, or explicitly captured for later
 * reuse:
 *
 * ```c
 * typedef struct query_doc {
 *   char namespace_[16];
 *   lonejson_json_value selector;
 * } query_doc;
 *
 * static const lonejson_field query_doc_fields[] = {
 *   LONEJSON_FIELD_STRING_FIXED_REQ(query_doc, namespace_, "namespace",
 *                                   LONEJSON_OVERFLOW_FAIL),
 *   LONEJSON_FIELD_JSON_VALUE_REQ(query_doc, selector, "selector")
 * };
 *
 * LONEJSON_MAP_DEFINE(query_doc_map, query_doc, query_doc_fields);
 *
 * query_doc doc;
 * lonejson_parse_options options = lonejson_default_parse_options();
 *
 * lonejson_init(&query_doc_map, &doc);
 * options.clear_destination = 0;
 * lonejson_json_value_enable_parse_capture(&doc.selector, &error);
 * lonejson_parse_cstr(&query_doc_map, &doc,
 *                     "{\"namespace\":\"ops\",\"selector\":{\"op\":\"and\"}}",
 *                     &options, &error);
 * lonejson_json_value_write_to_sink(&doc.selector, sink_fn, sink_user, &error);
 * lonejson_cleanup(&query_doc_map, &doc);
 * ```
 *
 * The lower-level visitor API is useful when you need to parse exactly one
 * JSON value without a schema and without retaining it:
 *
 * ```c
 * lonejson_value_visitor visitor = lonejson_default_value_visitor();
 * lonejson_value_limits limits = lonejson_default_value_limits();
 *
 * visitor.object_begin = on_object_begin;
 * visitor.object_key_chunk = on_object_key_chunk;
 * visitor.string_chunk = on_string_chunk;
 * visitor.number_chunk = on_number_chunk;
 *
 * lonejson_visit_value_cstr("{\"ok\":true,\"n\":42}", &visitor, user, &limits,
 *                           &error);
 * ```
 *
 * The repository examples expand these patterns into complete programs covering
 * one-shot parsing, object-framed streams, fixed-capacity fields, spool-to-disk
 * fields, source-backed outbound fields, JSONL output, optional curl
 * integration, and the Lua binding. For public structs, prefer lonejson's
 * explicit initializers and default helpers over manual `memset` or `{0}`:
 * `lonejson_init`, `lonejson_error_init`, `lonejson_default_parse_options`,
 * `lonejson_default_write_options`, `lonejson_default_value_visitor`, and the
 * handle-specific `*_init` helpers.
 */

/** Major component of the lonejson header version. */
#define LONEJSON_VERSION_MAJOR 0
/** Minor component of the lonejson header version. */
#define LONEJSON_VERSION_MINOR 1
/** Patch component of the lonejson header version. */
#define LONEJSON_VERSION_PATCH 0
/** Shared-library ABI / SONAME version for binary compatibility tracking. */
#define LONEJSON_ABI_VERSION 2

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
  LONEJSON_FIELD_KIND_JSON_VALUE,
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

/** Backing mode used by `lonejson_json_value` opaque embedded JSON handles. */
typedef enum lonejson_json_value_kind {
  /** No value configured. Serializes as JSON `null`. */
  LONEJSON_JSON_VALUE_NULL = 0,
  /** Owned compact JSON bytes retained in memory. */
  LONEJSON_JSON_VALUE_BUFFER = 1,
  /** Read JSON bytes from a caller-provided reader callback. */
  LONEJSON_JSON_VALUE_READER = 2,
  /** Read JSON bytes from a caller-provided `FILE *`. */
  LONEJSON_JSON_VALUE_FILE = 3,
  /** Read JSON bytes from a caller-provided file descriptor. */
  LONEJSON_JSON_VALUE_FD = 4,
  /** Open and read JSON bytes from a filesystem path on each serialization. */
  LONEJSON_JSON_VALUE_PATH = 5
} lonejson_json_value_kind;

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

/** Detailed error information populated by most public APIs. Caller-provided
 * error outputs do not need prior initialization because lonejson overwrites
 * them on entry, but `lonejson_error_init` is available when you want an
 * explicit known-empty state.
 */
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

/** Optional allocation counters used by custom allocators. lonejson updates
 * these counters only in debug builds. Release builds leave them untouched.
 */
typedef struct lonejson_allocator_stats {
  /** Number of successful allocate calls performed by lonejson. */
  size_t alloc_calls;
  /** Number of successful realloc calls performed by lonejson. */
  size_t realloc_calls;
  /** Number of free calls performed by lonejson. */
  size_t free_calls;
  /** Current user-visible bytes owned by lonejson through this allocator. */
  size_t bytes_live;
  /** Peak user-visible bytes owned by lonejson through this allocator. */
  size_t peak_bytes_live;
} lonejson_allocator_stats;

/** Callback signature used for lonejson-owned allocations. */
typedef void *(*lonejson_malloc_fn)(void *ctx, size_t size);
/** Callback signature used for lonejson-owned reallocations. */
typedef void *(*lonejson_realloc_fn)(void *ctx, void *ptr, size_t size);
/** Callback signature used for lonejson-owned frees. */
typedef void (*lonejson_free_fn)(void *ctx, void *ptr);

/** Allocator vtable used by parser, spool, stream, and lonejson-owned output
 * buffers. When all three callbacks are `NULL`, lonejson falls back to
 * `lonejson_default_allocator()`. Partial callback sets are invalid; callers
 * must provide either all callbacks or none.
 */
typedef struct lonejson_allocator {
  /** Allocation callback. `NULL` means use the default allocator. */
  lonejson_malloc_fn malloc_fn;
  /** Reallocation callback. `NULL` means use the default allocator. */
  lonejson_realloc_fn realloc_fn;
  /** Free callback. `NULL` means use the default allocator. */
  lonejson_free_fn free_fn;
  /** Opaque allocator context passed to every callback. */
  void *ctx;
  /** Optional debug allocation counters updated by lonejson in debug builds. */
  lonejson_allocator_stats *stats;
} lonejson_allocator;

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
  /** Allocator used for lonejson-owned spool metadata and buffers. */
  lonejson_allocator allocator;
  /** Internal state marker used by lonejson to distinguish initialized
   * handles from uninitialized memory. Treat as opaque.
   */
  unsigned _lonejson_magic;
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
typedef struct lonejson_generator lonejson_generator;

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

/** Optional controls for mapped parsing and streaming. Use
 * `lonejson_default_parse_options()` instead of manual zeroing so new fields
 * keep their intended defaults.
 */
typedef struct lonejson_parse_options {
  /** When non-zero, lonejson initializes the destination before parsing. */
  int clear_destination;
  /** When non-zero, duplicate object keys cause
   * `LONEJSON_STATUS_DUPLICATE_FIELD`.
   */
  int reject_duplicate_keys;
  /** Maximum allowed nesting depth before parsing fails with overflow. */
  size_t max_depth;
  /** Optional allocator used for parser-owned runtime state and lonejson-owned
   * field allocations created while parsing.
   */
  const lonejson_allocator *allocator;
} lonejson_parse_options;

/** Result returned by reader callbacks and `lonejson_spooled_read`. Use
 * `lonejson_default_read_result()` when constructing one manually inside a
 * reader callback instead of open-coding `memset` or `{0}`.
 */
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
/** Generic sink callback used by serializer APIs and raw spool writers. */
typedef lonejson_status (*lonejson_sink_fn)(void *user, const void *data,
                                            size_t len, lonejson_error *error);

/** Controls how a `lonejson_json_value` field receives inbound parsed JSON.
 * Parse is stream-first by default; callers must deliberately choose one of
 * these modes before decoding into a `JSON_VALUE` field.
 */
typedef enum lonejson_json_value_parse_mode {
  /** Parsing a `JSON_VALUE` field fails unless the caller configures a parse
   * sink or enables explicit capture first.
   */
  LONEJSON_JSON_VALUE_PARSE_NONE = 0,
  /** Parsing streams compact JSON bytes incrementally to a caller sink. */
  LONEJSON_JSON_VALUE_PARSE_SINK = 1,
  /** Parsing streams structured JSON visitor events incrementally. */
  LONEJSON_JSON_VALUE_PARSE_VISITOR = 2,
  /** Parsing captures compact JSON bytes into owned storage. */
  LONEJSON_JSON_VALUE_PARSE_CAPTURE = 3
} lonejson_json_value_parse_mode;

/** Limits applied while visiting an arbitrary JSON value. Zero means "use the
 * library default" for that field except `max_total_bytes`, where zero means
 * "unlimited". The default helper currently resolves to a depth limit of `64`,
 * a decoded string limit of `1 MiB`, a key limit of `64 KiB`, and a number
 * token limit of `256` bytes.
 */
typedef struct lonejson_value_limits {
  /** Maximum allowed nesting depth while parsing a value. */
  size_t max_depth;
  /** Maximum decoded byte length for one JSON string value. */
  size_t max_string_bytes;
  /** Maximum raw byte length for one JSON number token. */
  size_t max_number_bytes;
  /** Maximum decoded byte length for one object key. */
  size_t max_key_bytes;
  /** Maximum raw byte length consumed while parsing the value. Zero means
   * unlimited.
   */
  size_t max_total_bytes;
} lonejson_value_limits;

/** Callback signature for structure and boundary events during arbitrary JSON
 * value visiting.
 */
typedef lonejson_status (*lonejson_value_event_fn)(void *user,
                                                   lonejson_error *error);
/** Callback signature for chunked decoded text delivery during arbitrary JSON
 * value visiting.
 */
typedef lonejson_status (*lonejson_value_chunk_fn)(void *user, const char *data,
                                                   size_t len,
                                                   lonejson_error *error);
/** Callback signature for boolean scalar delivery during arbitrary JSON value
 * visiting.
 */
typedef lonejson_status (*lonejson_value_bool_fn)(void *user, int value,
                                                  lonejson_error *error);

/** Visitor callbacks for one arbitrary JSON value. String values and object
 * keys are delivered as decoded UTF-8 in chunks. Number values are delivered as
 * raw token bytes in chunks. Any callback may be `NULL` when the caller does
 * not need that event. Use `lonejson_default_value_visitor()` instead of
 * manual zeroing when you want the empty visitor state. The callback sequence
 * is balanced:
 * `object_begin/object_end`, `array_begin/array_end`, and matching
 * begin/chunk/end triplets for keys, strings, and numbers.
 */
typedef struct lonejson_value_visitor {
  lonejson_value_event_fn object_begin;
  lonejson_value_event_fn object_end;
  lonejson_value_event_fn object_key_begin;
  lonejson_value_chunk_fn object_key_chunk;
  lonejson_value_event_fn object_key_end;
  lonejson_value_event_fn array_begin;
  lonejson_value_event_fn array_end;
  lonejson_value_event_fn string_begin;
  lonejson_value_chunk_fn string_chunk;
  lonejson_value_event_fn string_end;
  lonejson_value_event_fn number_begin;
  lonejson_value_chunk_fn number_chunk;
  lonejson_value_event_fn number_end;
  lonejson_value_bool_fn boolean_value;
  lonejson_value_event_fn null_value;
} lonejson_value_visitor;

/** Opaque JSON value handle used by embedded arbitrary JSON fields. Parsing is
 * stream-first: the caller must configure either a parse sink, a parse
 * visitor, or explicit parse capture before decoding inbound JSON into this
 * field. Serialization can source one JSON value from memory, reader
 * callbacks, files, file descriptors, or reopenable paths.
 */
typedef struct lonejson_json_value {
  /** Current configured value kind. `LONEJSON_JSON_VALUE_NULL` emits `null`. */
  lonejson_json_value_kind kind;
  /** Owned compact JSON bytes when `kind == LONEJSON_JSON_VALUE_BUFFER`. */
  char *json;
  /** Logical byte length of `json`. */
  size_t len;
  /** Caller-provided reader callback when `kind == LONEJSON_JSON_VALUE_READER`.
   */
  lonejson_reader_fn reader;
  /** Caller-provided reader callback user data. */
  void *reader_user;
  /** Caller-provided file handle when `kind == LONEJSON_JSON_VALUE_FILE`. */
  FILE *fp;
  /** Caller-provided file descriptor when `kind == LONEJSON_JSON_VALUE_FD`. */
  int fd;
  /** Owned filesystem path when `kind == LONEJSON_JSON_VALUE_PATH`. */
  char *path;
  /** Parse-time delivery mode for inbound JSON values. */
  lonejson_json_value_parse_mode parse_mode;
  /** Caller-provided sink callback when `parse_mode ==
   * LONEJSON_JSON_VALUE_PARSE_SINK`.
   */
  lonejson_sink_fn parse_sink;
  /** Caller-provided sink callback user data. */
  void *parse_sink_user;
  /** Caller-provided visitor when `parse_mode ==
   * LONEJSON_JSON_VALUE_PARSE_VISITOR`.
   */
  const lonejson_value_visitor *parse_visitor;
  /** Caller-provided visitor user data. */
  void *parse_visitor_user;
  /** Limits applied while parsing through `parse_visitor`. */
  lonejson_value_limits parse_visitor_limits;
  /** Allocator used for lonejson-owned captured JSON and path state. */
  lonejson_allocator allocator;
  /** Internal state marker used by lonejson to distinguish initialized
   * handles from uninitialized memory. Treat as opaque.
   */
  unsigned _lonejson_magic;
} lonejson_json_value;

/** Options controlling serialization into buffers and sinks. Use
 * `lonejson_default_write_options()` instead of manual zeroing so new fields
 * keep their intended defaults.
 */
typedef struct lonejson_write_options {
  /** Buffer overflow handling policy for fixed-capacity output buffers. */
  lonejson_overflow_policy overflow_policy;
  /** Non-zero to emit two-space-indented pretty JSON instead of compact JSON.
   */
  int pretty;
  /** Optional allocator used for serializer-owned output buffers. */
  const lonejson_allocator *allocator;
} lonejson_write_options;

/** Owned serializer output buffer returned by alloc-returning APIs.
 *
 * Read `data` and `len`, then release the handle with
 * `lonejson_owned_buffer_free()`. Treat `alloc_size` and `allocator` as
 * internal cleanup state.
 */
typedef struct lonejson_owned_buffer {
  /** NUL-terminated serialized output bytes owned by this handle. */
  char *data;
  /** Logical byte length of `data`, excluding the trailing NUL. */
  size_t len;
  /** Internal allocation size used for symmetric cleanup. */
  size_t alloc_size;
  /** Allocator that owns `data`. */
  lonejson_allocator allocator;
} lonejson_owned_buffer;

/** Pull-style JSON generator for one mapped value.
 *
 * Generators are explicit, bounded serializer states meant for transport
 * adapters and other pull-based consumers. They do not materialize the full
 * payload. They support the normal mapped serializer surface plus
 * `lonejson_source`, `lonejson_spooled`, and `lonejson_json_value` fields.
 * `json_value` fields are streamed through the generator directly instead of
 * silently falling back to buffered serialization.
 */
struct lonejson_generator {
  /** Opaque generator state owned by lonejson. */
  void *state;
  /** Non-zero once the generator has reached end-of-stream. */
  int eof;
  /** Last generator error. Cleared by init and cleanup. */
  lonejson_error error;
};

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

/** Maps an arbitrary embedded JSON value into a `lonejson_json_value` member.
 * The field accepts any JSON value on parse and emits that value directly on
 * serialize without wrapping it as a JSON string. Before parsing into this
 * field, configure the destination handle for sink, visitor, or capture mode.
 */
#define LONEJSON_FIELD_JSON_VALUE(type, member, key)                           \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_JSON_VALUE,                                             \
   LONEJSON_STORAGE_FIXED,                                                     \
   LONEJSON_OVERFLOW_FAIL,                                                     \
   0u,                                                                         \
   0u,                                                                         \
   0u,                                                                         \
   NULL,                                                                       \
   NULL}

/** Maps a required arbitrary embedded JSON value into a `lonejson_json_value`
 * member. Before parsing into this field, configure the destination handle
 * for sink, visitor, or capture mode.
 */
#define LONEJSON_FIELD_JSON_VALUE_REQ(type, member, key)                       \
  {key,                                                                        \
   LONEJSON__KEY_LEN(key),                                                     \
   LONEJSON__KEY_FIRST(key),                                                   \
   LONEJSON__KEY_LAST(key),                                                    \
   offsetof(type, member),                                                     \
   LONEJSON_FIELD_KIND_JSON_VALUE,                                             \
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
/** Initializes an error struct to the empty `OK` state. */
void lonejson_error_init(lonejson_error *error);
/** Returns lonejson's default allocator backed by the configured allocation
 * macros.
 */
lonejson_allocator lonejson_default_allocator(void);
/** Returns the empty default read result used by reader callbacks. */
lonejson_read_result lonejson_default_read_result(void);
/** Returns the empty visitor with all callbacks set to `NULL`. */
lonejson_value_visitor lonejson_default_value_visitor(void);
/** Initializes a spool handle to its default or caller-configured empty state
 * using lonejson's default allocator.
 */
void lonejson_spooled_init(lonejson_spooled *value,
                           const lonejson_spool_options *options);
/** Initializes a spool handle with explicit options and allocator selection.
 */
void lonejson_spooled_init_with_allocator(lonejson_spooled *value,
                                          const lonejson_spool_options *options,
                                          const lonejson_allocator *allocator);
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
/** Initializes an embedded JSON value handle to the empty `null` state with no
 * inbound parse destination configured, using lonejson's default allocator.
 * This is the required starting state before setting parse sink, parse
 * visitor, parse capture, or outbound source configuration.
 */
void lonejson_json_value_init(lonejson_json_value *value);
/** Initializes an embedded JSON value handle with an explicit allocator and no
 * inbound parse destination configured.
 */
void lonejson_json_value_init_with_allocator(
    lonejson_json_value *value, const lonejson_allocator *allocator);
/** Resets an embedded JSON value handle to the empty `null` state. */
void lonejson_json_value_reset(lonejson_json_value *value);
/** Releases any storage or path owned by an embedded JSON value handle and
 * resets it. */
void lonejson_json_value_cleanup(lonejson_json_value *value);
/** Validates one JSON value from a memory buffer, stores a compact owned copy,
 * and configures the handle to serialize from memory. Use this when retained
 * ownership is intentional; it is not the default parse path for
 * `JSON_VALUE` fields.
 */
lonejson_status lonejson_json_value_set_buffer(lonejson_json_value *value,
                                               const void *data, size_t len,
                                               lonejson_error *error);
/** Configures an embedded JSON value handle to stream inbound parsed JSON
 * bytes to a caller sink as they are validated. Callers using parse sinks must
 * keep the handle configured across parsing, typically by initializing the
 * destination first and parsing with `clear_destination = 0`.
 */
lonejson_status lonejson_json_value_set_parse_sink(lonejson_json_value *value,
                                                   lonejson_sink_fn sink,
                                                   void *user,
                                                   lonejson_error *error);
/** Configures an embedded JSON value handle to deliver inbound parsed JSON as
 * structured visitor callbacks. Callers using parse visitors must keep the
 * handle configured across parsing, typically by initializing the destination
 * first and parsing with `clear_destination = 0`. The supplied limits may be
 * `NULL` to use `lonejson_default_value_limits()`.
 */
lonejson_status lonejson_json_value_set_parse_visitor(
    lonejson_json_value *value, const lonejson_value_visitor *visitor,
    void *user, const lonejson_value_limits *limits, lonejson_error *error);
/** Enables explicit parse-time capture of one inbound JSON value into owned
 * compact bytes. This is the opt-in storage path for callers that need to
 * retain a parsed arbitrary JSON value after decoding.
 */
lonejson_status
lonejson_json_value_enable_parse_capture(lonejson_json_value *value,
                                         lonejson_error *error);
/** Configures an embedded JSON value handle to stream from a caller-provided
 * reader callback at serialize time. Reader-backed values are consumed in one
 * pass per serialization call. */
lonejson_status lonejson_json_value_set_reader(lonejson_json_value *value,
                                               lonejson_reader_fn reader,
                                               void *user,
                                               lonejson_error *error);
/** Configures an embedded JSON value handle to read from a caller-owned
 * `FILE *`. */
lonejson_status lonejson_json_value_set_file(lonejson_json_value *value,
                                             FILE *fp, lonejson_error *error);
/** Configures an embedded JSON value handle to read from a caller-owned file
 * descriptor. */
lonejson_status lonejson_json_value_set_fd(lonejson_json_value *value, int fd,
                                           lonejson_error *error);
/** Configures an embedded JSON value handle to reopen and stream a filesystem
 * path. lonejson owns the duplicated path string and frees it on cleanup. */
lonejson_status lonejson_json_value_set_path(lonejson_json_value *value,
                                             const char *path,
                                             lonejson_error *error);
/** Streams the compact form of one embedded JSON value to a generic sink while
 * validating that the handle contains exactly one JSON value. */
lonejson_status
lonejson_json_value_write_to_sink(const lonejson_json_value *value,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error);

/** Returns the library's default parse options. The current defaults clear the
 * destination, reject duplicate keys, and cap nesting depth at `64`.
 */
lonejson_parse_options lonejson_default_parse_options(void);
/** Returns the library's default limits for arbitrary JSON value visitors.
 * The current defaults are depth `64`, string `1 MiB`, key `64 KiB`, number
 * token `256` bytes, and unlimited total bytes.
 */
lonejson_value_limits lonejson_default_value_limits(void);
/** Returns the library's default write options. The current defaults use
 * compact output with `LONEJSON_OVERFLOW_FAIL`.
 */
lonejson_write_options lonejson_default_write_options(void);
/** Returns an empty owned-buffer handle that uses the default allocator. */
lonejson_owned_buffer lonejson_default_owned_buffer(void);
/** Initializes an owned-buffer handle to its empty default state. */
void lonejson_owned_buffer_init(lonejson_owned_buffer *buffer);
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

/** Visits exactly one JSON value from a caller-provided buffer. The call
 * fails on malformed JSON or on trailing non-whitespace bytes after the first
 * complete value.
 */
lonejson_status lonejson_visit_value_buffer(
    const void *data, size_t len, const lonejson_value_visitor *visitor,
    void *user, const lonejson_value_limits *limits, lonejson_error *error);
/** Visits exactly one JSON value from a NUL-terminated string. The call fails
 * on malformed JSON or on trailing non-whitespace bytes after the first
 * complete value.
 */
lonejson_status lonejson_visit_value_cstr(const char *json,
                                          const lonejson_value_visitor *visitor,
                                          void *user,
                                          const lonejson_value_limits *limits,
                                          lonejson_error *error);
/** Visits exactly one JSON value from a caller-provided reader callback. The
 * call fails on malformed JSON or on trailing non-whitespace bytes after the
 * first complete value.
 */
lonejson_status
lonejson_visit_value_reader(lonejson_reader_fn reader, void *reader_user,
                            const lonejson_value_visitor *visitor, void *user,
                            const lonejson_value_limits *limits,
                            lonejson_error *error);
/** Visits exactly one JSON value from an open `FILE *`. The call fails on
 * malformed JSON or on trailing non-whitespace bytes after the first complete
 * value.
 */
lonejson_status
lonejson_visit_value_filep(FILE *fp, const lonejson_value_visitor *visitor,
                           void *user, const lonejson_value_limits *limits,
                           lonejson_error *error);
/** Visits exactly one JSON value from a filesystem path. The call fails on
 * malformed JSON or on trailing non-whitespace bytes after the first complete
 * value.
 */
lonejson_status lonejson_visit_value_path(const char *path,
                                          const lonejson_value_visitor *visitor,
                                          void *user,
                                          const lonejson_value_limits *limits,
                                          lonejson_error *error);
/** Visits exactly one JSON value from a file descriptor, including Unix domain
 * sockets. The call fails on malformed JSON or on trailing non-whitespace
 * bytes after the first complete value.
 */
lonejson_status lonejson_visit_value_fd(int fd,
                                        const lonejson_value_visitor *visitor,
                                        void *user,
                                        const lonejson_value_limits *limits,
                                        lonejson_error *error);

/** Serializes a mapped struct to a generic output sink callback. */
lonejson_status lonejson_serialize_sink(const lonejson_map *map,
                                        const void *src, lonejson_sink_fn sink,
                                        void *user,
                                        const lonejson_write_options *options,
                                        lonejson_error *error);
/** Initializes a pull-style JSON generator for one mapped struct.
 *
 * The generator stays bounded and never materializes the whole payload. Drain
 * it incrementally through `lonejson_generator_read()` and release it with
 * `lonejson_generator_cleanup()`. It supports the normal mapped serializer
 * surface plus `lonejson_source`, `lonejson_spooled`, and `lonejson_json_value`
 * fields without silently buffering them into one giant payload. The init call
 * fully establishes the generator state; callers do not need to pre-zero the
 * struct.
 */
lonejson_status lonejson_generator_init(lonejson_generator *generator,
                                        const lonejson_map *map,
                                        const void *src,
                                        const lonejson_write_options *options);
/** Pulls the next serialized JSON bytes from a generator.
 *
 * `out_len` receives the produced byte count and `out_eof` becomes non-zero
 * once the generator has fully drained.
 */
lonejson_status lonejson_generator_read(lonejson_generator *generator,
                                        unsigned char *buffer, size_t capacity,
                                        size_t *out_len, int *out_eof);
/** Releases resources owned by a pull-style JSON generator. */
void lonejson_generator_cleanup(lonejson_generator *generator);
/** Serializes a mapped struct into a caller-provided output buffer. */
lonejson_status lonejson_serialize_buffer(const lonejson_map *map,
                                          const void *src, char *buffer,
                                          size_t capacity, size_t *needed,
                                          const lonejson_write_options *options,
                                          lonejson_error *error);
/** Serializes a mapped struct into a newly allocated JSON string using the
 * default allocator. Release the returned buffer with `free()` /
 * `LONEJSON_FREE()`. This convenience API rejects custom allocators; use
 * `lonejson_serialize_owned()` when you need allocator-aware ownership.
 */
char *lonejson_serialize_alloc(const lonejson_map *map, const void *src,
                               size_t *out_len,
                               const lonejson_write_options *options,
                               lonejson_error *error);
/** Serializes a mapped struct into an owned output buffer. Initialize `out`
 * with `lonejson_owned_buffer_init()` or `lonejson_default_owned_buffer()`,
 * then release it with `lonejson_owned_buffer_free()`.
 */
lonejson_status lonejson_serialize_owned(const lonejson_map *map,
                                         const void *src,
                                         lonejson_owned_buffer *out,
                                         const lonejson_write_options *options,
                                         lonejson_error *error);
/** Releases an owned output buffer produced by `*_owned()` serializers. */
void lonejson_owned_buffer_free(lonejson_owned_buffer *buffer);
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
 * allocated string using the default allocator. Release the returned buffer
 * with `free()` / `LONEJSON_FREE()`. This convenience API rejects custom
 * allocators; use `lonejson_serialize_jsonl_owned()` when you need
 * allocator-aware ownership.
 */
char *lonejson_serialize_jsonl_alloc(const lonejson_map *map, const void *items,
                                     size_t count, size_t stride,
                                     size_t *out_len,
                                     const lonejson_write_options *options,
                                     lonejson_error *error);
/** Serializes an array of mapped structs as JSONL records into an owned output
 * buffer. Release it with `lonejson_owned_buffer_free()`. Uses compact output
 * by default, or two-space pretty printing when `options->pretty` is
 * non-zero. `stride` defaults to `map->struct_size` when set to zero.
 */
lonejson_status lonejson_serialize_jsonl_owned(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    lonejson_owned_buffer *out, const lonejson_write_options *options,
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
/** Initializes a mapped value according to its field descriptors. Use this
 * instead of manual `memset` when you need a known reusable starting state,
 * when you plan to parse with `clear_destination = 0`, or when you are
 * configuring caller-owned fixed-capacity array backing storage before parse.
 */
void lonejson_init(const lonejson_map *map, void *value);
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
  /** Underlying pull-style JSON generator used by the curl adapter. */
  lonejson_generator generator;
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

/** Initializes a curl upload adapter for a mapped struct.
 *
 * This adapter is stream-first: it incrementally serializes through curl's
 * `CURLOPT_READFUNCTION` callback instead of materializing the whole JSON
 * payload up front. The reported size is currently always `-1`; lonejson does
 * not do a hidden prebuffer or pre-count pass just to compute it.
 */
lonejson_status
lonejson_curl_upload_init(lonejson_curl_upload *ctx, const lonejson_map *map,
                          const void *src,
                          const lonejson_write_options *options);
/** Curl read callback that streams serialized JSON bytes to libcurl. */
size_t lonejson_curl_read_callback(char *ptr, size_t size, size_t nmemb,
                                   void *userdata);
/** Returns the upload size reported by a curl upload adapter. The value is
 * `-1` when lonejson is streaming with an unknown total length.
 */
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
#define LJ_ABI_VERSION LONEJSON_ABI_VERSION
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
#define LJ_FIELD_KIND_JSON_VALUE LONEJSON_FIELD_KIND_JSON_VALUE
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

#define LJ_JSON_VALUE_NULL LONEJSON_JSON_VALUE_NULL
#define LJ_JSON_VALUE_BUFFER LONEJSON_JSON_VALUE_BUFFER
#define LJ_JSON_VALUE_READER LONEJSON_JSON_VALUE_READER
#define LJ_JSON_VALUE_FILE LONEJSON_JSON_VALUE_FILE
#define LJ_JSON_VALUE_FD LONEJSON_JSON_VALUE_FD
#define LJ_JSON_VALUE_PATH LONEJSON_JSON_VALUE_PATH

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
#define LJ_FIELD_JSON_VALUE LONEJSON_FIELD_JSON_VALUE
#define LJ_FIELD_JSON_VALUE_REQ LONEJSON_FIELD_JSON_VALUE_REQ
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

/** 32-bit unsigned integer type used by lonejson. */
typedef lonejson_uint32 lj_uint32;
/** 64-bit signed integer type used by lonejson. */
typedef lonejson_int64 lj_int64;
/** 64-bit unsigned integer type used by lonejson. */
typedef lonejson_uint64 lj_uint64;
/** Status code returned by lonejson operations. */
typedef lonejson_status lj_status;
/** Enumerates the supported mapped field kinds. */
typedef lonejson_field_kind lj_field_kind;
/** Enumerates the backing source kinds used by `lj_source`. */
typedef lonejson_source_kind lj_source_kind;
/** Enumerates the backing kinds supported by `lj_json_value`. */
typedef lonejson_json_value_kind lj_json_value_kind;
/** Selects how `lj_json_value` handles inbound parsed JSON. */
typedef lonejson_json_value_parse_mode lj_json_value_parse_mode;
/** Bounded limits applied while visiting one `lj_json_value`. */
typedef lonejson_value_limits lj_value_limits;
/** Visitor callbacks for one arbitrary `lj_json_value`, including balanced
 * structure events and chunked string/key/number delivery.
 */
typedef lonejson_value_visitor lj_value_visitor;
/** Describes whether mapped storage is dynamic or fixed-capacity. */
typedef lonejson_storage_kind lj_storage_kind;
/** Overflow handling policy for fixed-capacity string storage. */
typedef lonejson_overflow_policy lj_overflow_policy;
/** Detailed error information produced by lonejson APIs. */
typedef lonejson_error lj_error;
/** Optional debug allocation counters maintained by lonejson. */
typedef lonejson_allocator_stats lj_allocator_stats;
/** Allocator vtable used for lonejson-owned runtime state and buffers. */
typedef lonejson_allocator lj_allocator;
/** Configuration for spooled string and base64 storage. */
typedef lonejson_spool_options lj_spool_options;
/** Spooled byte container used for streamed string and base64 fields. */
typedef lonejson_spooled lj_spooled;
/** Source-backed string or base64 value that can serialize from file, fd, or
 * path. */
typedef lonejson_source lj_source;
/** Opaque arbitrary JSON value that can be memory-backed or source-backed. */
typedef lonejson_json_value lj_json_value;
/** Dynamic or fixed-capacity array of UTF-8 strings. */
typedef lonejson_string_array lj_string_array;
/** Dynamic or fixed-capacity array of 64-bit signed integers. */
typedef lonejson_i64_array lj_i64_array;
/** Dynamic or fixed-capacity array of 64-bit unsigned integers. */
typedef lonejson_u64_array lj_u64_array;
/** Dynamic or fixed-capacity array of double-precision numbers. */
typedef lonejson_f64_array lj_f64_array;
/** Dynamic or fixed-capacity array of boolean values. */
typedef lonejson_bool_array lj_bool_array;
/** Dynamic or fixed-capacity array of mapped object values. */
typedef lonejson_object_array lj_object_array;
/** Field descriptor used to map one JSON member to one struct member. */
typedef lonejson_field lj_field;
/** Mapping description that defines how a struct is parsed and serialized. */
typedef lonejson_map lj_map;
/** Incremental parser for object-framed JSON streams. */
typedef lonejson_stream lj_stream;
/** Options that customize parsing behavior. */
typedef lonejson_parse_options lj_parse_options;
/** Result of reading bytes from a caller-supplied reader callback. */
typedef lonejson_read_result lj_read_result;
/** Reader callback used to supply JSON bytes incrementally. */
typedef lonejson_reader_fn lj_reader_fn;
/** Options that customize serialization behavior. */
typedef lonejson_write_options lj_write_options;
/** Sink callback that receives serialized JSON bytes. */
typedef lonejson_sink_fn lj_sink_fn;
/** Result returned when advancing an object-framed JSON stream. */
typedef lonejson_stream_result lj_stream_result;

/** Returns the default configuration for spooled storage. */
LONEJSON_SHORT_ALIAS_INLINE lj_spool_options lj_default_spool_options(void) {
  return lonejson_default_spool_options();
}
/** Initializes an error struct to the empty `OK` state. */
LONEJSON_SHORT_ALIAS_INLINE void lj_error_init(lj_error *error) {
  lonejson_error_init(error);
}
/** Returns lonejson's default allocator backed by the configured allocation
 * macros. */
LONEJSON_SHORT_ALIAS_INLINE lj_allocator lj_default_allocator(void) {
  return lonejson_default_allocator();
}
/** Returns the empty default read result used by reader callbacks. */
LONEJSON_SHORT_ALIAS_INLINE lj_read_result lj_default_read_result(void) {
  return lonejson_default_read_result();
}
/** Returns the empty visitor with all callbacks set to `NULL`. */
LONEJSON_SHORT_ALIAS_INLINE lj_value_visitor lj_default_value_visitor(void) {
  return lonejson_default_value_visitor();
}
/** Initializes a spooled byte container. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_spooled_init(lj_spooled *value, const lj_spool_options *options) {
  lonejson_spooled_init(value, options);
}
/** Initializes a spooled byte container with an explicit allocator. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_spooled_init_with_allocator(lj_spooled *value,
                               const lj_spool_options *options,
                               const lj_allocator *allocator) {
  lonejson_spooled_init_with_allocator(value, options, allocator);
}
/** Clears a spooled value while keeping its reusable storage. */
LONEJSON_SHORT_ALIAS_INLINE void lj_spooled_reset(lj_spooled *value) {
  lonejson_spooled_reset(value);
}
/** Releases resources owned by a spooled value. */
LONEJSON_SHORT_ALIAS_INLINE void lj_spooled_cleanup(lj_spooled *value) {
  lonejson_spooled_cleanup(value);
}
/** Returns the number of bytes currently stored in a spooled value. */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_spooled_size(const lj_spooled *value) {
  return lonejson_spooled_size(value);
}
/** Reports whether a spooled value has spilled from memory to a temporary file.
 */
LONEJSON_SHORT_ALIAS_INLINE int lj_spooled_spilled(const lj_spooled *value) {
  return lonejson_spooled_spilled(value);
}
/** Rewinds a spooled value so it can be read again from the beginning. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_spooled_rewind(lj_spooled *value,
                                                        lj_error *error) {
  return lonejson_spooled_rewind(value, error);
}
/** Reads bytes from a spooled value into a caller-provided buffer. */
LONEJSON_SHORT_ALIAS_INLINE lj_read_result
lj_spooled_read(lj_spooled *value, unsigned char *buffer, size_t capacity) {
  return lonejson_spooled_read(value, buffer, capacity);
}
/** Streams the contents of a spooled value to an output sink. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_spooled_write_to_sink(
    const lj_spooled *value, lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_spooled_write_to_sink(value, sink, user, error);
}
/** Initializes a source-backed string or base64 handle. */
LONEJSON_SHORT_ALIAS_INLINE void lj_source_init(lj_source *value) {
  lonejson_source_init(value);
}
/** Resets a source handle to the empty state. */
LONEJSON_SHORT_ALIAS_INLINE void lj_source_reset(lj_source *value) {
  lonejson_source_reset(value);
}
/** Releases resources owned by a source handle. */
LONEJSON_SHORT_ALIAS_INLINE void lj_source_cleanup(lj_source *value) {
  lonejson_source_cleanup(value);
}
/** Configures a source handle to read from an open `FILE *`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_source_set_file(lj_source *value,
                                                         FILE *fp,
                                                         lj_error *error) {
  return lonejson_source_set_file(value, fp, error);
}
/** Configures a source handle to read from a file descriptor. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_source_set_fd(lj_source *value, int fd,
                                                       lj_error *error) {
  return lonejson_source_set_fd(value, fd, error);
}
/** Configures a source handle to read from a filesystem path. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_source_set_path(lj_source *value,
                                                         const char *path,
                                                         lj_error *error) {
  return lonejson_source_set_path(value, path, error);
}
/** Streams the contents referenced by a source handle to an output sink. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_source_write_to_sink(
    const lj_source *value, lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_source_write_to_sink(value, sink, user, error);
}
/** Initializes an arbitrary JSON value handle. */
LONEJSON_SHORT_ALIAS_INLINE void lj_json_value_init(lj_json_value *value) {
  lonejson_json_value_init(value);
}
/** Initializes an arbitrary JSON value handle with an explicit allocator. */
LONEJSON_SHORT_ALIAS_INLINE void
lj_json_value_init_with_allocator(lj_json_value *value,
                                  const lj_allocator *allocator) {
  lonejson_json_value_init_with_allocator(value, allocator);
}
/** Resets an arbitrary JSON value handle to JSON `null`. */
LONEJSON_SHORT_ALIAS_INLINE void lj_json_value_reset(lj_json_value *value) {
  lonejson_json_value_reset(value);
}
/** Releases resources owned by an arbitrary JSON value handle. */
LONEJSON_SHORT_ALIAS_INLINE void lj_json_value_cleanup(lj_json_value *value) {
  lonejson_json_value_cleanup(value);
}
/** Sets an arbitrary JSON value from a caller-provided memory buffer. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_buffer(
    lj_json_value *value, const void *data, size_t len, lj_error *error) {
  return lonejson_json_value_set_buffer(value, data, len, error);
}

/** Streams inbound parsed JSON bytes from an arbitrary JSON field to a caller
 * sink. Keep the handle configured across parse, typically by setting
 * `clear_destination = 0`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_parse_sink(
    lj_json_value *value, lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_json_value_set_parse_sink(value, sink, user, error);
}

/** Streams inbound parsed JSON from an arbitrary JSON field as structured
 * visitor callbacks with the provided limits. Pass `NULL` limits to use
 * `lj_default_value_limits()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_parse_visitor(
    lj_json_value *value, const lj_value_visitor *visitor, void *user,
    const lj_value_limits *limits, lj_error *error) {
  return lonejson_json_value_set_parse_visitor(value, visitor, user, limits,
                                               error);
}

/** Enables explicit parse-time capture for an arbitrary JSON field so decoded
 * bytes remain available in the handle after parsing.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_json_value_enable_parse_capture(lj_json_value *value, lj_error *error) {
  return lonejson_json_value_enable_parse_capture(value, error);
}
/** Sets an arbitrary JSON value from a caller-supplied reader callback. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_reader(
    lj_json_value *value, lj_reader_fn reader, void *user, lj_error *error) {
  return lonejson_json_value_set_reader(value, reader, user, error);
}
/** Sets an arbitrary JSON value from an open `FILE *`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_json_value_set_file(lj_json_value *value, FILE *fp, lj_error *error) {
  return lonejson_json_value_set_file(value, fp, error);
}
/** Sets an arbitrary JSON value from a file descriptor. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_fd(lj_json_value *value,
                                                           int fd,
                                                           lj_error *error) {
  return lonejson_json_value_set_fd(value, fd, error);
}
/** Sets an arbitrary JSON value from a filesystem path. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_set_path(
    lj_json_value *value, const char *path, lj_error *error) {
  return lonejson_json_value_set_path(value, path, error);
}
/** Validates and streams an arbitrary JSON value to an output sink. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_json_value_write_to_sink(
    const lj_json_value *value, lj_sink_fn sink, void *user, lj_error *error) {
  return lonejson_json_value_write_to_sink(value, sink, user, error);
}
/** Returns the default parsing options. */
LONEJSON_SHORT_ALIAS_INLINE lj_parse_options lj_default_parse_options(void) {
  return lonejson_default_parse_options();
}
/** Returns lonejson's default bounded limits for arbitrary JSON value
 * visitors: depth `64`, string `1 MiB`, key `64 KiB`, number `256` bytes,
 * unlimited total bytes.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_value_limits lj_default_value_limits(void) {
  return lonejson_default_value_limits();
}
/** Returns the default serialization options. */
LONEJSON_SHORT_ALIAS_INLINE lj_write_options lj_default_write_options(void) {
  return lonejson_default_write_options();
}
/** Owned serializer output buffer handle. */
typedef lonejson_owned_buffer lj_owned_buffer;
/** Returns an empty owned-buffer handle that uses lonejson's default
 * allocator.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_owned_buffer lj_default_owned_buffer(void) {
  return lonejson_default_owned_buffer();
}
/** Resets an owned-buffer handle to the empty default state before first use
 * or reuse.
 */
LONEJSON_SHORT_ALIAS_INLINE void lj_owned_buffer_init(lj_owned_buffer *buffer) {
  lonejson_owned_buffer_init(buffer);
}
/** Returns a human-readable string for a status code. */
LONEJSON_SHORT_ALIAS_INLINE const char *lj_status_string(lj_status status) {
  return lonejson_status_string(status);
}
/** Opens an object-framed JSON stream over a reader callback. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream *
lj_stream_open_reader(const lj_map *map, lj_reader_fn reader, void *user,
                      const lj_parse_options *options, lj_error *error) {
  return lonejson_stream_open_reader(map, reader, user, options, error);
}
/** Opens an object-framed JSON stream over an open `FILE *`. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream *
lj_stream_open_filep(const lj_map *map, FILE *fp,
                     const lj_parse_options *options, lj_error *error) {
  return lonejson_stream_open_filep(map, fp, options, error);
}
/** Opens an object-framed JSON stream over a filesystem path. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream *
lj_stream_open_path(const lj_map *map, const char *path,
                    const lj_parse_options *options, lj_error *error) {
  return lonejson_stream_open_path(map, path, options, error);
}
/** Opens an object-framed JSON stream over a file descriptor. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream *
lj_stream_open_fd(const lj_map *map, int fd, const lj_parse_options *options,
                  lj_error *error) {
  return lonejson_stream_open_fd(map, fd, options, error);
}
/** Parses the next top-level JSON object from a stream into `dst`. */
LONEJSON_SHORT_ALIAS_INLINE lj_stream_result lj_stream_next(lj_stream *stream,
                                                            void *dst,
                                                            lj_error *error) {
  return lonejson_stream_next(stream, dst, error);
}
/** Returns the last error recorded by a stream parser. */
LONEJSON_SHORT_ALIAS_INLINE const lj_error *
lj_stream_error(const lj_stream *stream) {
  return lonejson_stream_error(stream);
}
/** Closes a stream parser and releases its resources. */
LONEJSON_SHORT_ALIAS_INLINE void lj_stream_close(lj_stream *stream) {
  lonejson_stream_close(stream);
}
/** Parses a JSON buffer into a mapped struct. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_parse_buffer(const lj_map *map, void *dst, const void *data, size_t len,
                const lj_parse_options *options, lj_error *error) {
  return lonejson_parse_buffer(map, dst, data, len, options, error);
}
/** Parses a NUL-terminated JSON string into a mapped struct. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_parse_cstr(const lj_map *map, void *dst, const char *json,
              const lj_parse_options *options, lj_error *error) {
  return lonejson_parse_cstr(map, dst, json, options, error);
}
/** Parses JSON from a reader callback into a mapped struct. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_parse_reader(const lj_map *map, void *dst, lj_reader_fn reader, void *user,
                const lj_parse_options *options, lj_error *error) {
  return lonejson_parse_reader(map, dst, reader, user, options, error);
}
/** Parses JSON from an open `FILE *` into a mapped struct. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_parse_filep(const lj_map *map, void *dst, FILE *fp,
               const lj_parse_options *options, lj_error *error) {
  return lonejson_parse_filep(map, dst, fp, options, error);
}
/** Parses JSON from a filesystem path into a mapped struct. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_parse_path(const lj_map *map, void *dst, const char *path,
              const lj_parse_options *options, lj_error *error) {
  return lonejson_parse_path(map, dst, path, options, error);
}
/** Validates that a buffer contains syntactically valid JSON. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_buffer(const void *data,
                                                         size_t len,
                                                         lj_error *error) {
  return lonejson_validate_buffer(data, len, error);
}
/** Validates that a NUL-terminated string contains syntactically valid JSON. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_cstr(const char *json,
                                                       lj_error *error) {
  return lonejson_validate_cstr(json, error);
}
/** Validates JSON produced by a reader callback. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_reader(lj_reader_fn reader,
                                                         void *user,
                                                         lj_error *error) {
  return lonejson_validate_reader(reader, user, error);
}
/** Validates JSON read from an open `FILE *`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_filep(FILE *fp,
                                                        lj_error *error) {
  return lonejson_validate_filep(fp, error);
}
/** Validates JSON read from a filesystem path. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_validate_path(const char *path,
                                                       lj_error *error) {
  return lonejson_validate_path(path, error);
}
/** Visits exactly one JSON value from a caller-provided buffer and rejects
 * trailing non-whitespace bytes after that value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_value_buffer(
    const void *data, size_t len, const lj_value_visitor *visitor, void *user,
    const lj_value_limits *limits, lj_error *error) {
  return lonejson_visit_value_buffer(data, len, visitor, user, limits, error);
}
/** Visits exactly one JSON value from a NUL-terminated string and rejects
 * trailing non-whitespace bytes after that value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_value_cstr(
    const char *json, const lj_value_visitor *visitor, void *user,
    const lj_value_limits *limits, lj_error *error) {
  return lonejson_visit_value_cstr(json, visitor, user, limits, error);
}
/** Visits exactly one JSON value from a caller-provided reader callback and
 * rejects trailing non-whitespace bytes after that value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_value_reader(
    lj_reader_fn reader, void *reader_user, const lj_value_visitor *visitor,
    void *user, const lj_value_limits *limits, lj_error *error) {
  return lonejson_visit_value_reader(reader, reader_user, visitor, user, limits,
                                     error);
}
/** Visits exactly one JSON value from an open `FILE *` and rejects trailing
 * non-whitespace bytes after that value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_visit_value_filep(FILE *fp, const lj_value_visitor *visitor, void *user,
                     const lj_value_limits *limits, lj_error *error) {
  return lonejson_visit_value_filep(fp, visitor, user, limits, error);
}
/** Visits exactly one JSON value from a filesystem path and rejects trailing
 * non-whitespace bytes after that value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_visit_value_path(
    const char *path, const lj_value_visitor *visitor, void *user,
    const lj_value_limits *limits, lj_error *error) {
  return lonejson_visit_value_path(path, visitor, user, limits, error);
}
/** Visits exactly one JSON value from a file descriptor and rejects trailing
 * non-whitespace bytes after that value.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_visit_value_fd(int fd, const lj_value_visitor *visitor, void *user,
                  const lj_value_limits *limits, lj_error *error) {
  return lonejson_visit_value_fd(fd, visitor, user, limits, error);
}
/** Pull-style JSON generator state. */
typedef lonejson_generator lj_generator;
/** Serializes a mapped struct to a generic output sink callback. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_sink(
    const lj_map *map, const void *src, lj_sink_fn sink, void *user,
    const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_sink(map, src, sink, user, options, error);
}
/** Initializes a pull-style JSON generator for one mapped struct.
 *
 * The generator stays bounded and never materializes the whole payload. Drain
 * it incrementally through `lj_generator_read()` and release it with
 * `lj_generator_cleanup()`. It supports the normal mapped serializer surface
 * plus `lonejson_source`, `lonejson_spooled`, and `lonejson_json_value`
 * fields. `json_value` fields are streamed through the generator directly
 * instead of silently falling back to buffered serialization.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_generator_init(lj_generator *generator, const lj_map *map, const void *src,
                  const lj_write_options *options) {
  return lonejson_generator_init(generator, map, src, options);
}
/** Pulls the next serialized JSON bytes from a generator. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_generator_read(lj_generator *generator,
                                                        unsigned char *buffer,
                                                        size_t capacity,
                                                        size_t *out_len,
                                                        int *out_eof) {
  return lonejson_generator_read(generator, buffer, capacity, out_len, out_eof);
}
/** Releases resources owned by a pull-style JSON generator. */
LONEJSON_SHORT_ALIAS_INLINE void lj_generator_cleanup(lj_generator *generator) {
  lonejson_generator_cleanup(generator);
}
/** Serializes a mapped struct into a caller-provided output buffer. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_buffer(
    const lj_map *map, const void *src, char *buffer, size_t capacity,
    size_t *needed, const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_buffer(map, src, buffer, capacity, needed, options,
                                   error);
}
/** Serializes a mapped struct into a newly allocated JSON string using the
 * default allocator. Release the returned buffer with `free()` / `LJ_FREE()`.
 * This convenience API rejects custom allocators; use `lj_serialize_owned()`
 * when you need allocator-aware ownership.
 */
LONEJSON_SHORT_ALIAS_INLINE char *
lj_serialize_alloc(const lj_map *map, const void *src, size_t *out_len,
                   const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_alloc(map, src, out_len, options, error);
}
/** Serializes a mapped struct into an owned output buffer. Initialize `out`
 * with `lj_owned_buffer_init()` or `lj_default_owned_buffer()`, then release
 * it with `lj_owned_buffer_free()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_serialize_owned(const lj_map *map, const void *src, lj_owned_buffer *out,
                   const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_owned(map, src, out, options, error);
}
/** Releases an owned output buffer produced by `lj_serialize_owned()` or
 * `lj_serialize_jsonl_owned()`.
 */
LONEJSON_SHORT_ALIAS_INLINE void lj_owned_buffer_free(lj_owned_buffer *buffer) {
  lonejson_owned_buffer_free(buffer);
}
/** Serializes a mapped struct to an open `FILE *`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_serialize_filep(const lj_map *map, const void *src, FILE *fp,
                   const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_filep(map, src, fp, options, error);
}
/** Serializes a mapped struct to a filesystem path. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_serialize_path(const lj_map *map, const void *src, const char *path,
                  const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_path(map, src, path, options, error);
}
/** Serializes an array of mapped structs as JSONL records to an output sink. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_serialize_jsonl_sink(const lj_map *map, const void *items, size_t count,
                        size_t stride, lj_sink_fn sink, void *user,
                        const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_jsonl_sink(map, items, count, stride, sink, user,
                                       options, error);
}
/** Serializes an array of mapped structs as JSONL records into a buffer. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_jsonl_buffer(
    const lj_map *map, const void *items, size_t count, size_t stride,
    char *buffer, size_t capacity, size_t *needed,
    const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_jsonl_buffer(map, items, count, stride, buffer,
                                         capacity, needed, options, error);
}
/** Serializes an array of mapped structs as JSONL records into a newly
 * allocated string using the default allocator. Release the returned buffer
 * with `free()` / `LJ_FREE()`. This convenience API rejects custom
 * allocators; use `lj_serialize_jsonl_owned()` when you need allocator-aware
 * ownership.
 */
LONEJSON_SHORT_ALIAS_INLINE char *
lj_serialize_jsonl_alloc(const lj_map *map, const void *items, size_t count,
                         size_t stride, size_t *out_len,
                         const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_jsonl_alloc(map, items, count, stride, out_len,
                                        options, error);
}
/** Serializes an array of mapped structs as JSONL records into an owned output
 * buffer. Initialize `out` with `lj_owned_buffer_init()` or
 * `lj_default_owned_buffer()`, then release it with `lj_owned_buffer_free()`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_jsonl_owned(
    const lj_map *map, const void *items, size_t count, size_t stride,
    lj_owned_buffer *out, const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_jsonl_owned(map, items, count, stride, out, options,
                                        error);
}
/** Serializes an array of mapped structs as JSONL records to an open `FILE *`.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_jsonl_filep(
    const lj_map *map, const void *items, size_t count, size_t stride, FILE *fp,
    const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_jsonl_filep(map, items, count, stride, fp, options,
                                        error);
}
/** Serializes an array of mapped structs as JSONL records to a filesystem path.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_serialize_jsonl_path(
    const lj_map *map, const void *items, size_t count, size_t stride,
    const char *path, const lj_write_options *options, lj_error *error) {
  return lonejson_serialize_jsonl_path(map, items, count, stride, path, options,
                                       error);
}
/** Frees dynamic storage owned by a mapped value. */
LONEJSON_SHORT_ALIAS_INLINE void lj_cleanup(const lj_map *map, void *value) {
  lonejson_cleanup(map, value);
}
/** Initializes a mapped value according to its field descriptors. */
LONEJSON_SHORT_ALIAS_INLINE void lj_init(const lj_map *map, void *value) {
  lonejson_init(map, value);
}
/** Resets a mapped value while preserving caller-owned fixed backing storage.
 */
LONEJSON_SHORT_ALIAS_INLINE void lj_reset(const lj_map *map, void *value) {
  lonejson_reset(map, value);
}
#ifdef LONEJSON_WITH_CURL
/** Curl response parser state for feeding bytes into lonejson incrementally. */
typedef lonejson_curl_parse lj_curl_parse;
/** Curl upload state for serving serialized JSON bytes to libcurl. */
typedef lonejson_curl_upload lj_curl_upload;
/** Initializes a curl parse adapter for use with `CURLOPT_WRITEFUNCTION`. */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_curl_parse_init(lj_curl_parse *ctx, const lj_map *map, void *dst,
                   const lj_parse_options *options) {
  return lonejson_curl_parse_init(ctx, map, dst, options);
}
/** Curl write callback that forwards response bytes into a parse adapter. */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_curl_write_callback(char *ptr,
                                                          size_t size,
                                                          size_t nmemb,
                                                          void *userdata) {
  return lonejson_curl_write_callback(ptr, size, nmemb, userdata);
}
/** Finalizes parsing after curl has delivered the complete response body. */
LONEJSON_SHORT_ALIAS_INLINE lj_status lj_curl_parse_finish(lj_curl_parse *ctx) {
  return lonejson_curl_parse_finish(ctx);
}
/** Releases resources owned by a curl parse adapter. */
LONEJSON_SHORT_ALIAS_INLINE void lj_curl_parse_cleanup(lj_curl_parse *ctx) {
  lonejson_curl_parse_cleanup(ctx);
}
/** Initializes a curl upload adapter for a mapped struct.
 *
 * This adapter is stream-first: it incrementally serializes through curl's
 * `CURLOPT_READFUNCTION` callback instead of materializing the whole JSON
 * payload up front. The reported size is currently always `-1`; lonejson does
 * not do a hidden prebuffer or pre-count pass just to compute it.
 */
LONEJSON_SHORT_ALIAS_INLINE lj_status
lj_curl_upload_init(lj_curl_upload *ctx, const lj_map *map, const void *src,
                    const lj_write_options *options) {
  return lonejson_curl_upload_init(ctx, map, src, options);
}
/** Curl read callback that serves serialized JSON bytes from an upload adapter.
 */
LONEJSON_SHORT_ALIAS_INLINE size_t lj_curl_read_callback(char *ptr, size_t size,
                                                         size_t nmemb,
                                                         void *userdata) {
  return lonejson_curl_read_callback(ptr, size, nmemb, userdata);
}
/** Returns the payload size exposed by a curl upload adapter. The value is
 * `-1` when lonejson is streaming with an unknown total length.
 */
LONEJSON_SHORT_ALIAS_INLINE curl_off_t
lj_curl_upload_size(const lj_curl_upload *ctx) {
  return lonejson_curl_upload_size(ctx);
}
/** Releases resources owned by a curl upload adapter. */
LONEJSON_SHORT_ALIAS_INLINE void lj_curl_upload_cleanup(lj_curl_upload *ctx) {
  lonejson_curl_upload_cleanup(ctx);
}
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
