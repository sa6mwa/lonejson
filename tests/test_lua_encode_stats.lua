local lonejson = require("lonejson")
local core = lonejson.core

local function assert_true(value, message)
  if not value then
    error(message or "assertion failed", 2)
  end
end

assert_true(type(core._test_reset_encode_stats) == "function")
assert_true(type(core._test_get_encode_stats) == "function")
assert_true(core._test_json_buf_overflow())

local limited = lonejson.new({ write_max_output_bytes = 32 })
local pretty_limited = lonejson.new({
  write_pretty = true,
  write_max_output_bytes = 32,
})
local pretty = lonejson.new({ write_pretty = true })

-- Schema, claim, and rewrite owners must release partial buffers when a
-- recoverable allocation failure interrupts their internal JSON encoding.
do
  local runtime = lonejson.new()
  local schema = runtime.schema("FailureCleanup", {
    runtime.field("value", runtime.json_value()),
  })
  local record = schema:new_record()
  local large = string.rep('"\\\n\t', 1024)
  for count = 0, 4 do
    core._test_fail_json_buf_allocation_after(count)
    local ok, err = pcall(function() schema:assign(record, {value = large}) end)
    core._test_fail_json_buf_allocation_after(-1)
    if count < 2 then
      assert_true(not ok)
      assert_true(err:find("failed to grow JSON buffer", 1, true) ~= nil, err)
    else
      -- Reserving the scalar's input length leaves just two allocations even
      -- when escaping doubles its output size.
      assert_true(ok, err)
      assert_true(record.value == large)
    end
    schema:assign(record, {value = "recovered"})
    assert_true(record.value == "recovered")
    core._test_fail_json_buf_allocation_after(count)
    ok, err = pcall(function() return schema:encode({value = large}) end)
    core._test_fail_json_buf_allocation_after(-1)
    if count < 2 then
      assert_true(not ok)
      assert_true(err:find("failed to grow JSON buffer", 1, true) ~= nil, err)
    else
      assert_true(ok, err)
    end
    assert_true(schema:encode({value = "recovered"}) == '{"value":"recovered"}')
  end
  local cycle = {alpha = {}}
  cycle.alpha.self = cycle
  for _, value in ipairs({cycle, {alpha = function() end}, {alpha = math.huge}}) do
    local ok = pcall(function() schema:assign(record, {value = value}) end)
    assert_true(not ok)
    schema:assign(record, {value = "recovered"})
    assert_true(record.value == "recovered")
  end
  for _, name in ipairs({"m2m_credential_generate", "m2m_signup_generate"}) do
    if runtime[name] ~= nil then
      for count = 0, 4 do
        core._test_fail_json_buf_allocation_after(count)
        local ok, err = pcall(function()
          return runtime[name]({base_url = "https://example.test/signup", claim = large})
        end)
        core._test_fail_json_buf_allocation_after(-1)
        assert_true(not ok)
        assert_true(err:find("failed to grow JSON buffer", 1, true) ~= nil, err)
      end
    end
  end
  for _, action in ipairs({"replace", "insert_before", "insert_after"}) do
    for count = 0, 4 do
      local ok, output, err = pcall(runtime.array_rewrite_string, "items", '{"items":[1]}', {
        item = function()
          core._test_fail_json_buf_allocation_after(count)
          return {action = action, replacement = large, insert = large}
        end,
      })
      core._test_fail_json_buf_allocation_after(-1)
      assert_true(ok, output)
      assert_true(output == nil and err.status == "callback_failed")
      assert_true(err.message:find("failed to grow JSON buffer", 1, true) ~= nil, err.message)
    end
  end
  for count = 0, 4 do
    local output, err = runtime.array_rewrite_string("items", '{"items":[]}', {
      append = function(_, emit)
        core._test_fail_json_buf_allocation_after(count)
        local ok, emit_err = emit(large)
        core._test_fail_json_buf_allocation_after(-1)
        assert_true(ok == nil and emit_err.status == "callback_failed")
        assert_true(emit_err.message:find("failed to grow JSON buffer", 1, true) ~= nil)
      end,
    })
    core._test_fail_json_buf_allocation_after(-1)
    assert_true(output == '{"items":[]}', err and err.message)
  end
  for _ = 1, 8 do
    local ok = pcall(runtime.schema, "InvalidNestedCleanup", {
      {name = "allocated", kind = "string"},
      {name = "nested", kind = "object", fields = {
        {name = "bad", kind = "unsupported"},
      }},
    })
    assert_true(not ok)
    ok = pcall(runtime.array_rewrite_string, "items", '{"items":[1]}', {
      parents = {{segment = "items", schema = schema}, {segment = "missing"}},
      item = function() return "keep" end,
    })
    assert_true(not ok)
  end
  local text_schema = runtime.schema("StreamRecordReuse", {
    runtime.field("text", runtime.string()),
  })
  local text_record = text_schema:new_record()
  for _, text in ipairs({"first", "second", "third"}) do
    local stream = text_schema:stream_string('{"text":"' .. text .. '"}')
    local value, err, status = stream:next(text_record)
    assert_true(status == "object" and value.text == text, err and err.message)
    stream:close()
  end
end

-- Inline records recover from validation errors and table-access reentrancy;
-- larger inline layouts still exercise the protected scratch path.
do
  local runtime = lonejson.new()
  for _, capacity in ipairs({32, 512}) do
    local schema = runtime.schema("InlineCleanup" .. capacity, {
      runtime.field("name", runtime.string({fixed_capacity = capacity})),
      runtime.field("id", runtime.i64()),
      runtime.field("value", runtime.f64()),
    })
    local expected = '{"name":"outer","id":42,"value":1.5}'
    assert_true(schema:encode({name = "outer", id = 42, value = 1.5}) == expected)
    for _, payload in ipairs({
      {name = string.rep("x", capacity + 1), id = 42, value = 1.5},
      {name = "outer", id = {}, value = 1.5},
    }) do
      assert_true(not pcall(schema.encode, schema, payload))
      assert_true(schema:encode({name = "outer", id = 42, value = 1.5}) == expected)
    end
    local payload = setmetatable({id = 42, value = 1.5}, {
      __index = function(_, key)
        if key == "name" then
          assert_true(schema:encode({name = "inner", id = 7, value = 2}) ==
                      '{"name":"inner","id":7,"value":2}')
          return "outer"
        end
      end,
    })
    assert_true(schema:encode(payload) == expected)
  end
end

-- Scalar results on either side of the inline copy boundary remain exact.
for _, length in ipairs({125, 126, 127, 509, 510, 511}) do
  local text = string.rep("x", length)
  for _, encode in ipairs({core.encode_json, lonejson.new().encode_json,
                          pretty.encode_json}) do
    assert_true(encode(text) == '"' .. text .. '"')
  end
end

-- Inject failure on the initial allocation, fragment growth, and closing quote.
for _, encoder in ipairs({
  core.encode_json, core.encode_value, lonejson.encode_json, lonejson.encode_value,
  lonejson.new().encode_json, pretty.encode_json,
}) do
  for _, case in ipairs({
    {string.rep("x", 254), 0}, {string.rep('"', 127), 1},
    {string.rep('\0', 254), 2},
  }) do
    for count = 0, case[2] do
      core._test_fail_json_buf_allocation_after(count)
      local ok, err = pcall(encoder, case[1])
      core._test_fail_json_buf_allocation_after(-1)
      assert_true(not ok)
      assert_true(err:find("failed to grow JSON buffer", 1, true) ~= nil, err)
      assert_true(err:match("^[%g%s]+$") ~= nil, err)
      assert_true(encoder("ok") == '"ok"')
    end
  end
end

for _, runtime in ipairs({lonejson.new(), pretty}) do
  for _, method in ipairs({"encode_json", "encode_value"}) do
    for count = 0, 4 do
      core._test_reset_encode_stats()
      core._test_fail_json_buf_allocation_after(count)
      local ok, err = pcall(runtime[method], {
        alpha = {beta = string.rep("x", 1024)},
        gamma = string.rep('x"\\\n', 1024),
        zeta = true,
      })
      core._test_fail_json_buf_allocation_after(-1)
      assert_true(not ok)
      assert_true(err:find("failed to grow JSON buffer", 1, true) ~= nil, err)
      assert_true(core._test_get_encode_stats().json_key_bytes_live == 0)
      assert_true(runtime[method]("ok") == '"ok"')
    end
  end
end

for _, runtime in ipairs({limited, pretty_limited}) do
  for _, value in ipairs({
    {alpha = string.rep("x", 256), beta = "y"},
    {[string.rep("x", 256)] = 1, beta = "y"},
  }) do
    core._test_reset_encode_stats()
    local ok, err = pcall(runtime.encode_json, value)
    assert_true(not ok)
    assert_true(err:find("max_output_bytes", 1, true) ~= nil, err)
    assert_true(err:match("^[%g%s]+$") ~= nil, err)
    assert_true(core._test_get_encode_stats().json_key_bytes_live == 0)
  end
end

for _, pretty_output in ipairs({false, true}) do
  local runtime = lonejson.new({write_pretty = pretty_output})
  local tiny = lonejson.new({write_pretty = pretty_output, write_max_output_bytes = 1})
  local wide = {}
  for i = 1, 17 do
    wide[string.rep("k", 64) .. i] = i
  end
  core._test_reset_encode_stats()
  local ok, err = pcall(tiny.encode_json, wide)
  assert_true(not ok)
  assert_true(err:find("max_output_bytes", 1, true) ~= nil, err)
  assert_true(core._test_get_encode_stats().json_key_bytes_live == 0)
  assert_true(core._test_get_encode_stats().json_key_peak_bytes_live > 0)

  local cycle = {alpha = {beta = {}}}
  cycle.alpha.beta.self = cycle
  for _, value in ipairs({cycle, {alpha = 1, beta = function() end},
    {alpha = 1, beta = math.huge}, {[false] = true, alpha = 1}}) do
    core._test_reset_encode_stats()
    ok = pcall(runtime.encode_json, value)
    assert_true(not ok)
    assert_true(core._test_get_encode_stats().json_key_bytes_live == 0)
  end
  core._test_reset_encode_stats()
  ok, err = pcall(runtime.encode_json_to_sink, wide, function()
    error("injected sink failure")
  end)
  assert_true(not ok)
  assert_true(err:find("injected sink failure", 1, true) ~= nil, err)
  assert_true(core._test_get_encode_stats().json_key_bytes_live == 0)
end

core._test_reset_encode_stats()
do
  local ok, err = pcall(function()
    limited:encode_json({ alpha = string.rep("x", 256) })
  end)
  local stats = core._test_get_encode_stats()
  assert_true(not ok)
  assert_true(tostring(err):find("max_output_bytes", 1, true) ~= nil)
  assert_true(
    stats.json_buf_peak_capacity <= 33,
    string.format("compact encoder buffered past cap: peak=%d",
                  stats.json_buf_peak_capacity)
  )
  assert_true(stats.json_key_bytes_live == 0)
end

core._test_reset_encode_stats()
do
  local ok, err = pcall(function()
    pretty_limited:encode_json({
      alpha = string.rep("x", 256),
      beta = "y",
      gamma = 1,
    })
  end)
  local stats = core._test_get_encode_stats()
  assert_true(not ok)
  assert_true(tostring(err):find("max_output_bytes", 1, true) ~= nil)
  assert_true(
    stats.json_buf_peak_capacity <= 33,
    string.format("pretty encoder buffered past cap: peak=%d",
                  stats.json_buf_peak_capacity)
  )
  assert_true(stats.json_key_bytes_live == 0)
end

core._test_reset_encode_stats()
do
  local root = {
    alpha = {
      beta = {
        self = nil,
      },
    },
    gamma = 1,
  }
  root.alpha.beta.self = root
  for _ = 1, 32 do
    local ok, err = pcall(function()
      pretty:encode_json(root)
    end)
    local stats = core._test_get_encode_stats()
    assert_true(not ok)
    assert_true(tostring(err):find("cyclic Lua tables", 1, true) ~= nil)
    assert_true(stats.json_key_bytes_live == 0,
      string.format("pretty key leak after cyclic failure: live=%d",
                    stats.json_key_bytes_live))
  end
end

core._test_reset_encode_stats()
do
  local bad = { alpha = 1, beta = 2 }
  bad[false] = "boom"
  for _ = 1, 32 do
    local ok, err = pcall(function()
      pretty:encode_json(bad)
    end)
    local stats = core._test_get_encode_stats()
    assert_true(not ok)
    assert_true(tostring(err):find("JSON object keys must be strings", 1, true) ~= nil)
    assert_true(stats.json_key_bytes_live == 0,
      string.format("pretty key leak after invalid key failure: live=%d",
                    stats.json_key_bytes_live))
  end
end
