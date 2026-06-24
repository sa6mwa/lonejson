local lonejson = require("lonejson")
local lj = lonejson.new()
local Schema = lj.schema("CacheableNested", {
  lj.field("payload", lj.object {
    fields = {
      lj.field("name", lj.string { fixed_capacity = 32, overflow = "fail" }),
      lj.field("count", lj.i64()),
    },
  }),
})
local json = [[{"payload":{"name":"alpha","count":3}}]]

assert(type(lj.core._test_reset_map_analysis_count) == "function")
assert(type(lj.core._test_get_map_analysis_count) == "function")
assert(type(lj.core._test_map_cache_enabled) == "function")
assert(type(lj.core._test_schema_map_cacheable) == "function")
assert(type(lj.core._test_schema_map_cookie) == "function")
assert(lj.core._test_map_cache_enabled())
assert(lj.core._test_schema_map_cacheable(Schema))

local Schema2 = lj.schema("CacheableNested2", {
  lj.field("payload", lj.object {
    fields = {
      lj.field("name", lj.string { fixed_capacity = 32, overflow = "fail" }),
      lj.field("count", lj.i64()),
    },
  }),
})
assert(lj.core._test_schema_map_cookie(Schema) ~=
       lj.core._test_schema_map_cookie(Schema2))

lj.core._test_reset_map_analysis_count()
local first = Schema:decode(json)
assert(first.payload.name == "alpha")
local count_after_first = lj.core._test_get_map_analysis_count()
assert(count_after_first == 0)

local second = Schema:decode(json)
assert(second.payload.name == "alpha")
local count_after_second = lj.core._test_get_map_analysis_count()
assert(count_after_second == count_after_first,
       string.format("expected stable analysis counter facade, got %d then %d",
                     count_after_first, count_after_second))
