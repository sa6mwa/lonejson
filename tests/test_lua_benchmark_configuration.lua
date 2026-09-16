local lonejson = require('lonejson')
for name in pairs(lonejson.core) do
  assert(not name:match('^_test_'), 'benchmark module exposes test instrumentation: ' .. name)
end
assert(lonejson.new():encode_json({ok = true}) == '{"ok":true}')
