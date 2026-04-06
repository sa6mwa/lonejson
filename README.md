# lonejson

`lonejson` is a schema-guided JSON library for C. In normal source-tree use it
is built and linked as `liblonejson`, with a public header in
`include/lonejson.h`. The release process also produces a standalone
single-header artifact for projects that prefer to vendor one file directly.
The library maps JSON objects directly into C structs and back again, with a
particular emphasis on object-framed streams, predictable fixed-capacity
decoding, and pipelines that need to move large values through JSON without
holding everything in memory at once.

The project is built for programs that already know the shape of the data they
care about. Instead of constructing a generic DOM and walking it afterward, you
describe the fields you want, bind them to a `lonejson_map`, and parse or
serialize through that schema. That approach gives the library its character:
it is strongly typed, stream-oriented, and deliberate about ownership and
allocation.

Repository: <https://github.com/sa6mwa/lonejson>  
Examples: <https://github.com/sa6mwa/lonejson/tree/main/examples>

## Use-case

`lonejson` is not a DOM library, and it is not trying to be one. The normal
workflow is to define a C struct, describe its JSON-visible fields with
`LONEJSON_FIELD_*` macros, bind those fields into a `lonejson_map`, and then
use that map to parse, stream, validate, or serialize. If you want stable
decoding into known types, object-framed streaming that does not depend on line
delimiters, bounded-memory behavior, and explicit control over fixed versus
dynamic storage, that model works very well.

The library also covers two cases that often push JSON tooling into awkward
territory. Inbound values can spill to disk through spool-backed fields when a
string or base64 payload is too large to hold comfortably in memory, and
outbound values can be serialized directly from paths, `FILE *`, or file
descriptors when the data already exists outside the process heap.

## Feature set

At the core is the public header `include/lonejson.h`, paired with the compiled
library in the normal build. That API can parse from C strings, raw buffers,
reader callbacks, `FILE *`, filesystem paths, and object-framed streams built
on the same underlying parser. Serialization is available to sink callbacks,
caller-supplied buffers, allocated strings, `FILE *`, filesystem paths, and
JSON Lines. Pretty printing is optional and uses a fixed two-space indentation
style.

Field mappings can be fixed-capacity or dynamically allocated. Large inbound
values can be represented by spool-backed text or base64-decoded byte fields,
and large outbound values can be serialized from source-backed text or binary
fields. Arbitrary embedded JSON values can be represented by
`lonejson_json_value`, which lets one mapped field contain an opaque JSON
object, array, scalar, or `null` without turning it into a JSON string. The
same surface also exposes `lonejson_value_visitor` and `lonejson_visit_value_*`
for streaming one arbitrary JSON value as structured callbacks without
building a DOM. Short aliases are enabled by default through `lj_*` and
`LJ_*`. The repository also ships a Lua binding with a schema DSL, reusable
records, object-framed streaming, spool-backed fields, and native-Lua
`json_value` decoding built on the same visitor model.

When you need caller-owned fixed-capacity array backing storage during parse,
initialize the destination first with `lonejson_init(...)`, configure that
backing storage, and parse with `clear_destination = 0` so lonejson preserves
the configured arrays instead of rebuilding the destination from scratch.

## Integration

The repository now supports two integration styles.

The normal development and binary-distribution path is a conventional linked
library. Include the public header and link `liblonejson.a` or
`liblonejson.so`:

```c
#include "lonejson.h"
```

The standalone single-header release artifact remains available for projects
that prefer vendoring one file. In that mode, define
`LONEJSON_IMPLEMENTATION` in exactly one translation unit before including the
generated standalone header from the release archive:

```c
#define LONEJSON_IMPLEMENTATION
#include "lonejson.h"
```

The source-tree `include/lonejson.h` is the linked-library public header. The
generated single-header artifact is a separate release output, not the same
file.

curl integration remains optional at compile time. When you want the curl
adapter declarations, define `LONEJSON_WITH_CURL` before including
`lonejson.h` and compile against a build environment that provides curl
headers and libraries. `lonejson_curl_upload_init()` now sits on top of the
public pull-style generator API and feeds libcurl through
`CURLOPT_READFUNCTION` without materializing the whole JSON payload first.
That upload path currently reports `-1` for the total size because lonejson
does not prebuffer or pre-count the payload.

Short aliases are enabled by default. Disable them if they collide with another
project:

```c
#define LONEJSON_DISABLE_SHORT_NAMES 1
#include "lonejson.h"
```

## Public API overview

The API is arranged in a few clear families. Mapping is done with
`LONEJSON_FIELD_*` and `LONEJSON_MAP_DEFINE(...)`. One-shot parsing is handled
by `lonejson_parse_cstr`, `lonejson_parse_buffer`, `lonejson_parse_reader`,
`lonejson_parse_filep`, and `lonejson_parse_path`, with `lonejson_validate_*`
variants available when syntax validation is all you need. Streaming uses
`lonejson_stream_open_*`, `lonejson_stream_next`, `lonejson_stream_error`, and
`lonejson_stream_close`. Serialization is exposed through `lonejson_serialize_*`,
`lonejson_serialize_jsonl_*`, and the pull-style
`lonejson_generator_init/read/cleanup` trio. Ownership and reuse of mapped
values is handled by `lonejson_cleanup` and `lonejson_reset`.

Large-value handling is expressed explicitly. `lonejson_spooled` and
`lonejson_spool_options` are for inbound values that may spill to disk.
`lonejson_source` and the corresponding `lonejson_source_set_*` helpers are for
outbound values that should be serialized directly from an existing path,
`FILE *`, or file descriptor. `lonejson_json_value` covers the related case
where a typed envelope needs to embed one already-formed JSON value directly,
and `lonejson_visit_value_*` covers the lower-level case where one arbitrary
JSON value should be visited without a schema. Parse and write behavior is
configured through `lonejson_parse_options` and `lonejson_write_options`.

The new generator surface is the transport-facing serializer primitive. It is
bounded and pull-based rather than sink-driven, so adapters like curl upload do
not need worker threads or hidden full-payload buffers. It supports the normal
mapped serializer surface plus `lonejson_source`, `lonejson_spooled`, and
`lonejson_json_value` fields. `json_value` fields are streamed through the
generator directly instead of silently falling back to buffered serialization.

```c
lonejson_generator generator;
unsigned char chunk[4096];
size_t out_len;
int eof;

status = lonejson_generator_init(&generator, &event_map, &event, NULL);
if (status != LONEJSON_STATUS_OK) {
  fprintf(stderr, "generator init failed: %s\n", generator.error.message);
  return 1;
}

eof = 0;
while (!eof) {
  status = lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof);
  if (status != LONEJSON_STATUS_OK) {
    fprintf(stderr, "generator read failed: %s\n", generator.error.message);
    break;
  }
  fwrite(chunk, 1u, out_len, stdout);
}
lonejson_generator_cleanup(&generator);
```

## Quick start

### Parse one object into a struct

```c
#include "lonejson.h"

typedef struct user_doc {
  char *name;
  lonejson_int64 age;
} user_doc;

static const lonejson_field user_doc_fields[] = {
  LONEJSON_FIELD_STRING_ALLOC_REQ(user_doc, name, "name"),
  LONEJSON_FIELD_I64(user_doc, age, "age")
};

LONEJSON_MAP_DEFINE(user_doc_map, user_doc, user_doc_fields);

int main(void) {
  user_doc doc;
  lonejson_error error;
  lonejson_status status;

  status = lonejson_parse_cstr(
    &user_doc_map, &doc, "{\"name\":\"Ada\",\"age\":37}", NULL, &error);
  if (status != LONEJSON_STATUS_OK) {
    return 1;
  }

  lonejson_cleanup(&user_doc_map, &doc);
  return 0;
}
```

### Read an object-framed stream

`lonejson` stream parsing is object-framed rather than delimiter-framed. It
ignores whitespace between objects, so `{"a":1}{"a":2}`, pretty-printed
objects separated by blank lines, and JSONL-style one-object-per-line input all
fit the same model.

```c
lonejson_stream stream;
user_doc doc;

if (lonejson_stream_open_fd(&stream, &user_doc_map, fd, NULL) != LONEJSON_STATUS_OK) {
  return 1;
}

for (;;) {
  lonejson_stream_result result = lonejson_stream_next(&stream, &doc);
  if (result == LONEJSON_STREAM_OBJECT) {
    /* process doc */
    lonejson_cleanup(&user_doc_map, &doc);
    continue;
  }
  if (result == LONEJSON_STREAM_EOF) {
    break;
  }
  /* inspect lonejson_stream_error(&stream) */
  break;
}

lonejson_stream_close(&stream);
```

### Serialize one mapped object

```c
char buffer[256];
size_t written = 0u;
lonejson_error error;
lonejson_write_options options = lonejson_default_write_options();

options.pretty = 1;

if (lonejson_serialize_buffer(
      &user_doc_map, &doc, buffer, sizeof(buffer), &written,
      &options, &error) != LONEJSON_STATUS_OK) {
  return 1;
}
```

### Emit JSON Lines

```c
user_doc docs[2];
char out[256];
size_t written = 0u;

lonejson_serialize_jsonl_buffer(
  &user_doc_map, docs, 2u, 0u, out, sizeof(out), &written, NULL, NULL);
```

## Large field handling

### Inbound spool-backed fields

Use spool-backed fields when a JSON string or base64 payload may be too large to
retain in memory comfortably:

```c
typedef struct ingest_doc {
  lonejson_spooled body;
} ingest_doc;

static const lonejson_field ingest_doc_fields[] = {
  LONEJSON_FIELD_STRING_STREAM_REQ(ingest_doc, body, "body")
};

LONEJSON_MAP_DEFINE(ingest_doc_map, ingest_doc, ingest_doc_fields);
```

Then configure spill behavior on the field mapping:

```c
static const lonejson_spool_options ingest_spool = {
  64u * 1024u,
  0u,
  "/tmp"
};

static const lonejson_field ingest_doc_fields[] = {
  LONEJSON_FIELD_STRING_STREAM_OPTS(ingest_doc, body, "body", &ingest_spool)
};
```

The resulting `lonejson_spooled` value remains in memory when it is small and
spills to a temporary file when it grows past the configured threshold. The
same handle can then be serialized back into JSON or streamed back out as raw
text or raw bytes.

### Outbound source-backed fields

Use source-backed fields when the JSON field value already exists as a file,
file descriptor, or `FILE *`, and you want lonejson to serialize it directly
without creating a spool first.

Text source:

```c
typedef struct outbound_text_doc {
  lonejson_source body;
} outbound_text_doc;

static const lonejson_field outbound_text_fields[] = {
  LONEJSON_FIELD_STRING_SOURCE_REQ(outbound_text_doc, body, "body")
};
```

Binary source encoded as base64:

```c
typedef struct outbound_bytes_doc {
  lonejson_source payload;
} outbound_bytes_doc;

static const lonejson_field outbound_bytes_fields[] = {
  LONEJSON_FIELD_BASE64_SOURCE_REQ(outbound_bytes_doc, payload, "payload")
};
```

Attach the backing source:

```c
lonejson_source_set_path(&doc.payload, "payload.bin");
```

Ownership is explicit. lonejson only auto-closes sources it opened itself;
caller-owned `FILE *` and file descriptors remain caller-owned.

### Embedded arbitrary JSON values

Use `lonejson_json_value` when one mapped field should contain arbitrary JSON
rather than a JSON string:

```c
typedef struct query_doc {
  char namespace_[16];
  lonejson_json_value selector;
  lonejson_json_value fields;
} query_doc;

static const lonejson_field query_doc_fields[] = {
  LONEJSON_FIELD_STRING_FIXED_REQ(query_doc, namespace_, "namespace",
                                  LONEJSON_OVERFLOW_FAIL),
  LONEJSON_FIELD_JSON_VALUE_REQ(query_doc, selector, "selector"),
  LONEJSON_FIELD_JSON_VALUE(query_doc, fields, "fields")
};
```

You can populate the handle from memory, a reader callback, a `FILE *`, a file
descriptor, or a path. lonejson validates that the handle contains exactly one
JSON value when serializing.

On parse, `lonejson_json_value` is stream-first. Callers must opt into one of
three inbound behaviors before decoding:

- `lonejson_json_value_set_parse_sink(...)` streams compact JSON bytes to a
  caller sink as lonejson validates the nested value incrementally
- `lonejson_json_value_set_parse_visitor(...)` streams structured object,
  array, string, number, boolean, and null events to a caller visitor while
  applying `lonejson_value_limits`
- `lonejson_json_value_enable_parse_capture(...)` explicitly captures compact
  JSON bytes into owned storage for later reuse or re-emission

When using inbound parse sinks, parse visitors, or explicit capture, initialize
the destination first and parse with `clear_destination = 0` so the configured
`json_value` handles remain attached:

```c
lonejson_parse_options options = lonejson_default_parse_options();
options.clear_destination = 0;
```

Visitor mode is the zero-retention parse path for arbitrary embedded JSON:

```c
lonejson_value_visitor visitor = lonejson_default_value_visitor();
visitor.object_begin = on_object_begin;
visitor.object_key_begin = on_object_key_begin;
visitor.object_key_chunk = on_object_key_chunk;
visitor.object_key_end = on_object_key_end;
visitor.string_begin = on_string_begin;
visitor.string_chunk = on_string_chunk;
visitor.string_end = on_string_end;
visitor.number_begin = on_number_begin;
visitor.number_chunk = on_number_chunk;
visitor.number_end = on_number_end;
visitor.boolean_value = on_boolean;
visitor.null_value = on_null;

status = lonejson_json_value_set_parse_visitor(&doc.selector, &visitor, user,
                                               NULL, &error);
```

### Visiting Arbitrary JSON Values

Use `lonejson_visit_value_*` when you need to parse one arbitrary JSON value
without mapping it into a typed schema or building a DOM. The visitor API emits
structural events plus chunked decoded strings/object keys and bounded number
tokens.

```c
lonejson_value_visitor visitor = lonejson_default_value_visitor();
visitor.object_begin = on_object_begin;
visitor.object_key_begin = on_key_begin;
visitor.object_key_chunk = on_key_chunk;
visitor.object_key_end = on_key_end;
visitor.string_begin = on_string_begin;
visitor.string_chunk = on_string_chunk;
visitor.string_end = on_string_end;
visitor.number_begin = on_number_begin;
visitor.number_chunk = on_number_chunk;
visitor.number_end = on_number_end;
visitor.boolean_value = on_boolean;
visitor.null_value = on_null;

lonejson_value_limits limits = lonejson_default_value_limits();
limits.max_depth = 32;

status = lonejson_visit_value_cstr(json, &visitor, user, &limits, &error);
```

Default limits are intentionally conservative. Strings, object keys, and total
input size remain bounded; excessively deep or oversized input fails cleanly
instead of allocating without limit.

In the Lua binding, nested `json_value` nulls round-trip through the singleton
`lj.json_null` so they remain distinguishable inside arrays and objects. A
whole-field JSON `null` still maps to Lua `nil`.

For public structs, prefer lonejson's initializers and default helpers over
manual `memset` or `{0}`:

- `lonejson_init(map, &value)` for mapped structs
- `lonejson_json_value_init`, `lonejson_source_init`, `lonejson_spooled_init`
  for explicit handles
- `lonejson_json_value_init_with_allocator` and
  `lonejson_spooled_init_with_allocator` when an explicit allocator should own
  future internal allocations
- `lonejson_default_parse_options()`, `lonejson_default_write_options()`,
  `lonejson_default_value_visitor()`, `lonejson_default_read_result()`, and
  `lonejson_default_allocator()`
  for option/result structs

`lonejson_error` is output-only and does not need prior initialization, but
`lonejson_error_init()` is available when you want an explicit empty state.

## Pretty printing

Pretty printing is optional and uses two-space indentation:

```c
lonejson_write_options options = lonejson_default_write_options();
options.pretty = 1;
```

The same option applies to normal JSON serializers and to JSONL serializers,
where each record is formatted independently and then terminated by `\n`.

When you use the default alloc-returning serializer, release the returned
buffer with `LONEJSON_FREE()`:

```c
char *json = lonejson_serialize_alloc(&user_doc_map, &doc, NULL, NULL, &error);
if (json == NULL) {
  return 1;
}

puts(json);
LONEJSON_FREE(json);
```

Custom allocators can be attached per parse or write operation. For
alloc-returning serializers with a custom allocator, use the owned-buffer
variants:

```c
static void *my_malloc(void *ctx, size_t size) { return malloc(size); }
static void *my_realloc(void *ctx, void *ptr, size_t size) {
  return realloc(ptr, size);
}
static void my_free(void *ctx, void *ptr) { free(ptr); }

lonejson_allocator allocator = lonejson_default_allocator();
lonejson_parse_options parse_options = lonejson_default_parse_options();
lonejson_write_options write_options = lonejson_default_write_options();

allocator.malloc_fn = my_malloc;
allocator.realloc_fn = my_realloc;
allocator.free_fn = my_free;
parse_options.allocator = &allocator;
write_options.allocator = &allocator;

lonejson_owned_buffer owned = lonejson_default_owned_buffer();
if (lonejson_serialize_owned(&user_doc_map, &doc, &owned, &write_options,
                             &error) == LONEJSON_STATUS_OK) {
  puts(owned.data);
}
lonejson_owned_buffer_free(&owned);
```

## Benchmarks

The repository includes two benchmark harnesses:

- `make bench` runs the C benchmark suite and compares lonejson against the
  frozen baseline in `perflogs/baseline.json`
- `make lua-bench` runs the Lua benchmark suite and compares Lua lanes against
  `perflogs/lua/baseline.json`, with informational sibling ratios against the
  latest C lonejson run

The C benchmark harness is now self-contained. It no longer depends on YAJL or
another external C comparator library. When the benchmark schema, case set, or
measurement method changes, refresh the committed baseline with:

```sh
make bench-freeze-baseline
```

## Short aliases

`lonejson` provides optional short aliases. Functions and types are available as
`lj_*`; macros and constants are available as `LJ_*`. They are enabled by
default and can be disabled with:

```c
#define LONEJSON_DISABLE_SHORT_NAMES 1
```

## Lua binding

The repository also ships a Lua binding with schema-guided decoding, reusable
records, object-framed streams, spool-backed fields, and native-Lua arbitrary
`json_value` fields backed by the C visitor path.

Build and install it into the local LuaRocks tree:

```sh
make lua-rock
```

Run the integration test:

```sh
make lua-test
```

Run the Lua example:

```sh
eval "$(luarocks path --tree build/luarocks)" && lua examples/lua_binding.lua
```

## Example programs

See [examples/README.md](examples/README.md) for the full set. The example
programs cover one-shot parsing, object-framed streams, fixed-storage mappings,
spool-to-disk fields, source-backed outbound fields, string/file/JSONL
serialization, optional curl integration, and the Lua binding.

Online:

- <https://github.com/sa6mwa/lonejson/tree/main/examples>

## Release artifacts

Release packaging is Makefile-driven:

```sh
make release
```

That command produces:

- a source-only archive, `lonejson-<version>.tar.gz`
- a compressed standalone single-header artifact,
  `lonejson-<version>.h.gz`
- a versioned Lua rockspec and packed Lua source rock
- a SHA-256 manifest under `dist/`

The compressed header artifact is the standalone embedded form for
single-header integration. The source-only archive contains the repository
source tree without generated build output or local stash material. The Lua
source package prefers a curl-enabled native build when curl is available in
the build environment and falls back automatically to a curl-free build
otherwise. The release version comes from `VERSION` when present in the source
tree, otherwise from an exact `vX.Y.Z` tag on `HEAD`, and otherwise falls back
to `0.0.0`.

## Verification

The standard verification commands are:

```sh
make test
make test-all-bindings
make asan
make bench-gate
make lua-bench-gate
make fuzz
```

`make test-all` is the C-centric aggregate suite. Use
`make test-all-bindings` when you also want the optional Lua binding tests.

## License

See [LICENSE](LICENSE).
