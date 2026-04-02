local lj = require("lonejson")
local core = lj.core
local HAVE_CJSON, cjson = pcall(require, "cjson")

local BENCH_SAMPLE_COUNT = 5
local BENCH_MIN_SAMPLE_NS = 100000000
local BENCH_SCHEMA_VERSION = 6
local BENCH_NOISE_DELTA_PCT = 3.0
local BENCH_MATERIAL_DELTA_PCT = 5.0

local PARSE_JSON = table.concat({
  '{"name":"Alice","nickname":"Wonderland","age":34,',
  '"score":99.5,"active":true,',
  '"address":{"city":"Stockholm","zip":12345},',
  '"lucky_numbers":[7,11,42],',
  '"tags":["admin","ops"],',
  '"items":[{"id":1,"label":"alpha"},{"id":2,',
  '"label":"bravo-longer-than-fit"}]}',
})

local VENDOR_LONG_STRINGS_JSON =
    '{"x":[{"id": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"}], "id": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"}'
local VENDOR_EXTREME_NUMBERS_JSON = '{ "min": -1.0e+28, "max": 1.0e+28 }'
local VENDOR_STRING_UNICODE_JSON =
    '{"title":"\\u041f\\u043e\\u043b\\u0442\\u043e\\u0440\\u0430 \\u0417\\u0435\\u043c\\u043b\\u0435\\u043a\\u043e\\u043f\\u0430" }'

local function hostname()
  local p = io.popen("hostname -s 2>/dev/null || hostname 2>/dev/null", "r")
  local out
  if p == nil then
    return "unknown"
  end
  out = p:read("*a") or ""
  p:close()
  out = out:gsub("%s+$", "")
  if out == "" then
    return "unknown"
  end
  return out
end

local function read_file(path)
  local f = assert(io.open(path, "rb"))
  local data = assert(f:read("*a"))
  f:close()
  return data
end

local function current_dir()
  local p = io.popen("pwd", "r")
  local out
  if p == nil then
    return nil
  end
  out = p:read("*a") or ""
  p:close()
  out = out:gsub("%s+$", "")
  if out == "" then
    return nil
  end
  return out
end

local function portable_path(path)
  local cwd = current_dir()
  if type(path) ~= "string" or path == "" then
    return path
  end
  if cwd ~= nil and path:sub(1, #cwd) == cwd then
    local relative = path:sub(#cwd + 1)
    if relative:sub(1, 1) == "/" then
      relative = relative:sub(2)
    end
    if relative ~= "" then
      return relative
    end
  end
  return path
end

local JAPANESE_UTF8_JSON = read_file("tests/fixtures/languages/japanese_hojoki.json")
local HEBREW_UTF8_JSON = read_file("tests/fixtures/languages/hebrew_fox_and_stork.json")
local ARABIC_UTF8_JSON = read_file("tests/fixtures/languages/arabic_kalila_intro.json")
local JAPANESE_UTF8_WIDE_JSON = read_file("tests/fixtures/languages/japanese_hojoki_wide.json")
local HEBREW_UTF8_WIDE_JSON = read_file("tests/fixtures/languages/hebrew_fox_and_stork_wide.json")
local ARABIC_UTF8_WIDE_JSON = read_file("tests/fixtures/languages/arabic_kalila_intro_wide.json")

local JAPANESE_UTF8_PREFIX = "行く川のながれは絶えずして、しかも本の水にあらず。"
local HEBREW_UTF8_PREFIX = "השועל והחסידה הוא משל."
local ARABIC_UTF8_PREFIX = "قدمها بهنود بن سحوان ويعرف بعلي بن الشاه الفارسي."

local function append_file(path, payload)
  local f = assert(io.open(path, "ab"))
  assert(f:write(payload))
  assert(f:write("\n"))
  f:close()
end

local function ensure_dir(path)
  assert(os.execute("mkdir -p " .. path) == true or os.execute("mkdir -p " .. path) == 0)
end

local function utc_timestamp_from_ns(ns)
  local sec = math.floor(ns / 1000000000)
  return os.date("!%Y-%m-%dT%H:%M:%SZ", sec)
end

local function wall_clock_epoch_ns()
  return os.time() * 1000000000
end

local function mib_per_sec(bytes, elapsed_ns)
  return (bytes / (1024.0 * 1024.0)) / (elapsed_ns / 1000000000.0)
end

local function docs_per_sec(docs, elapsed_ns)
  return docs / (elapsed_ns / 1000000000.0)
end

local function ns_per_byte(bytes, elapsed_ns)
  return elapsed_ns / bytes
end

local function shallow_copy(tbl)
  local out = {}
  local k
  for k, v in pairs(tbl) do
    out[k] = v
  end
  return out
end

local function classify_delta(delta_pct)
  local abs_delta = math.abs(delta_pct)
  if abs_delta < BENCH_NOISE_DELTA_PCT then
    return "noise"
  end
  if abs_delta < BENCH_MATERIAL_DELTA_PCT then
    return "small"
  end
  return "material"
end

local Address = lj.object {
  fields = {
    lj.field("city", lj.string { required = true, fixed_capacity = 24, overflow = "fail" }),
    lj.field("zip", lj.i64 { required = true }),
  },
}

local Item = lj.object {
  fields = {
    lj.field("id", lj.i64 { required = true }),
    lj.field("label", lj.string { fixed_capacity = 16, overflow = "truncate" }),
  },
}

local FixedRecord = lj.schema("BenchFixed", {
  lj.field("name", lj.string { required = true, fixed_capacity = 24, overflow = "fail" }),
  lj.field("nickname", lj.string { fixed_capacity = 16, overflow = "truncate" }),
  lj.field("age", lj.i64()),
  lj.field("score", lj.f64()),
  lj.field("active", lj.bool()),
  lj.field("address", lj.object { required = true, fields = Address.fields }),
  lj.field("lucky_numbers", lj.i64_array { fixed_capacity = 8, overflow = "fail" }),
  lj.field("items", lj.object_array { fields = Item.fields, fixed_capacity = 8, overflow = "fail" }),
})

local DynamicRecord = lj.schema("BenchDynamic", {
  lj.field("name", lj.string { required = true }),
  lj.field("nickname", lj.string { fixed_capacity = 16, overflow = "truncate" }),
  lj.field("age", lj.i64()),
  lj.field("score", lj.f64()),
  lj.field("active", lj.bool()),
  lj.field("address", lj.object { required = true, fields = Address.fields }),
  lj.field("lucky_numbers", lj.i64_array { overflow = "fail" }),
  lj.field("tags", lj.string_array { overflow = "fail" }),
  lj.field("items", lj.object_array { fields = Item.fields, overflow = "fail" }),
})

local VendorLong = lj.schema("BenchVendorLong", {
  lj.field("x", lj.object_array {
    overflow = "fail",
    fields = {
      lj.field("id", lj.string { required = true, fixed_capacity = 48, overflow = "fail" }),
    },
  }),
  lj.field("id", lj.string { required = true, fixed_capacity = 48, overflow = "fail" }),
})

local VendorNumbers = lj.schema("BenchVendorNumbers", {
  lj.field("min", lj.f64 { required = true }),
  lj.field("max", lj.f64 { required = true }),
})

local VendorUnicode = lj.schema("BenchVendorUnicode", {
  lj.field("title", lj.string { required = true, fixed_capacity = 96, overflow = "fail" }),
})

local VendorLanguage = lj.schema("BenchVendorLanguage", {
  lj.field("title", lj.string { required = true, fixed_capacity = 2048, overflow = "fail" }),
})

local VendorLanguageWide = lj.schema("BenchVendorLanguageWide", {
  lj.field("lang", lj.string { required = true, fixed_capacity = 8, overflow = "fail" }),
  lj.field("script", lj.string { required = true, fixed_capacity = 24, overflow = "fail" }),
  lj.field("source", lj.string { required = true, fixed_capacity = 32, overflow = "fail" }),
  lj.field("title", lj.string { required = true, fixed_capacity = 64, overflow = "fail" }),
  lj.field("summary", lj.string { required = true, fixed_capacity = 256, overflow = "fail" }),
  lj.field("body", lj.string { required = true, fixed_capacity = 2048, overflow = "fail" }),
  lj.field("metrics", lj.object {
    required = true,
    fields = {
      lj.field("chars", lj.i64 { required = true }),
      lj.field("segments", lj.i64 { required = true }),
    },
  }),
})

local PhaseStats = lj.object {
  fields = {
    lj.field("ops", lj.i64()),
    lj.field("errors", lj.i64()),
  },
}

local LockdPhasesFields = {
  lj.field("acquire", lj.object { fields = PhaseStats.fields }),
  lj.field("attach", lj.object { fields = PhaseStats.fields }),
  lj.field("delete", lj.object { fields = PhaseStats.fields }),
  lj.field("get", lj.object { fields = PhaseStats.fields }),
  lj.field("release", lj.object { fields = PhaseStats.fields }),
  lj.field("total", lj.object { fields = PhaseStats.fields }),
  lj.field("update", lj.object { fields = PhaseStats.fields }),
  lj.field("query", lj.object { fields = PhaseStats.fields }),
  lj.field("acquire-a", lj.object { fields = PhaseStats.fields }),
  lj.field("acquire-b", lj.object { fields = PhaseStats.fields }),
  lj.field("decide", lj.object { fields = PhaseStats.fields }),
  lj.field("update-a", lj.object { fields = PhaseStats.fields }),
  lj.field("update-b", lj.object { fields = PhaseStats.fields }),
}

local LockdCase = lj.object {
  fields = {
    lj.field("workload", lj.string { required = true, fixed_capacity = 24, overflow = "fail" }),
    lj.field("size", lj.i64()),
    lj.field("ops", lj.i64 { required = true }),
    lj.field("payload_bytes", lj.i64()),
    lj.field("attachment_bytes", lj.i64()),
    lj.field("phases", lj.object { required = true, fields = LockdPhasesFields }),
  },
}

local LockdRun = lj.schema("BenchLockdRun", {
  lj.field("run_id", lj.string { required = true, fixed_capacity = 64, overflow = "fail" }),
  lj.field("timestamp", lj.string { required = true, fixed_capacity = 32, overflow = "fail" }),
  lj.field("history_branch", lj.string { fixed_capacity = 32, overflow = "fail" }),
  lj.field("preset", lj.string { required = true, fixed_capacity = 16, overflow = "fail" }),
  lj.field("backend", lj.string { required = true, fixed_capacity = 16, overflow = "fail" }),
  lj.field("store", lj.string { fixed_capacity = 32, overflow = "fail" }),
  lj.field("disk_root", lj.string { fixed_capacity = 128, overflow = "fail" }),
  lj.field("git", lj.object {
    required = true,
    fields = {
      lj.field("short_sha", lj.string { required = true, fixed_capacity = 16, overflow = "fail" }),
      lj.field("dirty", lj.bool { required = true }),
    },
  }),
  lj.field("golden", lj.bool()),
  lj.field("config", lj.object {
    required = true,
    fields = {
      lj.field("ha_mode", lj.string { required = true, fixed_capacity = 16, overflow = "fail" }),
      lj.field("concurrency", lj.i64 { required = true }),
      lj.field("runs", lj.i64 { required = true }),
      lj.field("warmup", lj.i64 { required = true }),
      lj.field("query_return", lj.string { fixed_capacity = 16, overflow = "fail" }),
      lj.field("qrf_disabled", lj.bool()),
    },
  }),
  lj.field("cases", lj.object_array { fields = LockdCase.fields, fixed_capacity = 16, overflow = "fail" }),
})

local BenchResultSchema = lj.object {
  fields = {
    lj.field("name", lj.string { required = true, fixed_capacity = 64, overflow = "fail" }),
    lj.field("group", lj.string { required = true, fixed_capacity = 24, overflow = "fail" }),
    lj.field("elapsed_ns", lj.i64 { required = true }),
    lj.field("total_bytes", lj.i64 { required = true }),
    lj.field("total_documents", lj.i64 { required = true }),
    lj.field("mismatch_count", lj.i64 { required = true }),
    lj.field("mib_per_sec", lj.f64 { required = true }),
    lj.field("docs_per_sec", lj.f64 { required = true }),
    lj.field("ns_per_byte", lj.f64 { required = true }),
    lj.field("sibling_name", lj.string { fixed_capacity = 64, overflow = "fail" }),
    lj.field("sibling_mib_per_sec", lj.f64()),
    lj.field("sibling_ratio", lj.f64()),
  },
}

local BenchRunSchema = lj.schema("LuaBenchRun", {
  lj.field("schema_version", lj.i64 { required = true }),
  lj.field("timestamp_epoch_ns", lj.i64 { required = true }),
  lj.field("timestamp_utc", lj.string { required = true, fixed_capacity = 32, overflow = "fail" }),
  lj.field("host", lj.string { required = true, fixed_capacity = 64, overflow = "fail" }),
  lj.field("lua_version", lj.string { required = true, fixed_capacity = 32, overflow = "fail" }),
  lj.field("c_latest_path", lj.string { fixed_capacity = 256, overflow = "fail" }),
  lj.field("iterations", lj.i64 { required = true }),
  lj.field("results", lj.object_array { required = true, fields = BenchResultSchema.fields, fixed_capacity = 64, overflow = "fail" }),
})

local CLatestResult = lj.object {
  fields = {
    lj.field("name", lj.string { required = true, fixed_capacity = 64, overflow = "fail" }),
    lj.field("mib_per_sec", lj.f64 { required = true }),
  },
}

local CLatestRun = lj.schema("CBenchRunSubset", {
  lj.field("results", lj.object_array { required = true, fields = CLatestResult.fields, fixed_capacity = 64, overflow = "fail" }),
})

local LOCKD_JSONL = read_file("tests/fixtures/lockdbench.jsonl")
local BENCH_RECORD_COMPACT_JSON = read_file("tests/golden/bench_record_compact.json")
local BENCH_RECORD_PRETTY_JSON = read_file("tests/golden/bench_record_pretty.json")
local LOCKD_LINES = {}
for line in LOCKD_JSONL:gmatch("[^\r\n]+") do
  LOCKD_LINES[#LOCKD_LINES + 1] = line
end

local EXPECTED_LABEL = "bravo-longer-th"
local function build_serialize_source()
  return {
    name = "Alice",
    nickname = "Wonderland",
    age = 34,
    score = 99.5,
    active = true,
    address = { city = "Stockholm", zip = 12345 },
    lucky_numbers = { 7, 11, 42 },
    items = {
      { id = 1, label = "alpha" },
      { id = 2, label = "bravo" },
    },
  }
end
local EXPECTED_JSON = BENCH_RECORD_COMPACT_JSON
local EXPECTED_PRETTY_JSON = BENCH_RECORD_PRETTY_JSON
local STREAM_JSON = table.concat({
  EXPECTED_JSON,
  EXPECTED_JSON,
  EXPECTED_JSON,
  EXPECTED_JSON,
  EXPECTED_JSON,
  EXPECTED_JSON,
  EXPECTED_JSON,
  EXPECTED_JSON,
}, "")

local FIXED_PATHS = {
  name = FixedRecord:compile_get("name"),
  nickname = FixedRecord:compile_get("nickname"),
  age = FixedRecord:compile_get("age"),
  score = FixedRecord:compile_get("score"),
  active = FixedRecord:compile_get("active"),
  address_city = FixedRecord:compile_get("address.city"),
  address_zip = FixedRecord:compile_get("address.zip"),
  lucky_numbers_count = FixedRecord:compile_count("lucky_numbers"),
  lucky_1 = FixedRecord:compile_get("lucky_numbers[1]"),
  lucky_2 = FixedRecord:compile_get("lucky_numbers[2]"),
  lucky_3 = FixedRecord:compile_get("lucky_numbers[3]"),
  items_count = FixedRecord:compile_count("items"),
  item1_id = FixedRecord:compile_get("items[1].id"),
  item1_label = FixedRecord:compile_get("items[1].label"),
  item2_id = FixedRecord:compile_get("items[2].id"),
  item2_label = FixedRecord:compile_get("items[2].label"),
}

local LOCKD_PATHS = {
  run_id = LockdRun:compile_get("run_id"),
  git_short_sha = LockdRun:compile_get("git.short_sha"),
  git_dirty = LockdRun:compile_get("git.dirty"),
  config_concurrency = LockdRun:compile_get("config.concurrency"),
  cases_count = LockdRun:compile_count("cases"),
  case1_workload = LockdRun:compile_get("cases[1].workload"),
  case5_workload = LockdRun:compile_get("cases[5].workload"),
  case9_workload = LockdRun:compile_get("cases[9].workload"),
  case1_acquire_ops = LockdRun:compile_get("cases[1].phases.acquire.ops"),
}

local function assert_fixed_record(rec)
  if FIXED_PATHS.name(rec) ~= "Alice" or FIXED_PATHS.nickname(rec) ~= "Wonderland" or FIXED_PATHS.age(rec) ~= 34 or FIXED_PATHS.score(rec) ~= 99.5 or FIXED_PATHS.active(rec) ~= true then
    return false
  end
  if FIXED_PATHS.address_city(rec) ~= "Stockholm" or FIXED_PATHS.address_zip(rec) ~= 12345 then
    return false
  end
  if FIXED_PATHS.lucky_numbers_count(rec) ~= 3 or FIXED_PATHS.lucky_1(rec) ~= 7 or FIXED_PATHS.lucky_2(rec) ~= 11 or FIXED_PATHS.lucky_3(rec) ~= 42 then
    return false
  end
  if FIXED_PATHS.items_count(rec) ~= 2 or FIXED_PATHS.item1_id(rec) ~= 1 or FIXED_PATHS.item1_label(rec) ~= "alpha" or FIXED_PATHS.item2_id(rec) ~= 2 or FIXED_PATHS.item2_label(rec) ~= EXPECTED_LABEL then
    return false
  end
  return true
end

local function assert_stream_fixed_record(rec)
  if FIXED_PATHS.name(rec) ~= "Alice" or FIXED_PATHS.nickname(rec) ~= "Wonderland" or FIXED_PATHS.age(rec) ~= 34 or FIXED_PATHS.score(rec) ~= 99.5 or FIXED_PATHS.active(rec) ~= true then
    return false
  end
  if FIXED_PATHS.address_city(rec) ~= "Stockholm" or FIXED_PATHS.address_zip(rec) ~= 12345 then
    return false
  end
  if FIXED_PATHS.lucky_numbers_count(rec) ~= 3 or FIXED_PATHS.lucky_1(rec) ~= 7 or FIXED_PATHS.lucky_2(rec) ~= 11 or FIXED_PATHS.lucky_3(rec) ~= 42 then
    return false
  end
  if FIXED_PATHS.items_count(rec) ~= 2 or FIXED_PATHS.item1_id(rec) ~= 1 or FIXED_PATHS.item1_label(rec) ~= "alpha" or FIXED_PATHS.item2_id(rec) ~= 2 or FIXED_PATHS.item2_label(rec) ~= "bravo" then
    return false
  end
  return true
end

local function assert_dynamic_record(obj)
  if obj.name ~= "Alice" or obj.nickname ~= "Wonderland" or obj.age ~= 34 or obj.score ~= 99.5 or obj.active ~= true then
    return false
  end
  if obj.address.city ~= "Stockholm" or obj.address.zip ~= 12345 then
    return false
  end
  if #obj.lucky_numbers ~= 3 or obj.lucky_numbers[1] ~= 7 or obj.lucky_numbers[2] ~= 11 or obj.lucky_numbers[3] ~= 42 then
    return false
  end
  if #obj.tags ~= 2 or obj.tags[1] ~= "admin" or obj.tags[2] ~= "ops" then
    return false
  end
  if #obj.items ~= 2 or obj.items[1].id ~= 1 or obj.items[1].label ~= "alpha" or obj.items[2].id ~= 2 or obj.items[2].label ~= EXPECTED_LABEL then
    return false
  end
  return true
end

local function assert_plain_table_record(obj)
  if obj.name ~= "Alice" or obj.nickname ~= "Wonderland" or obj.age ~= 34 or obj.score ~= 99.5 or obj.active ~= true then
    return false
  end
  if obj.address == nil or obj.address.city ~= "Stockholm" or obj.address.zip ~= 12345 then
    return false
  end
  if type(obj.lucky_numbers) ~= "table" or #obj.lucky_numbers ~= 3 or obj.lucky_numbers[1] ~= 7 or obj.lucky_numbers[2] ~= 11 or obj.lucky_numbers[3] ~= 42 then
    return false
  end
  if type(obj.items) ~= "table" or #obj.items ~= 2 or obj.items[1].id ~= 1 or obj.items[1].label ~= "alpha" or obj.items[2].id ~= 2 or obj.items[2].label ~= "bravo-longer-than-fit" then
    return false
  end
  return true
end

local function assert_vendor_long_table(obj)
  return obj ~= nil and obj.id == "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      and type(obj.x) == "table" and #obj.x == 1
      and type(obj.x[1]) == "table"
      and obj.x[1].id == "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
end

local function assert_vendor_numbers_table(obj)
  return obj ~= nil and obj.min ~= nil and obj.max ~= nil
      and obj.min < -9.9e27 and obj.min > -1.1e28
      and obj.max > 9.9e27 and obj.max < 1.1e28
end

local function assert_vendor_unicode_table(obj)
  return obj ~= nil and obj.title ~= nil and #obj.title > 0
end

local function assert_vendor_language_table(obj, prefix)
  return obj ~= nil and obj.title ~= nil and #obj.title > #prefix and obj.title:sub(1, #prefix) == prefix
end

local function assert_vendor_language_wide_table(obj, lang, script, source, summary_prefix, body_prefix, min_chars)
  return obj ~= nil
      and obj.lang == lang
      and obj.script == script
      and obj.source == source
      and obj.title ~= nil and #obj.title > 0
      and obj.summary ~= nil and obj.summary:sub(1, #summary_prefix) == summary_prefix
      and obj.body ~= nil and obj.body:sub(1, #body_prefix) == body_prefix
      and obj.metrics ~= nil
      and obj.metrics.segments == 4
      and obj.metrics.chars ~= nil
      and obj.metrics.chars >= min_chars
end

local function assert_lockd_table(obj, index)
  local ids = {
    "20260322T130525Z-b62770c-dirty-fast-disk",
    "20260322T141810Z-b62770c-dirty-fast-disk",
    "20260323T003016Z-08a87c0-clean-fast-disk",
    "20260325T004115Z-55f841b-dirty-fast-disk",
    "20260325T155329Z-e66dc79-dirty-fast-disk",
  }
  return obj ~= nil
      and obj.run_id == ids[index]
      and obj.git ~= nil
      and obj.git.short_sha ~= nil
      and obj.git.dirty ~= nil
      and obj.config ~= nil
      and obj.config.concurrency == 8
      and type(obj.cases) == "table"
      and #obj.cases == 9
      and obj.cases[1] ~= nil
      and obj.cases[1].workload == "attachments"
      and obj.cases[5] ~= nil
      and obj.cases[5].workload == "public-read"
      and obj.cases[9] ~= nil
      and obj.cases[9].workload == "xa-rollback"
      and obj.cases[1].phases ~= nil
      and obj.cases[1].phases.acquire ~= nil
      and obj.cases[1].phases.acquire.ops == 4000
end

local function cjson_decode(json)
  return cjson.decode(json)
end

local function load_c_siblings(path)
  local run
  local index = {}
  local i

  if path == nil or path == "" then
    return index
  end
  local f = io.open(path, "rb")
  if f == nil then
    return index
  end
  f:close()
  run = CLatestRun:decode_path(path)
  for i = 1, #run.results do
    index[run.results[i].name] = run.results[i].mib_per_sec
  end
  return index
end

local function median_sample(samples)
  table.sort(samples, function(a, b)
    return a.ns_per_byte < b.ns_per_byte
  end)
  return samples[math.floor(#samples / 2) + 1]
end

local function measure_case(case, iterations)
  local samples = {}
  local sample_index

  for sample_index = 1, BENCH_SAMPLE_COUNT do
    local total_bytes = 0
    local total_documents = 0
    local mismatch_count = 0
    local start_ns = core.monotonic_ns()
    local elapsed_ns = 0

    repeat
      local iteration
      for iteration = 1, iterations do
        local ok = case.run_once()
        if not ok then
          mismatch_count = mismatch_count + 1
        end
        total_bytes = total_bytes + case.bytes_per_call
        total_documents = total_documents + case.docs_per_call
      end
      elapsed_ns = core.monotonic_ns() - start_ns
    until elapsed_ns >= BENCH_MIN_SAMPLE_NS

    samples[sample_index] = {
      elapsed_ns = elapsed_ns,
      total_bytes = total_bytes,
      total_documents = total_documents,
      mismatch_count = mismatch_count,
      ns_per_byte = ns_per_byte(total_bytes, elapsed_ns),
    }
  end

  return median_sample(samples)
end

local function bench_cases()
  local cases = {}

  do
    cases[#cases + 1] = {
      name = "decode/table_dynamic/lua",
      group = "decode",
      sibling_name = "parse/buffer_dynamic/lonejson",
      bytes_per_call = #PARSE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_dynamic_record(DynamicRecord:decode(PARSE_JSON))
      end,
    }
  end

  do
    cases[#cases + 1] = {
      name = "decode/doc_long_strings/lua",
      group = "decode",
      bytes_per_call = #VENDOR_LONG_STRINGS_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_long_table(VendorLong:decode(VENDOR_LONG_STRINGS_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_extreme_numbers/lua",
      group = "decode",
      bytes_per_call = #VENDOR_EXTREME_NUMBERS_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_numbers_table(VendorNumbers:decode(VENDOR_EXTREME_NUMBERS_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_string_unicode/lua",
      group = "decode",
      bytes_per_call = #VENDOR_STRING_UNICODE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_unicode_table(VendorUnicode:decode(VENDOR_STRING_UNICODE_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/lockdbench/lua",
      group = "decode",
      bytes_per_call = #LOCKD_JSONL,
      docs_per_call = #LOCKD_LINES,
      run_once = function()
        local i
        for i = 1, #LOCKD_LINES do
          if not assert_lockd_table(LockdRun:decode(LOCKD_LINES[i]), i) then
            return false
          end
        end
        return true
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_japanese/lua",
      group = "decode",
      bytes_per_call = #JAPANESE_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_table(VendorLanguage:decode(JAPANESE_UTF8_JSON), JAPANESE_UTF8_PREFIX)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_hebrew/lua",
      group = "decode",
      bytes_per_call = #HEBREW_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_table(VendorLanguage:decode(HEBREW_UTF8_JSON), HEBREW_UTF8_PREFIX)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_arabic/lua",
      group = "decode",
      bytes_per_call = #ARABIC_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_table(VendorLanguage:decode(ARABIC_UTF8_JSON), ARABIC_UTF8_PREFIX)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_japanese_wide/lua",
      group = "decode",
      bytes_per_call = #JAPANESE_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_wide_table(VendorLanguageWide:decode(JAPANESE_UTF8_WIDE_JSON),
          "ja", "japanese", "hojoki", JAPANESE_UTF8_PREFIX, JAPANESE_UTF8_PREFIX, 180)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_hebrew_wide/lua",
      group = "decode",
      bytes_per_call = #HEBREW_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_wide_table(VendorLanguageWide:decode(HEBREW_UTF8_WIDE_JSON),
          "he", "hebrew", "aesop_wikisource", "השועל מזמין את החסידה לביתו", HEBREW_UTF8_PREFIX, 220)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_arabic_wide/lua",
      group = "decode",
      bytes_per_call = #ARABIC_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_wide_table(VendorLanguageWide:decode(ARABIC_UTF8_WIDE_JSON),
          "ar", "arabic", "kalila_dimna", "مقدمة تشرح سبب وضع كتاب كليلة", ARABIC_UTF8_PREFIX, 220)
      end,
    }
  end

  if HAVE_CJSON then
    cases[#cases + 1] = {
      name = "decode/table_dynamic/cjson",
      group = "decode",
      bytes_per_call = #PARSE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_plain_table_record(cjson_decode(PARSE_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_long_strings/cjson",
      group = "decode",
      bytes_per_call = #VENDOR_LONG_STRINGS_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_long_table(cjson_decode(VENDOR_LONG_STRINGS_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_extreme_numbers/cjson",
      group = "decode",
      bytes_per_call = #VENDOR_EXTREME_NUMBERS_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_numbers_table(cjson_decode(VENDOR_EXTREME_NUMBERS_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_string_unicode/cjson",
      group = "decode",
      bytes_per_call = #VENDOR_STRING_UNICODE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_unicode_table(cjson_decode(VENDOR_STRING_UNICODE_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/lockdbench/cjson",
      group = "decode",
      bytes_per_call = #LOCKD_JSONL,
      docs_per_call = #LOCKD_LINES,
      run_once = function()
        local i
        for i = 1, #LOCKD_LINES do
          if not assert_lockd_table(cjson_decode(LOCKD_LINES[i]), i) then
            return false
          end
        end
        return true
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_japanese/cjson",
      group = "decode",
      bytes_per_call = #JAPANESE_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_table(cjson_decode(JAPANESE_UTF8_JSON), JAPANESE_UTF8_PREFIX)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_hebrew/cjson",
      group = "decode",
      bytes_per_call = #HEBREW_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_table(cjson_decode(HEBREW_UTF8_JSON), HEBREW_UTF8_PREFIX)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_arabic/cjson",
      group = "decode",
      bytes_per_call = #ARABIC_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_table(cjson_decode(ARABIC_UTF8_JSON), ARABIC_UTF8_PREFIX)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_japanese_wide/cjson",
      group = "decode",
      bytes_per_call = #JAPANESE_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_wide_table(cjson_decode(JAPANESE_UTF8_WIDE_JSON),
          "ja", "japanese", "hojoki", JAPANESE_UTF8_PREFIX, JAPANESE_UTF8_PREFIX, 180)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_hebrew_wide/cjson",
      group = "decode",
      bytes_per_call = #HEBREW_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_wide_table(cjson_decode(HEBREW_UTF8_WIDE_JSON),
          "he", "hebrew", "aesop_wikisource", "השועל מזמין את החסידה לביתו", HEBREW_UTF8_PREFIX, 220)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_arabic_wide/cjson",
      group = "decode",
      bytes_per_call = #ARABIC_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_wide_table(cjson_decode(ARABIC_UTF8_WIDE_JSON),
          "ar", "arabic", "kalila_dimna", "مقدمة تشرح سبب وضع كتاب كليلة", ARABIC_UTF8_PREFIX, 220)
      end,
    }
  end

  do
    local rec = FixedRecord:new_record()
    cases[#cases + 1] = {
      name = "decode/record_fixed/lua",
      group = "decode",
      sibling_name = "parse/buffer_fixed/lonejson",
      bytes_per_call = #PARSE_JSON,
      docs_per_call = 1,
      run_once = function()
        FixedRecord:decode_into(rec, PARSE_JSON)
        return assert_fixed_record(rec)
      end,
    }
  end

  do
    local rec = FixedRecord:new_record()
    cases[#cases + 1] = {
      name = "decode/record_fixed_prepared/lua",
      group = "decode",
      sibling_name = "parse/buffer_fixed_prepared/lonejson",
      bytes_per_call = #PARSE_JSON,
      docs_per_call = 1,
      run_once = function()
        rec:clear()
        FixedRecord:decode_into(rec, PARSE_JSON, { clear_destination = false })
        return assert_fixed_record(rec)
      end,
    }
  end

  do
    cases[#cases + 1] = {
      name = "stream/record_fixed/lua",
      group = "stream",
      sibling_name = "stream/object_fixed/lonejson",
      bytes_per_call = #STREAM_JSON,
      docs_per_call = 8,
      run_once = function()
        local stream = FixedRecord:stream_string(STREAM_JSON)
        local rec = FixedRecord:new_record()
        local seen = 0
        while true do
          local obj, err, status = stream:next(rec)
          if status == "object" then
            if not assert_stream_fixed_record(obj) then
              stream:close()
              return false
            end
            seen = seen + 1
          elseif status == "eof" then
            break
          else
            stream:close()
            return false
          end
        end
        stream:close()
        return seen == 8
      end,
    }
  end

  do
    cases[#cases + 1] = {
      name = "stream/record_fixed_prepared/lua",
      group = "stream",
      sibling_name = "stream/object_fixed_prepared/lonejson",
      bytes_per_call = #STREAM_JSON,
      docs_per_call = 8,
      run_once = function()
        local stream = FixedRecord:stream_string(STREAM_JSON, { clear_destination = false })
        local rec = FixedRecord:new_record()
        local seen = 0
        while true do
          rec:clear()
          local obj, err, status = stream:next(rec)
          if status == "object" then
            if not assert_stream_fixed_record(obj) then
              stream:close()
              return false
            end
            seen = seen + 1
          elseif status == "eof" then
            break
          else
            stream:close()
            return false
          end
        end
        stream:close()
        return seen == 8
      end,
    }
  end

  do
    local rec = VendorLong:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_long_strings/lua",
      group = "stream",
      sibling_name = "stream/doc_long_strings/lonejson",
      bytes_per_call = #VENDOR_LONG_STRINGS_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLong:stream_string(VENDOR_LONG_STRINGS_JSON)
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and obj.id == "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      end,
    }
  end

  do
    local rec = VendorNumbers:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_extreme_numbers/lua",
      group = "stream",
      sibling_name = "stream/doc_extreme_numbers/lonejson",
      bytes_per_call = #VENDOR_EXTREME_NUMBERS_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorNumbers:stream_string(VENDOR_EXTREME_NUMBERS_JSON)
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and obj.min == -1.0e+28 and obj.max == 1.0e+28
      end,
    }
  end

  do
    local rec = VendorUnicode:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_string_unicode/lua",
      group = "stream",
      sibling_name = "stream/doc_string_unicode/lonejson",
      bytes_per_call = #VENDOR_STRING_UNICODE_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorUnicode:stream_string(VENDOR_STRING_UNICODE_JSON)
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and obj.title ~= nil and #obj.title > 0
      end,
    }
  end

  do
    local rec = VendorLanguage:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_japanese/lua",
      group = "stream",
      sibling_name = "stream/doc_japanese/lonejson",
      bytes_per_call = #JAPANESE_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLanguage:stream_string(JAPANESE_UTF8_JSON)
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and assert_vendor_language_table(obj, JAPANESE_UTF8_PREFIX)
      end,
    }
  end

  do
    local rec = VendorLanguageWide:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_japanese_wide/lua",
      group = "stream",
      sibling_name = "stream/doc_japanese_wide/lonejson",
      bytes_per_call = #JAPANESE_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLanguageWide:stream_string(JAPANESE_UTF8_WIDE_JSON)
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and assert_vendor_language_wide_table(obj,
          "ja", "japanese", "hojoki", JAPANESE_UTF8_PREFIX, JAPANESE_UTF8_PREFIX, 180)
      end,
    }
  end

  do
    local rec = VendorLanguageWide:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_hebrew_wide/lua",
      group = "stream",
      sibling_name = "stream/doc_hebrew_wide/lonejson",
      bytes_per_call = #HEBREW_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLanguageWide:stream_string(HEBREW_UTF8_WIDE_JSON)
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and assert_vendor_language_wide_table(obj,
          "he", "hebrew", "aesop_wikisource", "השועל מזמין את החסידה לביתו", HEBREW_UTF8_PREFIX, 220)
      end,
    }
  end

  do
    local rec = VendorLanguageWide:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_arabic_wide/lua",
      group = "stream",
      sibling_name = "stream/doc_arabic_wide/lonejson",
      bytes_per_call = #ARABIC_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLanguageWide:stream_string(ARABIC_UTF8_WIDE_JSON)
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and assert_vendor_language_wide_table(obj,
          "ar", "arabic", "kalila_dimna", "مقدمة تشرح سبب وضع كتاب كليلة", ARABIC_UTF8_PREFIX, 220)
      end,
    }
  end

  do
    local rec = VendorLanguage:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_hebrew/lua",
      group = "stream",
      sibling_name = "stream/doc_hebrew/lonejson",
      bytes_per_call = #HEBREW_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLanguage:stream_string(HEBREW_UTF8_JSON)
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and assert_vendor_language_table(obj, HEBREW_UTF8_PREFIX)
      end,
    }
  end

  do
    local rec = VendorLanguage:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_arabic/lua",
      group = "stream",
      sibling_name = "stream/doc_arabic/lonejson",
      bytes_per_call = #ARABIC_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLanguage:stream_string(ARABIC_UTF8_JSON)
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and assert_vendor_language_table(obj, ARABIC_UTF8_PREFIX)
      end,
    }
  end

  do
    cases[#cases + 1] = {
      name = "stream/lockdbench/lua",
      group = "stream",
      sibling_name = "stream/lockdbench/lonejson",
      bytes_per_call = #LOCKD_JSONL,
      docs_per_call = 5,
      run_once = function()
        local stream = LockdRun:stream_string(LOCKD_JSONL, { clear_destination = false })
        local rec = LockdRun:new_record()
        local ids = {
          "20260322T130525Z-b62770c-dirty-fast-disk",
          "20260322T141810Z-b62770c-dirty-fast-disk",
          "20260323T003016Z-08a87c0-clean-fast-disk",
          "20260325T004115Z-55f841b-dirty-fast-disk",
          "20260325T155329Z-e66dc79-dirty-fast-disk",
        }
        local count = 0
        while true do
          rec:clear()
          local obj, err, status = stream:next(rec)
          if status == "object" then
            count = count + 1
            if LOCKD_PATHS.run_id(rec) ~= ids[count] or LOCKD_PATHS.git_short_sha(rec) == nil or LOCKD_PATHS.git_dirty(rec) == nil or LOCKD_PATHS.config_concurrency(rec) ~= 8 or LOCKD_PATHS.cases_count(rec) ~= 9 then
              stream:close()
              return false
            end
            if LOCKD_PATHS.case1_workload(rec) ~= "attachments" or LOCKD_PATHS.case5_workload(rec) ~= "public-read" or LOCKD_PATHS.case9_workload(rec) ~= "xa-rollback" or LOCKD_PATHS.case1_acquire_ops(rec) ~= 4000 then
              stream:close()
              return false
            end
          elseif status == "eof" then
            break
          else
            stream:close()
            return false
          end
        end
        stream:close()
        return count == 5
      end,
    }
  end

  do
    local rec = FixedRecord:new_record()
    FixedRecord:assign(rec, build_serialize_source())
    cases[#cases + 1] = {
      name = "encode/record_buffer/lua",
      group = "encode",
      sibling_name = "serialize/buffer/lonejson",
      bytes_per_call = #EXPECTED_JSON,
      docs_per_call = 1,
      run_once = function()
        return FixedRecord:encode(rec) == EXPECTED_JSON
      end,
    }
    cases[#cases + 1] = {
      name = "encode/record_buffer_pretty/lua",
      group = "encode",
      sibling_name = "serialize/buffer_pretty/lonejson",
      bytes_per_call = #EXPECTED_PRETTY_JSON,
      docs_per_call = 1,
      run_once = function()
        return FixedRecord:encode(rec, { pretty = true }) == EXPECTED_PRETTY_JSON
      end,
    }
  end

  return cases
end

local function build_run(iterations, c_latest_path)
  local cases = bench_cases()
  local c_siblings = load_c_siblings(c_latest_path)
  local results = {}
  local i
  local now_ns = wall_clock_epoch_ns()

  for i = 1, #cases do
    local sample = measure_case(cases[i], iterations)
    local sibling_mib = c_siblings[cases[i].sibling_name]
    local result = {
      name = cases[i].name,
      group = cases[i].group,
      elapsed_ns = sample.elapsed_ns,
      total_bytes = sample.total_bytes,
      total_documents = sample.total_documents,
      mismatch_count = sample.mismatch_count,
      mib_per_sec = mib_per_sec(sample.total_bytes, sample.elapsed_ns),
      docs_per_sec = docs_per_sec(sample.total_documents, sample.elapsed_ns),
      ns_per_byte = ns_per_byte(sample.total_bytes, sample.elapsed_ns),
      sibling_name = cases[i].sibling_name,
    }
    if sibling_mib ~= nil and sibling_mib > 0.0 then
      result.sibling_mib_per_sec = sibling_mib
      result.sibling_ratio = result.mib_per_sec / sibling_mib
    end
    results[#results + 1] = result
  end

  return {
    schema_version = BENCH_SCHEMA_VERSION,
    timestamp_epoch_ns = now_ns,
    timestamp_utc = utc_timestamp_from_ns(now_ns),
    host = hostname(),
    lua_version = _VERSION,
    c_latest_path = portable_path(c_latest_path),
    iterations = iterations,
    results = results,
  }
end

local function run_single_case(case_name, iterations, c_latest_path)
  local cases = bench_cases()
  local c_siblings = load_c_siblings(c_latest_path)
  local i

  for i = 1, #cases do
    if cases[i].name == case_name then
      local sample = measure_case(cases[i], iterations)
      local sibling_mib = c_siblings[cases[i].sibling_name]
      local result = {
        name = cases[i].name,
        group = cases[i].group,
        elapsed_ns = sample.elapsed_ns,
        total_bytes = sample.total_bytes,
        total_documents = sample.total_documents,
        mismatch_count = sample.mismatch_count,
        mib_per_sec = mib_per_sec(sample.total_bytes, sample.elapsed_ns),
        docs_per_sec = docs_per_sec(sample.total_documents, sample.elapsed_ns),
        ns_per_byte = ns_per_byte(sample.total_bytes, sample.elapsed_ns),
        sibling_name = cases[i].sibling_name,
      }
      if sibling_mib ~= nil and sibling_mib > 0.0 then
        result.sibling_mib_per_sec = sibling_mib
        result.sibling_ratio = result.mib_per_sec / sibling_mib
      end
      print(string.format("%s %s %.3f MiB/s %.1f docs/s mismatch=%d c_sib=%s ratio=%s",
        result.name,
        result.group,
        result.mib_per_sec,
        result.docs_per_sec,
        result.mismatch_count,
        result.sibling_mib_per_sec and string.format("%.3f", result.sibling_mib_per_sec) or "-",
        result.sibling_ratio and string.format("%.3fx", result.sibling_ratio) or "-"))
      return
    end
  end
  error("unknown benchmark case: " .. case_name)
end

local function print_run(run)
  local i
  print(string.format("lua benchmark run %s on %s", run.timestamp_utc, run.host))
  print(string.format("%-34s %-10s %12s %12s %12s %12s",
    "name", "group", "MiB/s", "docs/s", "C sib", "L/C"))
  for i = 1, #run.results do
    local r = run.results[i]
    print(string.format("%-34s %-10s %12.3f %12.1f %12s %12s",
      r.name,
      r.group,
      r.mib_per_sec,
      r.docs_per_sec,
      r.sibling_mib_per_sec and string.format("%.3f", r.sibling_mib_per_sec) or "-",
      r.sibling_ratio and string.format("%.3fx", r.sibling_ratio) or "-"))
  end
end

local function write_outputs(run, latest_path, history_path, archive_dir)
  local payload = BenchRunSchema:encode(run)
  local ok
  ensure_dir(archive_dir)
  ensure_dir(string.match(latest_path, "(.+)/[^/]+$"))
  ensure_dir(string.match(history_path, "(.+)/[^/]+$"))
  ok = BenchRunSchema:write_path(run, latest_path)
  assert(ok == true, type(ok) == "table" and ok.message or "failed to write latest benchmark output")
  append_file(history_path, payload)
  ok = BenchRunSchema:write_path(run, string.format("%s/%d.json", archive_dir, run.timestamp_epoch_ns))
  assert(ok == true, type(ok) == "table" and ok.message or "failed to write archived benchmark output")
end

local function freeze_baseline(history_path, baseline_path)
  local f = assert(io.open(history_path, "rb"))
  local last
  for line in f:lines() do
    if line ~= "" then
      last = line
    end
  end
  f:close()
  assert(last ~= nil, "history is empty")
  ensure_dir(string.match(baseline_path, "(.+)/[^/]+$"))
  local out = assert(io.open(baseline_path, "wb"))
  assert(out:write(last))
  out:close()
end

local function index_results(run)
  local out = {}
  local i
  for i = 1, #run.results do
    out[run.results[i].name] = run.results[i]
  end
  return out
end

local function compare_runs(baseline_path, latest_path)
  local baseline = BenchRunSchema:decode_path(baseline_path)
  local latest = BenchRunSchema:decode_path(latest_path)
  local baseline_index = index_results(baseline)
  local i

  print(string.format("lua compare baseline=%s latest=%s", baseline_path, latest_path))
  print(string.format("%-34s %12s %12s %10s %-10s %12s %12s",
    "name", "latest", "baseline", "delta", "class", "C sib", "L/C"))
  for i = 1, #latest.results do
    local current = latest.results[i]
    local prior = baseline_index[current.name]
    local delta_pct
    local cls
    if prior ~= nil and prior.mib_per_sec ~= 0.0 then
      delta_pct = ((current.mib_per_sec - prior.mib_per_sec) / prior.mib_per_sec) * 100.0
      cls = classify_delta(delta_pct)
    else
      delta_pct = 0.0
      cls = "new"
    end
    print(string.format("%-34s %12.3f %12s %9.2f%% %-10s %12s %12s",
      current.name,
      current.mib_per_sec,
      prior and string.format("%.3f", prior.mib_per_sec) or "-",
      delta_pct,
      cls,
      current.sibling_mib_per_sec and string.format("%.3f", current.sibling_mib_per_sec) or "-",
      current.sibling_ratio and string.format("%.3fx", current.sibling_ratio) or "-"))
  end
end

local function is_material_regression(prior, current)
  local delta_pct
  if prior == nil or prior.mib_per_sec == 0.0 then
    return false
  end
  if current.mib_per_sec >= prior.mib_per_sec then
    return false
  end
  delta_pct = ((current.mib_per_sec - prior.mib_per_sec) / prior.mib_per_sec) * 100.0
  return classify_delta(delta_pct) == "material"
end

local function gate_tracked_result(current)
  return not current.name:match("/cjson$")
end

local function gate_runs(baseline_path, latest_path)
  local baseline = BenchRunSchema:decode_path(baseline_path)
  local latest = BenchRunSchema:decode_path(latest_path)
  local baseline_index = index_results(baseline)
  local schema_mismatch = baseline.schema_version ~= latest.schema_version
  local missing = 0
  local material = 0
  local i

  for i = 1, #latest.results do
    local current = latest.results[i]
    local prior = baseline_index[current.name]
    if not gate_tracked_result(current) then
      -- Ignore non-lonejson sibling/reference lanes in the hard gate.
    elseif prior == nil then
      missing = missing + 1
    elseif is_material_regression(prior, current) then
      material = material + 1
    end
  end

  print(string.format("lua gate baseline=%s latest=%s", baseline_path, latest_path))
  print(string.format("  schema mismatch: %s", schema_mismatch and "yes" or "no"))
  print(string.format("  missing results: %d", missing))
  print(string.format("  material regressions: %d", material))

  if schema_mismatch or missing ~= 0 or material ~= 0 then
    io.stderr:write("lua benchmark gate failed: refresh the baseline for intentional benchmark-schema changes or fix the regressions before landing\n")
    os.exit(1)
  end
end

local function usage()
  io.stderr:write("usage:\n")
  io.stderr:write("  lua bench/lonejson_lua_bench.lua run <c_latest> <latest> <history> <archive_dir> <iterations>\n")
  io.stderr:write("  lua bench/lonejson_lua_bench.lua case <c_latest> <case_name> <iterations>\n")
  io.stderr:write("  lua bench/lonejson_lua_bench.lua freeze-baseline <history> <baseline>\n")
  io.stderr:write("  lua bench/lonejson_lua_bench.lua compare <baseline> <latest>\n")
  io.stderr:write("  lua bench/lonejson_lua_bench.lua gate <baseline> <latest>\n")
  os.exit(1)
end

local cmd = arg[1]

if cmd == "run" then
  local c_latest = arg[2]
  local latest = arg[3]
  local history = arg[4]
  local archive_dir = arg[5]
  local iterations = tonumber(arg[6]) or 1
  local run = build_run(iterations, c_latest)
  print_run(run)
  write_outputs(run, latest, history, archive_dir)
elseif cmd == "freeze-baseline" then
  freeze_baseline(arg[2], arg[3])
elseif cmd == "case" then
  run_single_case(arg[3], tonumber(arg[4]) or 1, arg[2])
elseif cmd == "compare" then
  compare_runs(arg[2], arg[3])
elseif cmd == "gate" then
  gate_runs(arg[2], arg[3])
else
  usage()
end
