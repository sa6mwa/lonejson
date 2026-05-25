local core = require("lonejson.core")

local M = {}

local function field(kind, opts)
  opts = opts or {}
  opts.kind = kind
  return opts
end

function M.field(name, spec)
  spec = spec or {}
  spec.name = name
  return spec
end

function M.string(opts)
  return field("string", opts)
end

function M.spooled_text(opts)
  return field("spooled_text", opts)
end

function M.spooled_bytes(opts)
  return field("spooled_bytes", opts)
end

function M.json_value(opts)
  return field("json_value", opts)
end

function M.i64(opts)
  return field("i64", opts)
end

function M.u64(opts)
  return field("u64", opts)
end

function M.f64(opts)
  return field("f64", opts)
end

function M.boolean(opts)
  return field("boolean", opts)
end

M.bool = M.boolean

function M.object(opts)
  return field("object", opts)
end

function M.string_array(opts)
  return field("string_array", opts)
end

function M.i64_array(opts)
  return field("i64_array", opts)
end

function M.u64_array(opts)
  return field("u64_array", opts)
end

function M.f64_array(opts)
  return field("f64_array", opts)
end

function M.boolean_array(opts)
  return field("boolean_array", opts)
end

function M.object_array(opts)
  return field("object_array", opts)
end

function M.json_array(value)
  value = value or {}
  return setmetatable(value, { __lonejson_json_kind = "array" })
end

function M.json_object(value)
  value = value or {}
  return setmetatable(value, { __lonejson_json_kind = "object" })
end

function M.chunks(spool, chunk_size)
  spool:rewind()
  return function()
    return spool:read(chunk_size or 4096)
  end
end

M.encode_json = core.encode_json
M.encode_json_to_sink = core.encode_json_to_sink
M.encode_value = core.encode_json
M.encode_value_to_sink = core.encode_json_to_sink
M.decode_json = core.decode_json
M.decode_value = core.decode_json
M.fixed_string_scratch = core.fixed_string_scratch
M.core = core
M.json_null = core.json_null()

local function unwrap_runtime(result)
  if type(result) == "table" and result.status ~= nil and
      result.message ~= nil and result.schema == nil then
    error(result, 2)
  end
  assert(result)
  return result
end

local function runtime_method_args(runtime_facade, first, ...)
  if first == runtime_facade then
    return ...
  end
  return first, ...
end

function M.new(config)
  local runtime = unwrap_runtime(core.new(config))
  local obj = {
    field = M.field,
    string = M.string,
    spooled_text = M.spooled_text,
    spooled_bytes = M.spooled_bytes,
    json_value = M.json_value,
    i64 = M.i64,
    u64 = M.u64,
    f64 = M.f64,
    boolean = M.boolean,
    bool = M.bool,
    object = M.object,
    string_array = M.string_array,
    i64_array = M.i64_array,
    u64_array = M.u64_array,
    f64_array = M.f64_array,
    boolean_array = M.boolean_array,
    object_array = M.object_array,
    json_array = M.json_array,
    json_object = M.json_object,
    chunks = M.chunks,
    fixed_string_scratch = M.fixed_string_scratch,
    core = core,
    json_null = M.json_null,
  }

  obj.schema = function(first, ...)
    local name, fields = runtime_method_args(obj, first, ...)
    return runtime:schema(name, fields)
  end
  obj.array_rewrite_string = function(first, ...)
    local selector, input, options = runtime_method_args(obj, first, ...)
    return runtime:array_rewrite_string(selector, input, options)
  end
  obj.array_rewrite_path = function(first, ...)
    local selector, input_path, output_path, options =
        runtime_method_args(obj, first, ...)
    return runtime:array_rewrite_path(selector, input_path, output_path, options)
  end
  obj.encode_json = function(first, ...)
    local value = runtime_method_args(obj, first, ...)
    return runtime:encode_json(value)
  end
  obj.encode_json_to_sink = function(first, ...)
    local value, sink = runtime_method_args(obj, first, ...)
    return runtime:encode_json_to_sink(value, sink)
  end
  obj.encode_value = obj.encode_json
  obj.encode_value_to_sink = obj.encode_json_to_sink
  obj.decode_json = function(first, ...)
    local json = runtime_method_args(obj, first, ...)
    return runtime:decode_json(json)
  end
  obj.decode_value = obj.decode_json

  return obj
end

return M
