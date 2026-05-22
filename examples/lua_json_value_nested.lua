local lj = require("lonejson")

local Response = lj.schema("Response", {
  lj.field("type", lj.string { required = true, fixed_capacity = 32, overflow = "fail" }),
  lj.field("response", lj.object {
    required = true,
    fields = {
      lj.field("status", lj.string()),
      lj.field("payload", lj.json_value { required = true }),
    },
  }),
})

local rec = Response:new_record()

Response:decode_into(rec, [[
  {"type":"response.completed","response":{"status":"ok","payload":{"a":[1,2,3],"mode":"full"}}}
]])
print("first type=" .. rec.type ..
      " status=" .. tostring(rec.response.status) ..
      " payload.mode=" .. tostring(rec.response.payload.mode))

Response:decode_into(rec, [[
  {"type":"response.delta","response":{"payload":{"delta":[true,null,2]}}}
]], { clear_destination = false })
print("second type=" .. rec.type ..
      " status=" .. tostring(rec.response.status) ..
      " payload.delta[2]=json_null:" ..
      tostring(rec.response.payload.delta[2] == lj.json_null))
