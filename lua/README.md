# lonejson Lua binding

The Lua binding exposes `lonejson` as a schema-guided JSON decoder and
serializer rather than as a generic `decode(anything)` convenience layer. That
choice is deliberate. The C library is built around mapped object shapes,
reusable records, object-framed streams, and explicit handling of large values;
the Lua surface keeps those strengths instead of hiding them behind a looser
API that would allocate more and explain less.

At the top level, the binding is loaded with:

```lua
local lj = require("lonejson")
```

The main repository README explains the overall project. This document focuses
on the Lua-facing API and on the practical question of how to use it well from
Lua.

## What the Lua binding is for

The binding supports two broad styles of use.

The first style is table-oriented and convenient. You define a schema once,
decode JSON into ordinary Lua tables, and encode ordinary Lua tables back to
JSON. That is the right choice when simplicity matters more than squeezing the
last allocation out of the pipeline.

The second style is record-oriented and intentionally low-allocation. In that
mode, a schema produces reusable record objects backed by the C mapping layer.
Those records can be cleared and reused, passed through object-framed streams,
and combined with spool-backed fields for large text or byte payloads. This is
the intended path for streaming ingestion, ETL, and services that want the
clarity of Lua without giving up the bounded-memory behavior of the C core.

## Building and testing from this repository

The repository Makefile installs the Lua module into a local LuaRocks tree under
`build/luarocks`:

```sh
make lua-rock
```

Run the Lua integration tests with:

```sh
make lua-test
```

Run the Lua benchmark harness with:

```sh
make lua-bench
```

To use the locally built rock in a shell session:

```sh
eval "$(luarocks path --tree build/luarocks)"
```

The example program in [`examples/lua_binding.lua`](../examples/lua_binding.lua)
is a good companion to this document.

When you also use the C API around the Lua binding, follow the same
initialization rule as the rest of lonejson: use the public `*_init` and
`lonejson_default_*` helpers for public structs instead of manual `memset`
or `{0}`.

## The schema DSL

The binding starts with a schema description. A schema is a name plus a list of
field declarations. Each field declaration gives the JSON key, the expected
type, and any relevant options such as `required`, `fixed_capacity`,
`overflow`, or spool settings.

```lua
local lj = require("lonejson")

local Address = lj.object {
  fields = {
    lj.field("city", lj.string { required = true }),
    lj.field("zip", lj.i64()),
  },
}

local User = lj.schema("User", {
  lj.field("name", lj.string { required = true }),
  lj.field("age", lj.i64()),
  lj.field("active", lj.bool()),
  lj.field("address", lj.object {
    fields = Address.fields,
  }),
  lj.field("tags", lj.string_array { overflow = "fail" }),
})
```

The field constructors mirror the main families in the C library:

- `lj.string`
- `lj.i64`
- `lj.u64`
- `lj.f64`
- `lj.bool` / `lj.boolean`
- `lj.object`
- `lj.json_value`
- `lj.string_array`
- `lj.i64_array`
- `lj.u64_array`
- `lj.f64_array`
- `lj.boolean_array`
- `lj.object_array`
- `lj.spooled_text`
- `lj.spooled_bytes`

`lj.json_value` maps one field to arbitrary nested JSON. In Lua it decodes to
native Lua values: objects become tables tagged as objects, arrays become
tables tagged as arrays, strings become Lua strings, numbers become Lua
numbers, booleans become booleans, and nested JSON `null` values become the
singleton `lj.json_null` so they survive round-trips inside arrays and objects.
When the entire `json_value` field is JSON `null`, the field itself is exposed
as Lua `nil`.

In table-returning decode paths, `json_value` is built directly from lonejson's
streamed parse visitor callbacks into normal Lua values; it does not first
materialize the whole nested value as retained raw JSON bytes on the C side.
Reusable record mode still keeps an explicit retained C representation so the
record can be accessed repeatedly after decode.

Use `lj.json_object(tbl)` and `lj.json_array(tbl)` when you want to preserve
object-vs-array intent while assigning arbitrary JSON values from Lua tables,
especially for empty tables.

```lua
local Query = lj.schema("Query", {
  lj.field("namespace", lj.string { required = true }),
  lj.field("selector", lj.json_value { required = true }),
  lj.field("fields", lj.json_value()),
})

local q = Query:decode([[{
  "namespace": "ops",
  "selector": {"op":"and","clauses":[{"field":"status","value":"open"}]},
  "fields": ["id", "title", {"metrics":["latency", true, 3.14]}]
}]])

assert(q.selector.op == "and")
assert(q.fields[1] == "id")
assert(q.fields[3].metrics[2] == lj.json_null)
```

The shape is intentionally explicit. A schema is not an afterthought; it is the
contract that makes the rest of the API efficient and predictable.

## Table mode

Table mode is the most direct way to use the binding. Decode returns a Lua
table; encode accepts a Lua table.

```lua
local obj = User:decode([[{
  "name": "Ada",
  "age": 37,
  "active": true
}]])

print(obj.name)

local json = User:encode({
  name = "Ada",
  age = 37,
  active = true,
}, { pretty = true })
```

Convenience entry points exist for the common sources:

- `schema:decode(json_string)`
- `schema:decode_path(path)`
- `schema:decode_fd(file_handle)`
- `schema:write_path(value, path [, options])`
- `schema:write_fd(value, file_handle [, options])`

Pretty printing is optional and matches the C library: two-space indentation
with stable object and array formatting.

## Reusable records

Reusable records are the most important Lua-specific feature for
performance-sensitive code. A record is allocated once by the schema and then
reused across many decode operations.

```lua
local rec = User:new_record()

User:decode_into(rec, [[{"name":"Bob","age":41}]])
print(rec.name, rec.age)

rec:clear()
User:assign(rec, {
  name = "Carol",
  age = 29,
})
```

Records are useful because they preserve the low-allocation structure of the C
library. If the data shape is stable and you are processing many objects of the
same kind, `decode_into` is usually the right entry point.

## Object-framed streams

The stream API is object-framed, not delimiter-framed. Newlines have no special
meaning beyond ordinary whitespace. A stream can therefore read:

- `{"a":1}{"a":2}`
- one-object-per-line JSONL
- pretty-printed objects separated by blank lines

The Lua API mirrors the C design:

```lua
local stream = User:stream_path("/tmp/users.jsonl")
local rec = User:new_record()

while true do
  local obj, err, status = stream:next(rec)
  if status == "object" then
    print(rec.name)
  elseif status == "eof" then
    break
  else
    error(err and err.message or "stream failed")
  end
end

stream:close()
```

When a record is supplied to `stream:next(rec)`, the same record is reused for
each object. That is the intended fast path for continuous streams.

## Path compilation and accessors

The binding also exposes compiled accessors for hot paths where repeated string
lookups would be wasteful.

```lua
local city = User:compile_get("address.city")
local tag_count = User:compile_count("tags")

local rec = User:new_record()
User:decode_into(rec, [[{
  "name":"Dora",
  "address":{"city":"Uppsala"},
  "tags":["ops","oncall"]
}]])

print(city(rec))
print(tag_count(rec))
```

There is also a lower-level `compile_path(...)` form, but in normal code the
compiled getter/count closures are the more direct interface.

## Spool-backed fields

Large values are represented in Lua through spool-backed objects rather than as
unbounded strings.

```lua
local Blob = lj.schema("Blob", {
  lj.field("name", lj.string { required = true }),
  lj.field("body", lj.spooled_text {
    memory_limit = 96,
    temp_dir = "/tmp",
  }),
  lj.field("payload", lj.spooled_bytes {
    memory_limit = 96,
    temp_dir = "/tmp",
  }),
})
```

Assigning or decoding into those fields produces spool handles with methods such
as:

- `spool:size()`
- `spool:spilled()`
- `spool:path()`
- `spool:rewind()`
- `spool:read(n)`
- `spool:read_all()`

The helper `lj.chunks(spool, chunk_size)` provides a small iterator wrapper for
chunked reads:

```lua
for chunk in lj.chunks(rec.payload, 16384) do
  sink(chunk)
end
```

This is the right model for large text or byte payloads that should remain
bounded in memory while still fitting into a JSON pipeline.

## Error handling

The binding uses normal Lua return conventions rather than making every
operation throw by default. Successful operations typically return the decoded
value or `true`. Failures generally return `nil` or `false` plus an error table
that carries the message from the underlying C layer.

For stream operations, `stream:next(...)` returns three values:

- object or `nil`
- error table or `nil`
- status string

The status is one of:

- `"object"`
- `"eof"`
- `"would_block"`
- `"error"`

Code should branch on the status instead of guessing from the object value
alone.

## Examples and related files

Useful files in this repository:

- [`examples/lua_binding.lua`](../examples/lua_binding.lua)
- [`tests/test_lua.lua`](../tests/test_lua.lua)
- [`perflogs/lua/README.md`](../perflogs/lua/README.md)
- [`README.md`](../README.md)

Those files together cover the binding from three angles: example usage,
behavioral tests, and performance measurement.
