local lonejson = require("lonejson")

local function new_runtime(extra)
  local config = {}
  local key

  if extra ~= nil then
    for key, value in pairs(extra) do
      config[key] = value
    end
  end
  return lonejson.new(config)
end

local lj = new_runtime()

local function fail(msg)
  error(msg, 0)
end

local function assert_true(cond, msg)
  if not cond then
    fail(msg or "assert_true failed")
  end
end

local function sort_object_keys(value)
  local keys = {}
  for key in pairs(value) do
    keys[#keys + 1] = key
  end
  table.sort(keys)
  return keys
end

local function canonicalize(value)
  local kind
  local out
  local i

  if value == lj.json_null then
    return { __json_null = true }
  end
  if type(value) ~= "table" then
    return value
  end
  kind = getmetatable(value) and getmetatable(value).__lonejson_json_kind
  if kind == "array" then
    out = {}
    for i = 1, #value do
      out[i] = canonicalize(value[i])
    end
    return out
  end
  out = {}
  for _, key in ipairs(sort_object_keys(value)) do
    out[key] = canonicalize(value[key])
  end
  return out
end

local function deep_equal(a, b)
  local ta = type(a)
  local tb = type(b)
  local key

  if ta ~= tb then
    return false
  end
  if ta ~= "table" then
    return a == b
  end
  for key, value in pairs(a) do
    if not deep_equal(value, b[key]) then
      return false
    end
  end
  for key in pairs(b) do
    if a[key] == nil then
      return false
    end
  end
  return true
end

local function assert_json_equal(actual, expected, label)
  local left = canonicalize(actual)
  local right = canonicalize(expected)

  assert_true(deep_equal(left, right), label or "json values differ")
end

local function encode_decode(value)
  return lj.decode_json(lj.encode_json(value))
end

local function random_string(prefix)
  return prefix .. tostring(math.random(1000000))
end

local function random_json_value(depth)
  local choice
  local out
  local count
  local i

  if depth <= 0 then
    choice = math.random(4)
  else
    choice = math.random(6)
  end
  if choice == 1 then
    return random_string("s")
  end
  if choice == 2 then
    return math.random(-50, 50)
  end
  if choice == 3 then
    return math.random(0, 1) == 1
  end
  if choice == 4 then
    return lj.json_null
  end
  if choice == 5 then
    out = lj.json_array({})
    count = math.random(0, 3)
    for i = 1, count do
      out[i] = random_json_value(depth - 1)
    end
    return out
  end
  out = lj.json_object({})
  count = math.random(0, 3)
  for i = 1, count do
    out[random_string("k")] = random_json_value(depth - 1)
  end
  return out
end

local function random_nested_query_doc()
  local response = {
    selector = random_json_value(3),
  }
  local doc = {
    type = random_string("evt-"),
    response = response,
  }

  if response.selector == lj.json_null then
    response.selector = lj.json_object({})
  end
  if math.random(0, 1) == 1 then
    response.status = random_string("st-")
  end
  if math.random(0, 1) == 1 then
    response.fields = random_json_value(3)
    if response.fields == lj.json_null then
      response.fields = lj.json_array({})
    end
  end
  return doc
end

local function random_batch_doc()
  local doc = {
    batch_id = random_string("batch-"),
    items = lj.json_array({}),
  }
  local count = math.random(0, 3)
  local i
  local item

  for i = 1, count do
    item = {
      id = random_string("item-"),
      payload = random_json_value(3),
    }
    if item.payload == lj.json_null then
      item.payload = lj.json_array({})
    end
    if math.random(0, 1) == 1 then
      item.tags = random_json_value(3)
      if item.tags == lj.json_null then
        item.tags = lj.json_object({})
      end
    end
    doc.items[i] = item
  end
  return doc
end

local function merge_nested_query(old_doc, patch)
  local merged = {
    type = patch.type or old_doc.type,
    response = old_doc.response,
  }

  if patch.response ~= nil then
    merged.response = patch.response
  end
  return merged
end

local function normalize_nested_query(doc)
  local out = {
    type = doc.type,
    response = {
      status = doc.response.status,
    },
  }

  if doc.response.selector ~= lj.json_null then
    out.response.selector = doc.response.selector
  end
  if doc.response.fields ~= lj.json_null then
    out.response.fields = doc.response.fields
  end
  return out
end

local function normalize_batch(doc)
  local out = {
    batch_id = doc.batch_id,
    items = lj.json_array({}),
  }
  local i

  for i = 1, #doc.items do
    local src = doc.items[i]
    local item = {
      id = src.id,
    }

    if src.payload ~= lj.json_null then
      item.payload = src.payload
    end
    if src.tags ~= nil and src.tags ~= lj.json_null then
      item.tags = src.tags
    end
    out.items[i] = item
  end
  return out
end

local function build_nested_query_schema(runtime, name)
  return runtime.schema(name, {
    lj.field("type", lj.string { required = true }),
    lj.field("response", lj.object {
      required = true,
      fields = {
        lj.field("status", lj.string()),
        lj.field("selector", lj.json_value { required = true }),
        lj.field("fields", lj.json_value()),
      },
    }),
  })
end

local function build_batch_schema(runtime, name)
  return runtime.schema(name, {
    lj.field("batch_id", lj.string()),
    lj.field("items", lj.object_array {
      fields = {
        lj.field("id", lj.string { required = true }),
        lj.field("payload", lj.json_value { required = true }),
        lj.field("tags", lj.json_value()),
      },
    }),
  })
end

local function build_fixed_large_schema(runtime, name)
  return runtime.schema(name, {
    lj.field("payload", lj.string { required = true, fixed_capacity = 8192, overflow = "fail" }),
  })
end

local merge_lj = new_runtime({ clear_destination_by_default = false })
local fixed_string_scratch = lonejson.fixed_string_scratch(8192)
local fixed_large_scratch_lj = new_runtime({
  clear_destination_by_default = false,
  max_alloc_bytes = 1,
  fixed_string_scratch = fixed_string_scratch,
})

local NestedQuery = build_nested_query_schema(lj, "NestedQueryFuzz")
local NestedQueryNoClear =
    build_nested_query_schema(merge_lj, "NestedQueryFuzzNoClear")
local Batch = build_batch_schema(lj, "BatchFuzz")
local BatchNoClear = build_batch_schema(merge_lj, "BatchFuzzNoClear")
local FixedLarge =
    build_fixed_large_schema(fixed_large_scratch_lj, "FixedLargeFuzz")

math.randomseed(123456789)

do
  local i

  for i = 1, 200 do
    local doc = random_nested_query_doc()
    local json = lj.encode_json(doc)
    local decoded = NestedQuery:decode(json)
    local rec = NestedQuery:new_record()
    local stream = NestedQuery:stream_string(json)
    local obj, err, status
    local expected = normalize_nested_query(doc)

    assert_json_equal(decoded, encode_decode(expected), "nested decode mismatch")

    NestedQuery:decode_into(rec, json)
    assert_json_equal(rec:to_table(), encode_decode(expected),
                      "nested decode_into mismatch")

    obj, err, status = stream:next()
    assert_true(err == nil, "nested stream error")
    assert_true(status == "object", "nested stream status mismatch")
    assert_json_equal(obj, encode_decode(expected),
                      "nested stream object mismatch")
    obj, err, status = stream:next()
    assert_true(status == "eof", "nested stream eof mismatch")
    stream:close()
  end
end

do
  local i

  for i = 1, 200 do
    local base = random_nested_query_doc()
    local patch = random_nested_query_doc()
    local expected
    local rec = NestedQueryNoClear:new_record()

    patch.type = random_string("evt-")
    NestedQueryNoClear:decode_into(rec, lj.encode_json(base))
    NestedQueryNoClear:decode_into(rec, lj.encode_json(patch))
    expected = normalize_nested_query(merge_nested_query(base, patch))
    assert_json_equal(rec:to_table(), encode_decode(expected),
                      "nested merge mismatch")
  end
end

do
  local i

  for i = 1, 200 do
    local doc = random_batch_doc()
    local json = lj.encode_json(doc)
    local decoded = Batch:decode(json)
    local rec = Batch:new_record()
    local stream = Batch:stream_string(json)
    local obj, err, status
    local expected = normalize_batch(doc)

    assert_json_equal(decoded, encode_decode(expected), "batch decode mismatch")

    Batch:decode_into(rec, json)
    assert_json_equal(rec:to_table(), encode_decode(expected),
                      "batch decode_into mismatch")

    obj, err, status = stream:next()
    assert_true(err == nil, "batch stream error")
    assert_true(status == "object", "batch stream status mismatch")
    assert_json_equal(obj, encode_decode(expected),
                      "batch stream object mismatch")
    obj, err, status = stream:next()
    assert_true(status == "eof", "batch stream eof mismatch")
    stream:close()
  end
end

do
  local i

  for i = 1, 200 do
    local first = random_batch_doc()
    local second = random_batch_doc()
    local rec = BatchNoClear:new_record()

    BatchNoClear:decode_into(rec, lj.encode_json(first))
    BatchNoClear:decode_into(rec, lj.encode_json(second))
    assert_json_equal(rec:to_table(), encode_decode(normalize_batch(second)),
                      "batch replace-on-present mismatch")
  end
end

do
  local function long_text(ch, n)
    return string.rep(ch, n)
  end

  local i

  for i = 1, 50 do
    local first = long_text("a", math.random(5000, 7000))
    local second = long_text("b", math.random(5000, 7600))
    local rec = FixedLarge:new_record()
    local stream
    local obj, err, status
    local ok

    FixedLarge:decode_into(rec, '{"payload":"' .. first .. '"}')
    ok, err = FixedLarge:decode_into(rec, '{"payload":"' .. second .. '"}')
    assert_true(err == nil, "fixed scratch decode_into error")
    assert_true(ok.payload == second, "fixed scratch decode_into mismatch")

    stream = FixedLarge:stream_string(
        '{"payload":"' .. first .. '"}{"payload":"' .. second .. '"}')
    obj, err, status = stream:next(rec)
    assert_true(err == nil and status == "object",
                "fixed scratch stream first mismatch")
    assert_true(obj.payload == first, "fixed scratch stream first payload")
    obj, err, status = stream:next(rec)
    assert_true(err == nil and status == "object",
                "fixed scratch stream second mismatch")
    assert_true(obj.payload == second, "fixed scratch stream second payload")
    obj, err, status = stream:next(rec)
    assert_true(status == "eof", "fixed scratch stream eof mismatch")
    stream:close()
  end
end

do
  local samples = {
    "",
    "{",
    "[1,",
    '{"a":true}',
    '{"a":[1,null,"x"]}',
    string.rep("A", 257),
    "\0\1\2not-json",
    "eyJhbGciOiJSUzI1NiJ9.e30.sig",
    "%%%%",
  }
  local i

  for i = 1, #samples do
    local sample = samples[i]
    pcall(function() lj.decode_json(sample) end)
    pcall(function() lj.base64_decode(sample) end)
    pcall(function() lj.base64_decode(sample, "url_raw") end)
    pcall(function() lj.base64_encode(sample, "url_raw") end)
    pcall(function()
      lj.visit_path_value_string(sample, {
        null = function() end,
        bool = function() end,
        number = function() end,
        string = function() end,
        object_begin = function() end,
        object_end = function() end,
        array_begin = function() end,
        array_end = function() end,
      })
    end)
    pcall(function()
      lj.visit_candidates_string(sample, {
        framing = "auto",
        capture = "buffer",
        candidate_end = function() end,
      })
    end)
    pcall(function()
      lj.array_rewrite_string("items", sample, {
        item = function(value)
          return value
        end,
      })
    end)
    if lj.jwt_parse_compact ~= nil then
      pcall(function() lj.jwt_parse_compact(sample) end)
      pcall(function() lj.jwt_decode_compact(sample) end)
      pcall(function()
        lj.jwt_validate_compact_claims(sample, {
          accepted_algs = { "RS256" },
          accepted_issuers = { "issuer" },
          accepted_audiences = { "api" },
          now = 1000,
        })
      end)
      pcall(function()
        lj.jwk_parse_json(sample)
      end)
      pcall(function()
        lj.jwks_parse_json(sample)
      end)
      pcall(function()
        lj.jwks_select_json(sample, { kid = "kid", kty = "RSA" })
      end)
    end
    if lj.oauth2_token_response_parse_json ~= nil then
      pcall(function() lj.oauth2_token_response_parse_json(sample) end)
      pcall(function() lj.oauth2_introspection_response_parse_json(sample) end)
      pcall(function() lj.oidc_userinfo_response_parse_json(sample) end)
      pcall(function() lj.oidc_discovery_parse_json(sample) end)
      pcall(function()
        lj.oidc_authorization_callback_parse_query(sample, {
          expected_state = "state",
          max_query_bytes = 1024,
        })
      end)
      pcall(function()
        lj.oauth2_token_flow_update_response({}, {
          access_token = sample,
          token_type = "Bearer",
          refresh_token = sample,
          expires_in = 1,
        }, 1000)
      end)
      pcall(function()
        lj.oauth2_token_flow_is_expired({
          access_token = sample,
          token_type = "Bearer",
          expires_at = 1000,
        }, 1001, 0)
      end)
      pcall(function()
        lj.oidc_authorization_url({
          authorization_endpoint = "https://id.example/auth",
          client_id = sample,
          redirect_uri = "http://127.0.0.1/cb",
          state = "state",
          code_challenge = "challenge",
        })
      end)
      pcall(function()
        lj.m2m_verify_authorization("Authorization: Bearer " .. sample,
                                    '{"credentials":[]}', {})
      end)
    end
  end
end

print("lua fuzz smoke passed")
