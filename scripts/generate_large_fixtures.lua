#!/usr/bin/env lua

local GENERATOR_VERSION = "lonejson-large-fixtures-v4"

local SMALL_DOC_NAMES = {
  "profile_compact.json",
  "profile_pretty.json",
  "profile_custom.json",
}

local LARGE_SPECS = {
  { name = "large_object_compact.json", bytes = 24 * 1024 * 1024 },
  { name = "large_object_pretty.json", bytes = 18 * 1024 * 1024 },
  { name = "large_array_objects.json", bytes = 20 * 1024 * 1024 },
  { name = "large_base64_payloads.json", bytes = 18 * 1024 * 1024 },
  { name = "large_stream.jsonl", bytes = 20 * 1024 * 1024 },
}

local function usage()
  io.stderr:write("usage: generate_large_fixtures.lua <output-dir>\n")
  os.exit(1)
end

if #arg ~= 1 then
  usage()
end

local output_dir = arg[1]

local function mkdir_p(path)
  local ok = os.execute("mkdir -p " .. string.format("%q", path))
  if ok ~= true and ok ~= 0 then
    io.stderr:write("failed to create " .. path .. "\n")
    os.exit(1)
  end
end

local function file_size(path)
  local fp = io.open(path, "rb")
  local size

  if fp == nil then
    return nil
  end
  size = fp:seek("end")
  fp:close()
  return size
end

local function join_path(a, b)
  if string.sub(a, -1) == "/" then
    return a .. b
  end
  return a .. "/" .. b
end

local function repeat_unit(unit, count)
  local parts = {}
  local i

  for i = 1, count do
    parts[i] = unit
  end
  return table.concat(parts)
end

local function write_all(fp, text, state)
  fp:write(text)
  state.bytes = state.bytes + #text
end

local function write_repeated_chars(fp, ch, count, state)
  local chunk
  local remaining

  if count <= 0 then
    return
  end
  chunk = string.rep(ch, math.min(count, 65536))
  remaining = count
  while remaining > 0 do
    local take = math.min(remaining, #chunk)
    fp:write(string.sub(chunk, 1, take))
    state.bytes = state.bytes + take
    remaining = remaining - take
  end
end

local function append_small_string_array(fp, state, target_bytes, prefix, suffix, label_prefix, value_size)
  local index = 1
  local entry

  write_all(fp, prefix, state)
  while true do
    local remaining = target_bytes - state.bytes - #suffix
    if remaining <= 4 then
      break
    end
    entry = "\"" .. label_prefix .. tostring(index) .. "-" ..
            string.rep("x", value_size) .. "\""
    if index ~= 1 then
      entry = "," .. entry
    end
    if #entry > remaining then
      local payload_len = remaining - #label_prefix - #tostring(index) - 4
      if index ~= 1 then
        payload_len = payload_len - 1
      end
      if payload_len <= 0 then
        break
      end
      entry = "\"" .. label_prefix .. tostring(index) .. "-" ..
              string.rep("x", payload_len) .. "\""
      if index ~= 1 then
        entry = "," .. entry
      end
      if #entry > remaining then
        break
      end
    end
    write_all(fp, entry, state)
    index = index + 1
  end
  write_all(fp, suffix, state)
end

local function profile_document()
  local doc = {
    name = "Fixture Person Example With A Long Name",
    nickname = "fixture-user",
    age = 57,
    score = 4321.125,
    active = true,
    address_city = "Stockholm",
    address_zip = 11223344,
    lucky_numbers = { 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233 },
    tags = { "alpha", "beta", "gamma", "delta", "epsilon",
             "zeta", "eta", "theta", "iota", "kappa" },
    items = {
      { 101, "first-entry" },
      { 202, "second-entry" },
      { 303, "third-entry" },
      { 404, "fourth-entry" },
      { 505, "fifth-entry" },
    },
    description =
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt "
      .. "ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
      .. "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in "
      .. "voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat "
      .. "non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.",
  }
  return doc
end

local function write_profile_compact(path)
  local doc = profile_document()
  local fp = assert(io.open(path, "wb"))
  local i

  fp:write("{")
  fp:write("\"name\":\"", doc.name, "\",")
  fp:write("\"nickname\":\"fixture\",")
  fp:write("\"age\":", tostring(doc.age), ",")
  fp:write("\"score\":4321.125,")
  fp:write("\"active\":true,")
  fp:write("\"address\":{\"city\":\"", doc.address_city, "\",\"zip\":", tostring(doc.address_zip), "},")
  fp:write("\"lucky_numbers\":[")
  for i = 1, 8 do
    local j
    for j = 1, #doc.lucky_numbers do
      if i ~= 1 or j ~= 1 then
        fp:write(",")
      end
      fp:write(tostring(doc.lucky_numbers[j]))
    end
  end
  fp:write("],\"tags\":[")
  for i = 1, #doc.tags do
    if i ~= 1 then
      fp:write(",")
    end
    fp:write("\"", doc.tags[i], "\"")
  end
  fp:write("],\"items\":[")
  for i = 1, #doc.items do
    if i ~= 1 then
      fp:write(",")
    end
    fp:write("{\"id\":", tostring(doc.items[i][1]), ",\"label\":\"", doc.items[i][2], "\"}")
  end
  fp:write("],\"description\":\"", doc.description, "\"}")
  fp:close()
end

local function write_profile_pretty(path)
  local doc = profile_document()
  local fp = assert(io.open(path, "wb"))
  local i

  fp:write("{\n")
  fp:write("  \"name\": \"", doc.name, "\",\n")
  fp:write("  \"nickname\": \"fixture\",\n")
  fp:write("  \"age\": ", tostring(doc.age), ",\n")
  fp:write("  \"score\": 4321.125,\n")
  fp:write("  \"active\": true,\n")
  fp:write("  \"address\": {\n")
  fp:write("    \"city\": \"", doc.address_city, "\",\n")
  fp:write("    \"zip\": ", tostring(doc.address_zip), "\n")
  fp:write("  },\n")
  fp:write("  \"lucky_numbers\": [\n")
  for i = 1, 96 do
    fp:write("    ", tostring(doc.lucky_numbers[((i - 1) % #doc.lucky_numbers) + 1]))
    if i ~= 96 then
      fp:write(",")
    end
    fp:write("\n")
  end
  fp:write("  ],\n")
  fp:write("  \"tags\": [\n")
  for i = 1, #doc.tags do
    fp:write("    \"", doc.tags[i], "\"")
    if i ~= #doc.tags then
      fp:write(",")
    end
    fp:write("\n")
  end
  fp:write("  ],\n")
  fp:write("  \"items\": [\n")
  for i = 1, #doc.items do
    fp:write("    {\"id\": ", tostring(doc.items[i][1]), ", \"label\": \"", doc.items[i][2], "\"}")
    if i ~= #doc.items then
      fp:write(",")
    end
    fp:write("\n")
  end
  fp:write("  ],\n")
  fp:write("  \"description\": \"", doc.description, "\"\n")
  fp:write("}\n")
  fp:close()
end

local function write_profile_custom(path)
  local doc = profile_document()
  local fp = assert(io.open(path, "wb"))

  fp:write("{ \"name\" : \"", doc.name, "\" , \"nickname\" : \"fixture\" , \"age\" : ",
           tostring(doc.age), " , \"score\" : 4321.125 , \"active\" : true , ")
  fp:write("\"address\" : { \"city\" : \"", doc.address_city, "\" , \"zip\" : ",
           tostring(doc.address_zip), " } , ")
  fp:write("\"lucky_numbers\" : [ 1 , 2 , 3 , 5 , 8 , 13 , 21 , 34 , 55 , 89 , 144 , 233")
  fp:write(repeat_unit(" , 1 , 2 , 3 , 5 , 8 , 13 , 21 , 34 , 55 , 89 , 144 , 233", 7))
  fp:write(" ] , ")
  fp:write("\"tags\" : [ \"alpha\" , \"beta\" , \"gamma\" , \"delta\" , \"epsilon\" , \"zeta\" , \"eta\" , \"theta\" , \"iota\" , \"kappa\" ] , ")
  fp:write("\"items\" : [ { \"id\" : 101 , \"label\" : \"first-entry\" } , { \"id\" : 202 , \"label\" : \"second-entry\" } , { \"id\" : 303 , \"label\" : \"third-entry\" } , { \"id\" : 404 , \"label\" : \"fourth-entry\" } , { \"id\" : 505 , \"label\" : \"fifth-entry\" } ] , ")
  fp:write("\"description\" : \"", doc.description, "\" }\n")
  fp:close()
end

local function write_large_object_compact(path, target_bytes)
  local fp = assert(io.open(path, "wb"))
  local state = { bytes = 0 }

  append_small_string_array(
      fp, state, target_bytes,
      "{\"kind\":\"large-object-compact\",\"version\":1,\"metadata\":{\"dataset\":\"lonejson-large\",\"shape\":\"compact-object\"},\"chunks\":[",
      "]}\n", "chunk-", 768)
  fp:close()
end

local function write_large_object_pretty(path, target_bytes)
  local fp = assert(io.open(path, "wb"))
  local state = { bytes = 0 }
  local i

  write_all(fp, "{\n  \"kind\": \"large-object-pretty\",\n  \"version\": 1,\n  \"documents\": [\n", state)
  i = 1
  while true do
    local entry = string.format(
        "    {\"id\": %d, \"name\": \"record-%06d\", \"active\": %s, \"score\": %.3f, \"city\": \"Stockholm\", \"note\": \"%s\"}",
        i, i, (i % 2 == 0) and "true" or "false", (i % 1000) / 10.0,
        "pretty formatted record payload for large fixture generation and parser validation")
    local remaining = target_bytes - state.bytes - #"\n  ]\n}\n"
    if i ~= 1 then
      entry = ",\n" .. entry
    end
    if #entry > remaining then
      local payload_len = remaining - 96
      if i ~= 1 then
        payload_len = payload_len - 2
      end
      if payload_len <= 0 then
        break
      end
      entry = string.format(
          "    {\"id\": %d, \"name\": \"record-%06d\", \"active\": %s, \"score\": %.3f, \"city\": \"Stockholm\", \"note\": \"%s\"}",
          i, i, (i % 2 == 0) and "true" or "false", (i % 1000) / 10.0,
          string.rep("p", payload_len))
      if i ~= 1 then
        entry = ",\n" .. entry
      end
      if #entry > remaining then
        break
      end
    end
    write_all(fp, entry, state)
    i = i + 1
  end
  write_all(fp, "\n  ]\n}\n", state)
  fp:close()
end

local function write_large_array_objects(path, target_bytes)
  local fp = assert(io.open(path, "wb"))
  local state = { bytes = 0 }
  local i

  write_all(fp, "[", state)
  i = 1
  while true do
    local entry = string.format(
        "{\"id\":%d,\"name\":\"obj-%06d\",\"payload\":\"normal text payload for large object arrays and parser validation\",\"byte_run\":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15],\"nested\":{\"enabled\":%s,\"label\":\"group-%03d\"}}",
        i, i, (i % 2 == 0) and "true" or "false", i % 128)
    local remaining = target_bytes - state.bytes - #"]\n"
    if i ~= 1 then
      entry = "," .. entry
    end
    if #entry > remaining then
      local payload_len = remaining - 128
      if i ~= 1 then
        payload_len = payload_len - 1
      end
      if payload_len <= 0 then
        break
      end
      entry = string.format(
          "{\"id\":%d,\"name\":\"obj-%06d\",\"payload\":\"%s\",\"byte_run\":[0,1,2,3,4,5,6,7,8],\"nested\":{\"enabled\":true,\"label\":\"tail\"}}",
          i, i, string.rep("o", payload_len))
      if i ~= 1 then
        entry = "," .. entry
      end
      if #entry > remaining then
        break
      end
    end
    write_all(fp, entry, state)
    i = i + 1
  end
  write_all(fp, "]\n", state)
  fp:close()
end

local function write_large_base64_payloads(path, target_bytes)
  local fp = assert(io.open(path, "wb"))
  local state = { bytes = 0 }
  local i

  write_all(fp, "{\"kind\":\"large-base64-payloads\",\"payloads\":[", state)
  i = 1
  while true do
    local entry = "{\"id\":" .. tostring(i) ..
                  ",\"encoding\":\"base64\",\"data\":\"" ..
                  string.rep("Q", 1024) .. "\"}"
    local remaining = target_bytes - state.bytes - #"]}\n"
    if i ~= 1 then
      entry = "," .. entry
    end
    if #entry > remaining then
      local payload_len = remaining - 32
      if i ~= 1 then
        payload_len = payload_len - 1
      end
      if payload_len <= 0 then
        break
      end
      entry = "{\"id\":" .. tostring(i) ..
              ",\"encoding\":\"base64\",\"data\":\"" ..
              string.rep("Q", payload_len) .. "\"}"
      if i ~= 1 then
        entry = "," .. entry
      end
      if #entry > remaining then
        break
      end
    end
    write_all(fp, entry, state)
    i = i + 1
  end
  write_all(fp, "]}\n", state)
  fp:close()
end

local function write_large_stream(path, target_bytes)
  local fp = assert(io.open(path, "wb"))
  local bytes = 0
  local index = 1

  while bytes < target_bytes do
    local line = string.format(
        "{\"id\":%d,\"name\":\"stream-%06d\",\"kind\":\"jsonl\",\"payload\":\"normal sized stream object payload\",\"base64\":\"%s\",\"values\":[1,2,3,4,5,6,7,8],\"meta\":{\"shard\":%d,\"ok\":%s}}\n",
        index, index, string.rep("Z", 512), index % 32,
        (index % 2 == 0) and "true" or "false")
    if bytes + #line > target_bytes then
      local filler = target_bytes - bytes - 128
      if filler < 16 then
        break
      end
      filler = math.floor(filler / 2)
      line = string.format(
          "{\"id\":%d,\"name\":\"stream-%06d\",\"kind\":\"jsonl\",\"payload\":\"%s\",\"base64\":\"%s\",\"values\":[1],\"meta\":{\"shard\":%d,\"ok\":true}}\n",
          index, index, string.rep("s", math.min(filler, 1024)),
          string.rep("Z", math.min(filler, 1024)), index % 32)
      if bytes + #line > target_bytes then
        break
      end
    end
    fp:write(line)
    bytes = bytes + #line
    index = index + 1
  end
  fp:close()
end

local function expected_manifest()
  local lines = { "version=" .. GENERATOR_VERSION }
  local i

  for i = 1, #SMALL_DOC_NAMES do
    lines[#lines + 1] = "small:" .. SMALL_DOC_NAMES[i]
  end
  for i = 1, #LARGE_SPECS do
    lines[#lines + 1] = LARGE_SPECS[i].name .. "=" .. tostring(LARGE_SPECS[i].bytes)
  end
  return table.concat(lines, "\n") .. "\n"
end

local function manifest_matches(path)
  local fp = io.open(path, "rb")
  local content

  if fp == nil then
    return false
  end
  content = fp:read("*a")
  fp:close()
  return content == expected_manifest()
end

local function fixtures_match_manifest(dir)
  local i

  if not manifest_matches(join_path(dir, "manifest.txt")) then
    return false
  end
  for i = 1, #SMALL_DOC_NAMES do
    if file_size(join_path(dir, SMALL_DOC_NAMES[i])) == nil then
      return false
    end
  end
  for i = 1, #LARGE_SPECS do
    local actual = file_size(join_path(dir, LARGE_SPECS[i].name))
    if actual == nil or actual ~= LARGE_SPECS[i].bytes then
      return false
    end
  end
  return true
end

local function write_manifest(dir)
  local fp = assert(io.open(join_path(dir, "manifest.txt"), "wb"))
  fp:write(expected_manifest())
  fp:close()
end

mkdir_p(output_dir)
if fixtures_match_manifest(output_dir) then
  os.exit(0)
end

write_profile_compact(join_path(output_dir, "profile_compact.json"))
write_profile_pretty(join_path(output_dir, "profile_pretty.json"))
write_profile_custom(join_path(output_dir, "profile_custom.json"))
write_large_object_compact(join_path(output_dir, LARGE_SPECS[1].name), LARGE_SPECS[1].bytes)
write_large_object_pretty(join_path(output_dir, LARGE_SPECS[2].name), LARGE_SPECS[2].bytes)
write_large_array_objects(join_path(output_dir, LARGE_SPECS[3].name), LARGE_SPECS[3].bytes)
write_large_base64_payloads(join_path(output_dir, LARGE_SPECS[4].name), LARGE_SPECS[4].bytes)
write_large_stream(join_path(output_dir, LARGE_SPECS[5].name), LARGE_SPECS[5].bytes)
write_manifest(output_dir)
