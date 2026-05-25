local lj = require("lonejson").new({
  clear_destination_by_default = false,
})

local Batch = lj.schema("Batch", {
  lj.field("batch_id", lj.string { required = true }),
  lj.field("items", lj.object_array {
    fields = {
      lj.field("id", lj.string { required = true }),
      lj.field("payload", lj.json_value { required = true }),
      lj.field("tags", lj.json_value()),
    },
  }),
})

local rec = Batch:new_record()

Batch:decode_into(rec, [[
  {
    "batch_id":"b1",
    "items":[
      {"id":"i1","payload":{"kind":"full","count":2},"tags":["a","b"]},
      {"id":"i2","payload":{"kind":"delta","ops":[1,null,true]}}
    ]
  }
]])

print("first batch=" .. rec.batch_id ..
      " items=" .. tostring(#rec.items) ..
      " first.kind=" .. tostring(rec.items[1].payload.kind))

Batch:decode_into(rec, [[
  {
    "batch_id":"b2",
    "items":[
      {"id":"i3","payload":{"kind":"replace","ok":true},"tags":{"scope":"fresh"}}
    ]
  }
]])

print("second batch=" .. rec.batch_id ..
      " items=" .. tostring(#rec.items) ..
      " only.id=" .. tostring(rec.items[1].id) ..
      " tags.scope=" .. tostring(rec.items[1].tags.scope))
