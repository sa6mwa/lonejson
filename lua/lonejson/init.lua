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

local function export_core(name)
  if core[name] ~= nil then
    M[name] = core[name]
  end
end

local core_exports = {
  "encode_json",
  "encode_json_to_sink",
  "decode_json",
  "base64_encode",
  "base64_decode",
  "visit_path_value_string",
  "visit_path_value_path",
  "visit_path_value_file",
  "visit_path_value_fd",
  "visit_candidates_string",
  "visit_candidates_path",
  "visit_candidates_file",
  "visit_candidates_fd",
  "fixed_string_scratch",
  "jwt_parse_compact",
  "jwt_decode_compact",
  "jwt_validate_compact_claims",
  "jwt_validate_compact_signature",
  "jwk_parse_json",
  "jwks_parse_json",
  "jwks_select_json",
  "oauth2_client_credentials_body",
  "oauth2_refresh_token_body",
  "oauth2_token_introspection_body",
  "oauth2_token_revocation_body",
  "oidc_authorization_code_token_body",
  "oauth2_token_response_parse_json",
  "oauth2_introspection_response_parse_json",
  "oidc_userinfo_response_parse_json",
  "oidc_pkce_challenge",
  "oidc_pkce_generate",
  "oidc_authorization_url",
  "oidc_authorization_callback_parse_query",
  "oidc_validate_bearer_token",
  "oidc_discovery_url",
  "oidc_discovery_parse_json",
  "oidc_jwks_cache_select_json",
}

for _, name in ipairs(core_exports) do
  export_core(name)
end

M.encode_value = M.encode_json
M.encode_value_to_sink = M.encode_json_to_sink
M.decode_value = M.decode_json
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

local function bind_runtime(obj, runtime, name)
  if runtime[name] ~= nil then
    obj[name] = function(first, ...)
      return runtime[name](runtime, runtime_method_args(obj, first, ...))
    end
  end
end

local runtime_exports = {
  "set_openssl_auth_provider",
  "set_http_provider",
  "jwt_parse_compact",
  "jwt_decode_compact",
  "jwt_validate_compact_claims",
  "jwt_validate_compact_signature",
  "jwk_parse_json",
  "jwks_parse_json",
  "jwks_select_json",
  "oauth2_client_credentials_body",
  "oauth2_refresh_token_body",
  "oauth2_token_introspection_body",
  "oauth2_token_revocation_body",
  "oidc_authorization_code_token_body",
  "oauth2_client_credentials_request",
  "oauth2_refresh_token_request",
  "oauth2_introspect_token_request",
  "oauth2_revoke_token_request",
  "oidc_userinfo_request",
  "oidc_authorization_code_token_request",
  "oauth2_token_response_parse_json",
  "oauth2_introspection_response_parse_json",
  "oidc_userinfo_response_parse_json",
  "oidc_pkce_challenge",
  "oidc_pkce_generate",
  "oidc_authorization_url",
  "oidc_authorization_callback_parse_query",
  "oidc_validate_bearer_token",
  "oidc_discovery_url",
  "oidc_discovery_parse_json",
  "oidc_fetch_discovery",
  "oidc_jwks_cache_select_json",
  "oidc_jwks_cache_refresh",
  "m2m_credential_generate",
  "m2m_verify_authorization",
  "m2m_signup_generate",
  "m2m_signup_complete",
}

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
  obj.base64_encode = function(first, ...)
    return core.base64_encode(runtime_method_args(obj, first, ...))
  end
  obj.base64_decode = function(first, ...)
    return core.base64_decode(runtime_method_args(obj, first, ...))
  end
  obj.visit_path_value_string = function(first, ...)
    local json, callbacks = runtime_method_args(obj, first, ...)
    return runtime:visit_path_value_string(json, callbacks)
  end
  obj.visit_path_value_path = function(first, ...)
    local path, callbacks = runtime_method_args(obj, first, ...)
    return runtime:visit_path_value_path(path, callbacks)
  end
  obj.visit_path_value_file = function(first, ...)
    local file, callbacks = runtime_method_args(obj, first, ...)
    return runtime:visit_path_value_file(file, callbacks)
  end
  obj.visit_path_value_fd = function(first, ...)
    local fd, callbacks = runtime_method_args(obj, first, ...)
    return runtime:visit_path_value_fd(fd, callbacks)
  end
  obj.visit_candidates_string = function(first, ...)
    local json, options = runtime_method_args(obj, first, ...)
    return runtime:visit_candidates_string(json, options)
  end
  obj.visit_candidates_path = function(first, ...)
    local path, options = runtime_method_args(obj, first, ...)
    return runtime:visit_candidates_path(path, options)
  end
  obj.visit_candidates_file = function(first, ...)
    local file, options = runtime_method_args(obj, first, ...)
    return runtime:visit_candidates_file(file, options)
  end
  obj.visit_candidates_fd = function(first, ...)
    local fd, options = runtime_method_args(obj, first, ...)
    return runtime:visit_candidates_fd(fd, options)
  end

  for _, name in ipairs(runtime_exports) do
    bind_runtime(obj, runtime, name)
  end

  return obj
end

return M
