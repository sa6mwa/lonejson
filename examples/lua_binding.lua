local lj = require("lonejson")

local Example = lj.schema("Example", {
  lj.field("id", lj.i64 { required = true }),
  lj.field("name", lj.string { required = true, fixed_capacity = 64, overflow = "fail" }),
  lj.field("body", lj.spooled_text { memory_limit = 96, temp_dir = "/tmp" }),
  lj.field("payload", lj.spooled_bytes { memory_limit = 96, temp_dir = "/tmp" }),
})

local function spool_summary(label, spool)
  local path

  path = spool:path()
  print(label .. ": size=" .. spool:size() ..
        " spilled=" .. tostring(spool:spilled()) ..
        " path=" .. tostring(path))
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

do
  local rec
  local jsonl_path
  local stream
  local obj
  local err
  local status
  local body_path
  local payload_path

  rec = Example:new_record()
  Example:assign(rec, {
    id = 1,
    name = "spooled",
    body = ("lonejson-text-"):rep(20),
    payload = string.char(0, 1, 2, 3, 4, 5, 6, 7) .. ("ABC"):rep(40),
  })
  print("encoded json bytes=" .. #Example:encode(rec))
  spool_summary("body", rec.body)
  spool_summary("payload", rec.payload)
  body_path = rec.body:path()
  payload_path = rec.payload:path()
  print("body temp exists before clear=" .. tostring(exists(body_path)))
  print("payload temp exists before clear=" .. tostring(exists(payload_path)))
  rec:clear()
  print("body temp exists after clear=" .. tostring(exists(body_path)))
  print("payload temp exists after clear=" .. tostring(exists(payload_path)))

  jsonl_path = "/tmp/lonejson-lua-example.jsonl"
  assert(Example:write_path(rec, "/tmp/lonejson-lua-example-single.json"))
  do
    local f = assert(io.open(jsonl_path, "wb"))
    f:write('{"id":2,"name":"first"}')
    f:write('{"id":3,"name":"second","body":"')
    f:write(("streamed-"):rep(18))
    f:write('"}')
    f:close()
  end

  stream = Example:stream_path(jsonl_path)
  while true do
    obj, err, status = stream:next(rec)
    if status == "object" then
      print("stream object id=" .. rec.id .. " name=" .. rec.name)
      if rec.body:size() ~= 0 then
        spool_summary("stream body", rec.body)
      end
    elseif status == "eof" then
      break
    else
      error(err and err.message or "stream failed")
    end
  end
  stream:close()
  os.remove("/tmp/lonejson-lua-example-single.json")
  os.remove(jsonl_path)
end
