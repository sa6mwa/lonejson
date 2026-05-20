local lj = require("lonejson")

local function assert_eq(a, b, msg)
  if a ~= b then
    error((msg or "assert_eq failed") .. ": expected " .. tostring(b) .. ", got " .. tostring(a))
  end
end

local function assert_true(v, msg)
  if not v then
    error(msg or "assert_true failed")
  end
end

local function assert_rewrite_callback_failed(value, err, needle)
  assert_true(value == nil)
  assert_true(err ~= nil)
  assert_eq(err.status, "callback_failed")
  assert_true(err.message:find(needle, 1, true) ~= nil, err.message)
end

local function exists(path)
  local f

  if path == nil then
    return false
  end
  f = io.open(path, "rb")
  if f == nil then
    return false
  end
  f:close()
  return true
end

local Test = lj.schema("Test", {
  lj.field("name", lj.string { required = true }),
  lj.field("age", lj.i64()),
  lj.field("active", lj.bool()),
  lj.field("body", lj.spooled_text { memory_limit = 32, temp_dir = "/tmp" }),
  lj.field("payload", lj.spooled_bytes { memory_limit = 32, temp_dir = "/tmp" }),
  lj.field("meta", lj.object {
    fields = {
      lj.field("city", lj.string()),
      lj.field("zip", lj.i64()),
    },
  }),
  lj.field("nums", lj.i64_array { fixed_capacity = 8, overflow = "fail" }),
})

local UInts = lj.schema("UInts", {
  lj.field("value", lj.u64()),
  lj.field("values", lj.u64_array { fixed_capacity = 8, overflow = "fail" }),
  lj.field("tags", lj.string_array { fixed_capacity = 8, overflow = "fail" }),
})

local Query = lj.schema("Query", {
  lj.field("id", lj.string { required = true }),
  lj.field("selector", lj.json_value { required = true }),
  lj.field("fields", lj.json_value()),
  lj.field("last_error", lj.json_value()),
})

local Merge = lj.schema("Merge", {
  lj.field("id", lj.string()),
  lj.field("age", lj.i64()),
  lj.field("selector", lj.json_value()),
  lj.field("fields", lj.json_value()),
})

local Nullable = lj.schema("Nullable", {
  lj.field("required_count", lj.i64 { required = true }),
  lj.field("amount", lj.i64 { nullable = true }),
  lj.field("seed", lj.u64 { nullable = true }),
  lj.field("ratio", lj.f64 { nullable = true }),
  lj.field("enabled", lj.bool { nullable = true }),
  lj.field("child", lj.object {
    fields = {
      lj.field("priority", lj.i64 { nullable = true }),
    },
  }),
})

do
  local obj = Test:decode('{"name":"Alice","age":3,"active":true}')
  assert_eq(obj.name, "Alice")
  assert_eq(obj.age, 3)
  assert_eq(obj.active, true)
end

do
  local obj =
      Nullable:decode('{"required_count":7,"child":{"priority":null}}')
  assert_eq(obj.required_count, 7)
  assert_true(obj.amount == nil)
  assert_true(obj.seed == nil)
  assert_true(obj.ratio == nil)
  assert_true(obj.enabled == nil)
  assert_true(obj.child.priority == nil)

  obj = Nullable:decode(
      '{"required_count":8,"amount":-12,"seed":42,"ratio":1.25,"enabled":true,"child":{"priority":3}}')
  assert_eq(obj.amount, -12)
  assert_eq(obj.seed, 42)
  assert_eq(obj.ratio, 1.25)
  assert_eq(obj.enabled, true)
  assert_eq(obj.child.priority, 3)

  local rec = Nullable:new_record()
  local amount_get = Nullable:compile_get("amount")
  local child_get = Nullable:compile_get("child.priority")
  Nullable:decode_into(rec, '{"required_count":9,"amount":null,"child":{}}')
  assert_true(rec.amount == nil)
  assert_true(rec:get("amount") == nil)
  assert_true(amount_get(rec) == nil)
  assert_true(rec:get("child.priority") == nil)
  assert_true(child_get(rec) == nil)
  Nullable:decode_into(rec, '{"required_count":10,"amount":5,"child":{"priority":6}}')
  assert_eq(rec.amount, 5)
  assert_eq(amount_get(rec), 5)
  assert_eq(child_get(rec), 6)

  local json = Nullable:encode({
    required_count = 11,
    amount = lj.json_null,
    seed = nil,
    ratio = 2.5,
    enabled = false,
    child = { priority = lj.json_null },
  })
  assert_true(json:find('"required_count":11', 1, true) ~= nil)
  assert_true(json:find('"ratio":2.5', 1, true) ~= nil)
  assert_true(json:find('"enabled":false', 1, true) ~= nil)
  assert_true(json:find('"amount"', 1, true) == nil)
  assert_true(json:find('"seed"', 1, true) == nil)
  assert_true(json:find('"priority"', 1, true) == nil)

  for _, bad in ipairs({
    '{"required_count":1,"amount":"x"}',
    '{"required_count":1,"seed":-1}',
    '{"required_count":1,"ratio":true}',
    '{"required_count":1,"enabled":0}',
  }) do
    local value, err = Nullable:decode(bad)
    assert_eq(value, bad)
    assert_true(err ~= nil and err.status == "type_mismatch",
                "nullable primitive accepted invalid JSON: " .. bad)
  end

  local ok = pcall(function()
    lj.schema("BadRequiredNullable", {
      lj.field("x", lj.i64 { required = true, nullable = true }),
    })
  end)
  assert_true(not ok)
  ok = pcall(function()
    lj.schema("BadStringNullable", {
      lj.field("x", lj.string { nullable = true }),
    })
  end)
  assert_true(not ok)
  ok = pcall(function()
    lj.schema("BadArrayNullable", {
      lj.field("x", lj.i64_array { nullable = true }),
    })
  end)
  assert_true(not ok)
end

do
  assert_eq(lj.encode_json("a\nb"), [["a\nb"]])
  assert_eq(lj.encode_value("a\nb"), [["a\nb"]])
  assert_eq(lj.encode_json(lj.json_null), "null")
  assert_eq(lj.encode_json({ b = true, a = lj.json_array({ 1, lj.json_null }) }), '{"a":[1,null],"b":true}')
  do
    local chunks = {}
    lj.encode_json_to_sink({ z = "sink", a = lj.json_array({ true, false }) }, function(chunk)
      chunks[#chunks + 1] = chunk
    end)
    assert_eq(table.concat(chunks), '{"a":[true,false],"z":"sink"}')
  end
  do
    local ok, err = pcall(function()
      lj.encode_value_to_sink({ a = 1 }, function()
        error("sink boom")
      end)
    end)
    assert_true(not ok)
    assert_true(tostring(err):find("sink boom", 1, true) ~= nil)
  end
  assert_eq(lj.decode_json('"x\\ny"'), "x\ny")
  assert_eq(lj.decode_value('"x\\ny"'), "x\ny")
  assert_true(lj.decode_json("null") == lj.json_null)
  local decoded = lj.decode_json('{"a":[1,null],"b":false}')
  assert_eq(decoded.a[1], 1)
  assert_true(decoded.a[2] == lj.json_null)
  assert_eq(decoded.b, false)
  local ok = pcall(function()
    lj.decode_json("true false")
  end)
  assert_true(not ok)
end

do
  local rec = Test:new_record()
  local city_path = Test:compile_path("meta.city")
  local second_num_path = Test:compile_path("nums[2]")
  local city_get = Test:compile_get("meta.city")
  local nums_count = Test:compile_count("nums")
  local pretty_expected
  Test:decode_into(rec, '{"name":"Bob","age":5,"meta":{"city":"Uppsala","zip":44},"nums":[1,2,3]}')
  assert_eq(rec.name, "Bob")
  assert_eq(rec.age, 5)
  assert_eq(rec.meta.city, "Uppsala")
  assert_eq(rec:get("meta.city"), "Uppsala")
  assert_eq(rec:get(city_path), "Uppsala")
  assert_eq(city_path(rec), "Uppsala")
  assert_eq(city_get(rec), "Uppsala")
  assert_eq(rec:count("nums"), 3)
  assert_eq(city_path:get(rec), "Uppsala")
  assert_eq(Test:compile_path("nums"):count(rec), 3)
  assert_eq(nums_count(rec), 3)
  assert_eq(rec:get("nums[2]"), 2)
  assert_eq(rec:get(second_num_path), 2)
  assert_eq(second_num_path(rec), 2)
  assert_eq(rec.nums[2], 2)
  local json = Test:encode(rec)
  assert_true(json:find('"name":"Bob"', 1, true) ~= nil)
  pretty_expected = table.concat({
    "{",
    '  "name": "Bob",',
    '  "age": 5,',
    '  "active": false,',
    '  "body": "",',
    '  "payload": "",',
    '  "meta": {',
    '    "city": "Uppsala",',
    '    "zip": 44',
    "  },",
    '  "nums": [',
    "    1,",
    "    2,",
    "    3",
    "  ]",
    "}",
  }, "\n")
  assert_eq(Test:encode(rec, { pretty = true }), pretty_expected)
  assert_eq(Test:encode(rec, { max_output_bytes = 4096 }), json)
  assert_eq(Test:encode(rec, { max_output_bytes = #json }), json)
  local ok, err = pcall(function()
    Test:encode(rec, { max_output_bytes = #json - 1 })
  end)
  assert_true(not ok)
  assert_true(tostring(err):find("max_output_bytes", 1, true) ~= nil)
end

do
  local text = ("abc123-"):rep(24)
  local raw = string.char(0, 1, 2, 3, 4, 5, 6, 7) .. ("XYZ"):rep(20)
  local rec = Test:new_record()
  local body_path
  local payload_path
  Test:assign(rec, {
    name = "Spool",
    body = text,
    payload = raw,
  })
  assert_true(rec.body:spilled())
  assert_true(rec.payload:spilled())
  assert_eq(rec.body:read_all(), text)
  assert_eq(rec.payload:read_all(), raw)
  body_path = rec.body:path()
  payload_path = rec.payload:path()
  assert_true(exists(body_path))
  assert_true(exists(payload_path))
  rec:clear()
  assert_true(not exists(body_path))
  assert_true(not exists(payload_path))
  Test:assign(rec, {
    name = "Spool",
    body = text,
    payload = raw,
  })
  local roundtrip = Test:decode(Test:encode(rec))
  assert_eq(roundtrip.body:read_all(), text)
  assert_eq(roundtrip.payload:read_all(), raw)
end

do
  local path = "/tmp/lonejson-lua-test.json"
  local rec = Test:new_record()
  local f
  local pretty
  Test:assign(rec, {
    name = "Path",
    age = 7,
  })
  assert_true(Test:write_path(rec, path, { pretty = true }))
  f = assert(io.open(path, "rb"))
  pretty = assert(f:read("*a"))
  f:close()
  assert_true(pretty:find('\n  "name": "Path"', 1, true) ~= nil)
  local obj = Test:decode_path(path)
  assert_eq(obj.name, "Path")
  os.remove(path)
end

do
  local path = "/tmp/lonejson-lua-fd.json"
  local f
  local obj

  local rec = Test:new_record()
  Test:assign(rec, {
    name = "FD",
    age = 9,
  })

  f = assert(io.open(path, "wb"))
  assert_true(Test:write_fd(rec, f))
  f:close()

  f = assert(io.open(path, "rb"))
  obj = Test:decode_fd(f)
  assert_eq(obj.name, "FD")
  assert_eq(obj.age, 9)
  f:close()
  os.remove(path)
end

do
  local path = "/tmp/lonejson-lua-stream.jsonl"
  local f = assert(io.open(path, "wb"))
  f:write('{"name":"One","age":1}{"name":"Two","age":2}')
  f:close()

  local stream = Test:stream_path(path)
  local rec = Test:new_record()
  local obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.name, "One")
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_eq(obj.name, "Two")
  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
  os.remove(path)
end

do
  local max_u64 = "18446744073709551615"
  local above_i64 = "9223372036854775808"
  local obj = UInts:decode('{"value":18446744073709551615,"values":[1,9223372036854775808,18446744073709551615],"tags":["a","b"]}')
  assert_eq(type(obj.value), "string")
  assert_eq(obj.value, max_u64)
  assert_eq(type(obj.values[2]), "string")
  assert_eq(obj.values[2], above_i64)
  assert_eq(obj.values[3], max_u64)

  local rec = UInts:new_record()
  UInts:assign(rec, {
    value = max_u64,
    values = { 1, above_i64, max_u64 },
    tags = { [2] = "b", [1] = "a", extra = "ignored" },
  })
  assert_eq(rec.value, max_u64)
  assert_eq(rec.values[2], above_i64)
  assert_eq(UInts:encode(rec), '{"value":18446744073709551615,"values":[1,9223372036854775808,18446744073709551615],"tags":["a","b"]}')
end

do
  local obj = Query:decode('{"id":"q-42","selector":{"op":"and","clauses":[{"field":"status","value":"open"},{"field":"priority","gte":3}]},"fields":["id","title",{"metrics":[true,null,3.14]}],"last_error":null}')
  assert_eq(obj.id, "q-42")
  assert_eq(getmetatable(obj.selector).__lonejson_json_kind, "object")
  assert_eq(obj.selector.op, "and")
  assert_eq(getmetatable(obj.selector.clauses).__lonejson_json_kind, "array")
  assert_eq(obj.selector.clauses[2].gte, 3)
  assert_eq(getmetatable(obj.fields).__lonejson_json_kind, "array")
  assert_eq(obj.fields[3].metrics[1], true)
  assert_true(obj.fields[3].metrics[2] == lj.json_null)
  assert_true(obj.last_error == nil)
end

do
  local obj = Query:decode('{"id":"q-null","selector":{"a":null,"b":[1,null,2]},"fields":{"inner":[null,true]},"last_error":null}')
  local encoded

  assert_true(obj.selector.a == lj.json_null)
  assert_true(obj.selector.b[2] == lj.json_null)
  assert_true(obj.fields.inner[1] == lj.json_null)
  assert_true(obj.last_error == nil)

  encoded = Query:encode(obj)
  assert_true(encoded:find('"a":null', 1, true) ~= nil)
  assert_true(encoded:find('"b":[1,null,2]', 1, true) ~= nil)
  assert_true(encoded:find('"inner":[null,true]', 1, true) ~= nil)
end

do
  local path = "/tmp/lonejson-lua-query-stream.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local obj, err, status

  f:write('{"id":"stream-1","selector":{"kind":"term","field":"name","value":"alice"},"fields":["id",{"deep":[true,false,null,4.5]}],"last_error":{"code":"bad","retryable":false}}')
  f:close()

  stream = Query:stream_path(path)
  obj, err, status = stream:next()
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.selector.kind, "term")
  assert_eq(getmetatable(obj.fields).__lonejson_json_kind, "array")
  assert_eq(obj.fields[2].deep[1], true)
  assert_eq(obj.fields[2].deep[2], false)
  assert_true(obj.fields[2].deep[3] == lj.json_null)
  assert_eq(obj.last_error.code, "bad")
  assert_eq(obj.last_error.retryable, false)
  obj, err, status = stream:next()
  assert_eq(status, "eof")
  stream:close()
  os.remove(path)
end

do
  local rec = Query:new_record()
  Query:decode_into(rec, '{"id":"record-null","selector":{"x":[null,1],"y":{"z":null}}}')
  assert_true(rec.selector.x[1] == lj.json_null)
  assert_true(rec.selector.y.z == lj.json_null)
  assert_true(Query:encode(rec):find('"x":[null,1]', 1, true) ~= nil)
end

do
  local rec = Merge:new_record()

  Merge:assign(rec, {
    id = "old",
    age = 7,
    selector = lj.json_object { a = 1 },
    fields = lj.json_array { "keep" },
  })
  Merge:decode_into(rec, '{"selector":{"b":2}}', { clear_destination = false })

  assert_eq(rec.id, "old")
  assert_eq(rec.age, 7)
  assert_eq(getmetatable(rec.selector).__lonejson_json_kind, "object")
  assert_true(rec.selector.a == nil)
  assert_eq(rec.selector.b, 2)
  assert_eq(getmetatable(rec.fields).__lonejson_json_kind, "array")
  assert_eq(rec.fields[1], "keep")
end

do
  local path = "/tmp/lonejson-lua-merge-stream.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local rec = Merge:new_record()
  local obj, err, status

  f:write('{"id":"stream-old","age":9,"selector":{"a":1},"fields":["keep"]}')
  f:write('{"selector":{"b":2}}')
  f:close()

  stream = Merge:stream_path(path, { clear_destination = false })
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.id, "stream-old")
  assert_eq(obj.age, 9)
  assert_eq(obj.selector.a, 1)
  assert_eq(obj.fields[1], "keep")

  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.id, "stream-old")
  assert_eq(obj.age, 9)
  assert_true(obj.selector.a == nil)
  assert_eq(obj.selector.b, 2)
  assert_eq(obj.fields[1], "keep")

  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
  os.remove(path)
end

do
  local rec = Query:new_record()
  local encoded
  local roundtrip

  Query:assign(rec, {
    id = "assign",
    selector = lj.json_object {
      op = "or",
      clauses = lj.json_array {
        lj.json_object { field = "state", value = "ready" },
        lj.json_object { field = "retries", lt = 5 },
      },
    },
    fields = lj.json_array { "id", "title", lj.json_object { nested = lj.json_array { true, false, 1.25 } } },
    last_error = nil,
  })
  encoded = Query:encode(rec)
  assert_true(encoded:find('"selector":{"clauses":[', 1, true) ~= nil)
  assert_true(encoded:find('"last_error":null', 1, true) ~= nil)
  roundtrip = Query:decode(encoded)
  assert_eq(roundtrip.selector.clauses[1].field, "state")
  assert_eq(roundtrip.fields[3].nested[3], 1.25)
  assert_eq(getmetatable(roundtrip.fields).__lonejson_json_kind, "array")
end

do
  local stream =
    Test:array_stream_string("items", '{"meta":{"source":"skip"},"items":[{"name":"A","age":1},{"name":"B","age":2}]}')
  local obj, err, status

  obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "A")
  assert_eq(obj.age, 1)

  obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "B")
  assert_eq(obj.age, 2)

  obj, err, status = stream:next()
  assert_eq(status, "eof")
  assert_true(err == nil)
  stream:close()
end

do
  local stream =
    Query:array_stream_string("queries", '{"queries":[{"id":"q1","selector":{"x":null},"fields":["id",{"deep":[true,null]}]},{"id":"q2","selector":{"ok":true}}]}')
  local rec = Query:new_record()
  local obj, err, status

  obj, err, status = stream:next(rec)
  assert_eq(status, "item")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.id, "q1")
  assert_true(rec.selector.x == lj.json_null)
  assert_eq(rec.fields[2].deep[1], true)
  assert_true(rec.fields[2].deep[2] == lj.json_null)

  obj, err, status = stream:next(rec)
  assert_eq(status, "item")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.id, "q2")
  assert_eq(rec.selector.ok, true)

  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
end

do
  local stream =
    Merge:array_stream_string("items", '{"items":[{"id":"stream-old","age":9,"selector":{"a":1},"fields":["keep"]},{"selector":{"b":2}}]}', { clear_destination = false })
  local rec = Merge:new_record()
  local obj, err, status

  obj, err, status = stream:next(rec)
  assert_eq(status, "item")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.id, "stream-old")
  assert_eq(rec.age, 9)
  assert_eq(rec.selector.a, 1)
  assert_eq(rec.fields[1], "keep")

  obj, err, status = stream:next(rec)
  assert_eq(status, "item")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.id, "stream-old")
  assert_eq(rec.age, 9)
  assert_true(rec.selector.a == nil)
  assert_eq(rec.selector.b, 2)
  assert_eq(rec.fields[1], "keep")

  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
end

do
  local stream = Query:array_stream_string("", '[{"a":1},null,[true,null],"text"]')
  local value, err, status

  value, err, status = stream:next_value()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(value.a, 1)
  assert_eq(getmetatable(value).__lonejson_json_kind, "object")

  value, err, status = stream:next_value()
  assert_eq(status, "item")
  assert_true(value == nil)
  assert_true(err == nil)

  value, err, status = stream:next_value()
  assert_eq(status, "item")
  assert_eq(value[1], true)
  assert_true(value[2] == lj.json_null)
  assert_eq(getmetatable(value).__lonejson_json_kind, "array")

  value, err, status = stream:next_value()
  assert_eq(status, "item")
  assert_eq(value, "text")

  value, err, status = stream:next_value()
  assert_eq(status, "eof")
  stream:close()
end

do
  local path = "/tmp/lonejson-lua-array-stream.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local obj, err, status

  f:write('{"items":[{"name":"Path","age":11}]}')
  f:close()

  stream = Test:array_stream_path("items", path)
  obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "Path")
  obj, err, status = stream:next()
  assert_eq(status, "eof")
  stream:close()
  os.remove(path)
end

do
  local path = "/tmp/lonejson-lua-array-stream-fd.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local obj, err, status

  f:write('{"items":[{"name":"FD","age":12}]}')
  f:close()

  f = assert(io.open(path, "rb"))
  stream = Test:array_stream_fd("items", f)
  obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "FD")
  stream:close()
  f:close()
  os.remove(path)
end

do
  local path = "/tmp/lonejson-lua-array-stream-file.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local obj, err, status

  f:write('{"items":[{"name":"File","age":13}]}')
  f:close()

  f = assert(io.open(path, "rb"))
  stream = Test:array_stream_file("items", f)
  obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "File")
  stream:close()
  f:close()
  os.remove(path)
end

do
  local bad = Test:array_stream_string("boards.items", '{"boards":[{"items":[{"name":"x"}]}]}')
  assert_true(type(bad) == "table")
  assert_true(bad.status ~= nil)
end

do
  local stream = Test:array_stream_string("items", '{"meta":{"x":1,"x":2},"items":[{"name":"bad"}]}')
  local obj, err, status = stream:next()
  assert_eq(status, "error")
  assert_true(obj == nil)
  assert_true(err ~= nil)
  assert_eq(err.status, "duplicate_field")
  stream:close()
end

do
  local stream = Test:array_stream_string("items", '{"meta":{"x":1,"x":2},"items":[{"name":"ok","age":14}]}', { reject_duplicate_keys = false })
  local obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "ok")
  assert_eq(obj.age, 14)
  obj, err, status = stream:next()
  assert_eq(status, "eof")
  stream:close()
end

do
  local stream = Query:array_stream_string("", '[{"ok":true}x]')
  local value, err, status = stream:next_value()
  assert_eq(status, "error")
  assert_true(value == nil)
  assert_true(err ~= nil)
  stream:close()
end

do
  local input = '{"items":[{"id":"a","n":1},{"id":"b","n":2},{"id":"c","n":3}],"tail":true}'
  local seen = {}
  local output = lj.array_rewrite_string("items", input, {
    item = function(item)
      seen[#seen + 1] = item.id
      if item.id == "a" then
        return "keep"
      end
      if item.id == "b" then
        return { action = "replace", replacement = { id = "bb", n = 20 } }
      end
      return { action = "insert_after", insert = { id = "after-c", n = 4 } }
    end,
    append = function(ctx, emit)
      assert_eq(ctx.selector, "items")
      local ok, err = emit({ id = "appended", n = 5 })
      assert_true(ok, err and err.message)
    end,
  })
  assert_eq(table.concat(seen, ","), "a,b,c")
  assert_eq(output, '{"items":[{"id":"a","n":1},{"id":"bb","n":20},{"id":"c","n":3},{"id":"after-c","n":4},{"id":"appended","n":5}],"tail":true}')
end

do
  local Parent = lj.schema("RewriteParent", {
    lj.field("id", lj.string { required = true }),
  })
  local selected = {}
  local output = lj.array_rewrite_string("boards[].items", '{"boards":[{"id":"left","items":[{"v":1},{"v":2}]},{"id":"right","items":[{"v":3}]}]}', {
    parents = {
      { segment = "boards", schema = Parent },
    },
    item = function(item, ctx)
      selected[#selected + 1] = ctx.parents[1].value.id .. ":" .. tostring(item.v)
      if ctx.parents[1].value.id == "left" then
        return "drop"
      end
      return { action = "replace", value = { v = item.v * 10 } }
    end,
    append = function(ctx, emit)
      if ctx.parents[1].value.id == "right" then
        assert_true(emit({ v = 40 }))
      end
    end,
  })
  assert_eq(table.concat(selected, ","), "left:1,left:2,right:3")
  assert_eq(output, '{"boards":[{"id":"left","items":[]},{"id":"right","items":[{"v":30},{"v":40}]}]}')
end

do
  local input_path = "/tmp/lonejson-lua-array-rewrite-in.json"
  local output_path = "/tmp/lonejson-lua-array-rewrite-out.json"
  local f = assert(io.open(input_path, "wb"))
  f:write('{"items":[{"id":1},{"id":2}]}')
  f:close()
  local ok, err = lj.array_rewrite_path("items", input_path, output_path, {
    item = function(item)
      if item.id == 1 then
        return "drop"
      end
      return "keep"
    end,
  })
  assert_true(ok, err and err.message)
  f = assert(io.open(output_path, "rb"))
  assert_eq(f:read("*a"), '{"items":[{"id":2}]}')
  f:close()
  os.remove(input_path)
  os.remove(output_path)
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1,2]}', {
    item = function()
      error("forced item failure")
    end,
  })
  assert_true(out == nil)
  assert_true(err ~= nil)
  assert_eq(err.status, "callback_failed")
  assert_true(err.message:find("forced item failure", 1, true) ~= nil)
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1]}', {
    item = function()
      return "nonsense"
    end,
  })
  assert_true(out == nil)
  assert_true(err ~= nil)
  assert_eq(err.status, "callback_failed")
  assert_true(err.message:find("unsupported array rewrite action", 1, true) ~= nil)
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1]}', {
    item = function()
      return { action = "replace", value = function()
      end }
    end,
  })
  assert_rewrite_callback_failed(out, err, "unsupported Lua type for JSON value")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1]}', {
    item = function()
      return { action = "replace", value = lj.json_object({ [2] = "x" }) }
    end,
  })
  assert_rewrite_callback_failed(out, err, "JSON object keys must be strings")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1]}', {
    item = function()
      return { action = "insert_after", insert = lj.json_object({ [2] = "x" }) }
    end,
  })
  assert_rewrite_callback_failed(out, err, "JSON object keys must be strings")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1]}', {
    item = function()
      local cycle = {}
      cycle.self = cycle
      return { action = "insert_after", insert = cycle }
    end,
  })
  assert_rewrite_callback_failed(out, err, "cyclic Lua tables cannot be encoded as JSON")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1]}', {
    item = function()
      return { action = "replace_and_insert_after", replacement = { n = 0 / 0 }, insert = { ok = true } }
    end,
  })
  assert_rewrite_callback_failed(out, err, "JSON numbers must be finite")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[]}', {
    append = function(ctx, emit)
      local ok, emit_err = emit(lj.json_object({ [2] = "x" }))
      assert_true(ok == nil)
      assert_true(emit_err ~= nil)
      error(emit_err.message)
    end,
  })
  assert_rewrite_callback_failed(out, err, "JSON object keys must be strings")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[]}', {
    append = function(ctx, emit)
      local ok, emit_err = emit({ bad = function()
      end })
      assert_true(ok == nil)
      assert_true(emit_err ~= nil)
      error(emit_err.message)
    end,
  })
  assert_rewrite_callback_failed(out, err, "unsupported Lua type for JSON value")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1,]}', {
    item = function()
      return "keep"
    end,
  })
  assert_true(out == nil)
  assert_true(err ~= nil)
  assert_true(err.status ~= "ok")
end

do
  local ok = pcall(function()
    lj.array_rewrite_string("boards[].items", '{"boards":[{"items":[1]}]}', {
      parents = {
        { segment = "boards" },
      },
      item = function()
        return "keep"
      end,
    })
  end)
  assert_true(not ok)
end

print("lua tests passed")
