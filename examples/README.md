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

* `parse_file.c`
* `parse_reader.c`
* `push_parser.c` (object-framed streaming reader example)
* `json_value_buffer.c` (embeds selector/fields JSON directly from memory)
* `json_value_parse.c` (enables explicit parse capture for opaque embedded JSON
  values and re-emits them)
* `json_value_visitor.c` (streams parsed embedded JSON through visitor
  callbacks without retaining raw bytes)
* `json_value_source.c` (streams embedded JSON values from filesystem paths)
* `spooled_text.c` (forces spill-to-disk, shows temp path, and confirms cleanup)
* `spooled_bytes.c` (forces Base64 decode spill-to-disk and confirms cleanup)
* `source_text.c` (serializes a caller-declared JSON text field from a filesystem path)
* `source_bytes.c` (serializes a caller-owned fd as a Base64 JSON field)
* `serialize_string.c`
* `serialize_file.c`
* `fixed_storage.c`

`parse_string.c` and `serialize_jsonl.c` are the embedded single-header
variants in the tree; the rest model linked-library use.

The linked-library examples intentionally use lonejson's public initializers and
default helpers instead of manual `memset` or `{0}` for public structs:
`lonejson_init` / `lj_init` for mapped values, `*_init` for handles, and
`lonejson_default_*` / `lj_default_*` for option, visitor, and read-result
structs.

The curl examples also define `LONEJSON_WITH_CURL` and need curl headers/libs. With a host-native `liblockdc` dev bundle downloaded via `make deps-host`, a typical command is:

```sh
cc -I ../include ../src/lonejson.c \
   -I ../.deps/liblockdc/x86_64-linux-gnu/root/include \
   -o bin/curl_get curl_get.c \
   -L ../.deps/liblockdc/x86_64-linux-gnu/root/lib \
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

Then run the Lua example:

```sh
eval "$(luarocks path --tree build/luarocks)" && lua examples/lua_binding.lua
```

That example shows the schema DSL, reusable record decoding, object-framed stream parsing, and spool-backed text/byte fields.
