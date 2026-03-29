#!/usr/bin/env lua

local function usage()
  io.stderr:write("usage: report_stack_usage.lua <build-dir>\n")
  os.exit(1)
end

if #arg ~= 1 then
  usage()
end

local root = arg[1]
local public_prefixes = {
  "lonejson_parse_",
  "lonejson_validate_",
  "lonejson_serialize_",
  "lonejson_stream_",
  "lonejson_cleanup",
  "lonejson_reset",
}

local function split_tabs(line)
  local parts = {}
  for part in string.gmatch(line, "([^\t]+)") do
    parts[#parts + 1] = part
  end
  return parts
end

local function function_name(location)
  return string.match(location, "([^:]+)$") or location
end

local function starts_with_any(name, prefixes)
  local i
  for i = 1, #prefixes do
    if string.sub(name, 1, #prefixes[i]) == prefixes[i] then
      return true
    end
  end
  return false
end

local function path_join(a, b)
  if string.sub(a, -1) == "/" then
    return a .. b
  end
  return a .. "/" .. b
end

local function relative_path(path, prefix)
  if string.sub(path, 1, #prefix) == prefix then
    local rel = string.sub(path, #prefix + 1)
    if string.sub(rel, 1, 1) == "/" then
      rel = string.sub(rel, 2)
    end
    return rel
  end
  return path
end

local function sorted_lines_from_find(root_dir)
  local cmd = "find " .. string.format("%q", root_dir) ..
                  " -type f -name '*.su' | LC_ALL=C sort"
  local pipe = io.popen(cmd, "r")
  local lines = {}
  local line

  if pipe == nil then
    return nil
  end
  for current in pipe:lines() do
    lines[#lines + 1] = current
  end
  pipe:close()
  return lines
end

local su_files = sorted_lines_from_find(root)
local reports = {}
local max_by_function = {}

if su_files == nil then
  io.stderr:write("failed to enumerate .su files under " .. root .. "\n")
  os.exit(1)
end

for _, path in ipairs(su_files) do
  local fp = io.open(path, "r")
  if fp ~= nil then
    for line in fp:lines() do
      local parts = split_tabs(line)
      local usage_value = tonumber(parts[2] or "")
      if usage_value ~= nil and parts[1] ~= nil and parts[3] ~= nil then
        local location = parts[1]
        local qualifier = parts[3]
        local name = function_name(location)
        reports[#reports + 1] = {
          usage = usage_value,
          path = path,
          location = location,
          qualifier = qualifier,
          name = name,
        }
        if max_by_function[name] == nil or usage_value > max_by_function[name].usage then
          max_by_function[name] = reports[#reports]
        end
      end
    end
    fp:close()
  end
end

if #reports == 0 then
  io.stderr:write("no .su stack-usage files found under " .. root .. "\n")
  os.exit(1)
end

table.sort(reports, function(a, b)
  return a.usage > b.usage
end)

local public_reports = {}
for _, item in pairs(max_by_function) do
  if starts_with_any(item.name, public_prefixes) then
    public_reports[#public_reports + 1] = item
  end
end

table.sort(public_reports, function(a, b)
  return a.usage > b.usage
end)

if #public_reports > 0 then
  print("public API stack usage summary (max bytes per function):")
  for _, item in ipairs(public_reports) do
    print(string.format("%6d  %s  [%s]  (%s)", item.usage, item.name,
                        item.qualifier, relative_path(item.path, root)))
  end
  print("")
end

print("stack usage report (bytes):")
for _, item in ipairs(reports) do
  print(string.format("%6d  %s  [%s]  (%s)", item.usage, item.location,
                      item.qualifier, relative_path(item.path, root)))
end
