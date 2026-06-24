local lonejson = require("lonejson")
local core = lonejson.core

local function assert_true(value, message)
  if not value then
    error(message or "assertion failed", 2)
  end
end

assert_true(type(core._test_reset_encode_stats) == "function")
assert_true(type(core._test_get_encode_stats) == "function")

local limited = lonejson.new({ write_max_output_bytes = 32 })
local pretty_limited = lonejson.new({
  write_pretty = true,
  write_max_output_bytes = 32,
})
local pretty = lonejson.new({ write_pretty = true })

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
  assert_true(stats.pretty_key_bytes_live == 0)
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
  assert_true(stats.pretty_key_bytes_live == 0)
  assert_true(stats.pretty_key_peak_bytes_live > 0)
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
    assert_true(stats.pretty_key_bytes_live == 0,
      string.format("pretty key leak after cyclic failure: live=%d",
                    stats.pretty_key_bytes_live))
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
    assert_true(stats.pretty_key_bytes_live == 0,
      string.format("pretty key leak after invalid key failure: live=%d",
                    stats.pretty_key_bytes_live))
  end
end
