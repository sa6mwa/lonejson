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

do
  local obj = Test:decode('{"name":"Alice","age":3,"active":true}')
  assert_eq(obj.name, "Alice")
  assert_eq(obj.age, 3)
  assert_eq(obj.active, true)
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

print("lua tests passed")
