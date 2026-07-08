# Local Verification

## Debug Gate

Use `make test` as the debug lifecycle gate.

`make test` is intentionally broader than raw CTest: it configures and builds
the debug tree, stages standalone examples, runs the debug CTest preset, and
then runs the Lua integration test through `make lua-test`.

Do not use `ctest --preset debug` as a completion, pre-release, release, or
status gate. Raw CTest does not configure or build the tree, so it can report
results from stale or manually reconfigured artifacts. That is diagnostic
output, not lifecycle evidence.

Raw CTest is acceptable only for focused diagnosis after the matching debug
configuration and build have already been run in the same flow. Reports that
use raw CTest as evidence must include the configure/build command that made
the artifacts current, for example:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

For normal local confidence and completion reports, use:

```sh
make test
```

For a focused test while iterating on a narrow change, build the required
targets first and state that the result is focused diagnostic coverage, not the
debug lifecycle gate:

```sh
cmake --build --preset debug --target lonejson_tests
ctest --test-dir build/debug -R '<test-regex>' --output-on-failure
```
