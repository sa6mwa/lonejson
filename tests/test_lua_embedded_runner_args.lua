-- The runner's `--` form is used by benchmark and maintenance commands.
assert(arg[0]:match("test_lua_embedded_runner_args%.lua$") ~= nil)
assert(arg[1] == "runner-argument")
