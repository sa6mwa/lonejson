-- Run with scripts/run_lua_benchmark.sh REPO BUILD_DIR this-file
-- [--growth | --case NAME].
-- Uses the production module, seven 250 ms samples, and median ns/document.
local lonejson = require("lonejson")
local core = lonejson.core
local compact = lonejson.new()
local pretty = lonejson.new({write_pretty = true})
local cases = {
  {"core/scalar", core.encode_json, "short string"},
  {"facade/scalar", lonejson.encode_value, "short string"},
  {"compact/plain254", compact.encode_json, string.rep("x", 254)},
  {"compact/escaped", compact.encode_json, string.rep('x"\\\n', 64)},
  {"compact/sparse-escaped", compact.encode_json, string.rep("x", 254) .. '\n'},
  {"compact/object", compact.encode_json, {alpha = 1, beta = "text", gamma = true}},
  {"compact/nested", compact.encode_json,
    {alpha = {beta = {1, 2, 3}, gamma = "text"}, delta = false}},
  {"pretty/plain254", pretty.encode_json, string.rep("x", 254)},
  {"pretty/escaped", pretty.encode_json, string.rep('x"\\\n', 64)},
  {"pretty/sparse-escaped", pretty.encode_json, string.rep("x", 254) .. '\n'},
  {"pretty/object", pretty.encode_json, {alpha = 1, beta = "text", gamma = true}},
  {"pretty/nested", pretty.encode_json,
    {alpha = {beta = {1, 2, 3}, gamma = "text"}, delta = false}},
}

local fields = {
  lonejson.field("name", lonejson.string({fixed_capacity = 32})),
  lonejson.field("id", lonejson.i64()),
}
for _, runtime in ipairs({compact, pretty}) do
  local schema = runtime.schema("EncodeBench", fields)
  local record = schema:new_record()
  schema:assign(record, {name = "Synthetic opportunity", id = 42})
  cases[#cases + 1] = {
    runtime == compact and "schema/compact" or "schema/pretty",
    function(value) return schema:encode(value) end, record,
  }
end

if arg[1] == "--growth" then
  cases[#cases + 1] = {"growth/plain1024", compact.encode_json, string.rep("x", 1024)}
  for _, count in ipairs({1, 600}) do
    local records = {}
    for i = 1, count do
      records[i] = {
        id = i, name = "Synthetic opportunity " .. i,
        stage_id = (i % 5) + 1, owner_id = (i % 3) + 1,
        expected_revenue = i * 1000, contact_ids = {i + 1000},
        description = string.rep("Synthetic context. ", 12),
      }
    end
    cases[#cases + 1] = {"growth/pipeline" .. count, compact.encode_json,
      {backend = "synthetic-probe", total = count, opportunities = records}}
  end
end

local selected = arg[1] == "--case" and assert(arg[2], "--case requires NAME") or nil
local matched = false
for _, case in ipairs(cases) do
  if selected == nil or case[1] == selected then
    matched = true
    local name, encode, value = table.unpack(case)
    local expected = encode(value)
    local samples = {}
    for sample = 1, 7 do
      collectgarbage("collect")
      local start = core.monotonic_ns()
      local elapsed, count = 0, 0
      repeat
        for _ = 1, 100 do
          assert(encode(value) == expected)
        end
        count = count + 100
        elapsed = core.monotonic_ns() - start
      until elapsed >= 250000000
      samples[sample] = elapsed / count
    end
    table.sort(samples)
    print(string.format("%s\t%.3f\t%d", name, samples[4], #expected))
  end
end
assert(matched, "unknown benchmark case: " .. tostring(selected))
