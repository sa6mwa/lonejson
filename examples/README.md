# Examples

`make build` writes the standalone C example binaries to `examples/bin/`. That
is the intended repository entrypoint for the non-curl examples.

If you want to compile one manually from this directory, use:

```sh
mkdir -p bin
cc -I ../include -o bin/parse_string parse_string.c
```

The same pattern applies to:

* `parse_file.c`
* `parse_reader.c`
* `push_parser.c` (object-framed streaming reader example)
* `spooled_text.c` (forces spill-to-disk, shows temp path, and confirms cleanup)
* `spooled_bytes.c` (forces Base64 decode spill-to-disk and confirms cleanup)
* `source_text.c` (serializes a caller-declared JSON text field from a filesystem path)
* `source_bytes.c` (serializes a caller-owned fd as a Base64 JSON field)
* `serialize_string.c`
* `serialize_file.c`
* `serialize_jsonl.c`
* `fixed_storage.c`

The curl examples also define `LONEJSON_WITH_CURL` and need curl headers/libs. With a host-native `liblockdc` dev bundle downloaded via `make deps-host`, a typical command is:

```sh
cc -I ../include \
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
