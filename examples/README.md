# Examples

`make build` writes the standalone C example binaries to `examples/bin/`. That
is the intended repository entrypoint for the non-curl examples.

`parse_string.c` and `serialize_jsonl.c` are intentional single-header
examples. They define `LONEJSON_IMPLEMENTATION` and should be compiled against a
generated standalone header:

```sh
mkdir -p bin
cc -I ../build/debug/generated/single-header/include \
  -o bin/parse_string parse_string.c
```

The remaining standalone examples are linked-library examples. After `make
build`, compile them against the built archive and the normal public header:

```sh
mkdir -p bin
cc -I ../include -o bin/parse_reader parse_reader.c \
  ../build/debug/liblonejson.a
cc -I ../include -o bin/serialize_file serialize_file.c \
  ../build/debug/liblonejson.a
```

The same pattern applies to:

* `array_stream.c` (streams the `items` array from
  `tests/fixtures/array_stream/issues.json` item by item)
* `parse_file.c`
* `parse_reader.c`
* `push_parser.c` (object-framed streaming reader example)
* `generator_pull.c` (pull-style generator example for transport adapters)
* `json_value_buffer.c` (embeds selector/fields JSON directly from memory)
* `json_value_nested_parse.c` (configures a nested mapped `json_value` parse
  sink once, then reuses the same destination across multiple parses)
* `json_value_parse.c` (enables explicit parse capture for opaque embedded JSON
  values and re-emits them)
* `json_value_visitor.c` (streams parsed embedded JSON through visitor
  callbacks without retaining raw bytes)
* `json_value_source.c` (streams embedded JSON values from filesystem paths)
* `nullable_fields.c` (maps optional nullable primitive fields with explicit
  presence bits and omits absent/null values on serialization)
* `spooled_text.c` (forces spill-to-disk, shows temp path, and confirms cleanup)
* `spooled_bytes.c` (forces Base64 decode spill-to-disk and confirms cleanup)
* `source_text.c` (serializes a caller-declared JSON text field from a filesystem path)
* `source_bytes.c` (serializes a caller-owned fd as a Base64 JSON field)
* `value_rewrite_replace_with.c` (rewrites one selected JSON value by
  inspecting the old value and emitting a replacement through the writer)
* `serialize_string.c`
* `serialize_file.c`
* `fixed_storage.c`

`parse_string.c` and `serialize_jsonl.c` are the embedded single-header
variants in the tree; the rest model linked-library use.

The public API also includes streaming selected-array rewrites
(`lonejson_array_rewrite_*`), whole-value rewrites
(`lonejson_value_rewrite_*`), incremental Server-Sent Events parsing
(`lonejson_sse_*`), and incremental MIME multipart parsing
(`lonejson_multipart_*`). Those surfaces are covered by the integration tests
under `tests/` even when there is no dedicated standalone example program yet.

The linked-library examples intentionally use lonejson's public initializers and
default helpers instead of manual `memset` or `{0}` for public structs:
`lonejson_init` / `lj_init` for mapped values, `*_init` for handles, and
`lonejson_default_*` / `lj_default_*` for option, visitor, and read-result
structs.

The curl examples also define `LONEJSON_WITH_CURL` and need curl headers/libs. With a host-native `c.pkt.systems` dependency bundle downloaded via `make deps-host`, a typical command is:

```sh
cc -I ../include ../src/lonejson.c \
   -I ../.deps/c.pkt.systems/x86_64-linux-gnu/root/include \
   -o bin/curl_get curl_get.c \
   -L ../.deps/c.pkt.systems/x86_64-linux-gnu/root/lib \
   -lcurl -lssl -lcrypto -lnghttp2 -lz
```

The repository helper also writes the curl binaries to `examples/bin/`:

```sh
make curl-examples
```

Start the local test rig first:

```sh
make compose-up
```

## Lua binding

Build and install the Lua module into the local LuaRocks tree:

```sh
make lua-rock
```

Then run the main Lua example:

```sh
eval "$(luarocks path --tree build/luarocks)" && lua examples/lua_binding.lua
```

That example shows the schema DSL, reusable record decoding, object-framed
stream parsing, selected-array stream parsing, selected-array rewrites, native
`json_value` handling, and spool-backed text/byte fields.

Run the smaller nullable primitive Lua example with:

```sh
eval "$(luarocks path --tree build/luarocks)" && lua examples/lua_nullable.lua
```

It shows `nullable = true` on optional `u64`, `f64`, and `bool` fields, where
missing or JSON `null` decode to Lua `nil` and `nil`/`lj.json_null` values are
omitted when encoding.

Run the nested `json_value` Lua example with:

```sh
eval "$(luarocks path --tree build/luarocks)" && lua examples/lua_json_value_nested.lua
```

It shows a nested object `json_value` field decoding into Lua values and being
reused with `clear_destination = false`.

Run the object-array `json_value` Lua example with:

```sh
eval "$(luarocks path --tree build/luarocks)" && lua examples/lua_json_value_object_array.lua
```

It shows `json_value` fields inside object-array element schemas. With
`clear_destination = false`, omitted arrays are preserved and present arrays are
replaced, which matches the more idiomatic Lua expectation for record reuse.
