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
fields. Short aliases are enabled by default through `lj_*` and `LJ_*`. The
repository also ships a Lua binding with a schema DSL, reusable records,
object-framed streaming, and spool-backed fields on the Lua side.

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
`lonejson_stream_close`. Serialization is exposed through `lonejson_serialize_*`
and `lonejson_serialize_jsonl_*`. Ownership and reuse of mapped values is
handled by `lonejson_cleanup` and `lonejson_reset`.

Large-value handling is expressed explicitly. `lonejson_spooled` and
`lonejson_spool_options` are for inbound values that may spill to disk.
`lonejson_source` and the corresponding `lonejson_source_set_*` helpers are for
outbound values that should be serialized directly from an existing path,
`FILE *`, or file descriptor. Parse and write behavior is configured through
`lonejson_parse_options` and `lonejson_write_options`.

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

Then configure spill behavior at parse time:

```c
lonejson_parse_options options = lonejson_default_parse_options();
options.spool.memory_limit = 64u * 1024u;
options.spool.temp_dir = "/tmp";
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

## Pretty printing

Pretty printing is optional and uses two-space indentation:

```c
lonejson_write_options options = lonejson_default_write_options();
options.pretty = 1;
```

The same option applies to normal JSON serializers and to JSONL serializers,
where each record is formatted independently and then terminated by `\n`.

## Short aliases

`lonejson` provides optional short aliases. Functions and types are available as
`lj_*`; macros and constants are available as `LJ_*`. They are enabled by
default and can be disabled with:

```c
#define LONEJSON_DISABLE_SHORT_NAMES 1
```

## Lua binding

The repository also ships a Lua binding with schema-guided decoding, reusable
records, object-framed streams, and spool-backed fields.

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

- six binary release tarballs named `liblonejson-<version>-<target>.tar.gz`
  for the supported GNU and musl Linux targets
- a compressed standalone single-header artifact,
  `lonejson-<version>.h.gz`
- a versioned Lua rockspec
- a packed Lua source rock
- a SHA-256 manifest under `dist/`

The binary tarballs contain the declarations-only public header plus the
matching `liblonejson.a` and `liblonejson.so` artifacts. The compressed header
artifact is the standalone embedded form for single-header integration. The
version is taken from an exact `vX.Y.Z` tag on `HEAD`; if the current commit is
untagged, the release version falls back to `0.0.0`.

## Verification

The standard verification commands are:

```sh
make test
make bench
make lua-bench
```

## License

See [LICENSE](LICENSE).
