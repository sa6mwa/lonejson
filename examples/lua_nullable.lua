local lj = require("lonejson").new()

local JobStatus = lj.schema("JobStatus", {
  lj.field("name", lj.string { required = true }),
  lj.field("attempts", lj.u64 { nullable = true }),
  lj.field("score", lj.f64 { nullable = true }),
  lj.field("enabled", lj.bool { nullable = true }),
})

local queued = JobStatus:decode('{"name":"queued","attempts":null,"enabled":true}')
print("queued attempts=" .. tostring(queued.attempts) ..
      " enabled=" .. tostring(queued.enabled))

local fresh = JobStatus:decode('{"name":"new"}')
print("new attempts=" .. tostring(fresh.attempts) ..
      " score=" .. tostring(fresh.score) ..
      " enabled=" .. tostring(fresh.enabled))

local encoded = JobStatus:encode({
  name = "retrying",
  attempts = 3,
  score = lj.json_null,
  enabled = false,
})

print(encoded)
