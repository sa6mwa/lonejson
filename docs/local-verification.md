# Local Verification

## Compiler and Cross Toolchains

CMake selects the pinned Bootlin x86_64 GNU collection by default. Linux
target toolchain files select their matching pinned Bootlin collection. Before
a Linux cross or release build, install the collections into the lifecycle
cache:

```sh
make toolchains-all
```

The default cache is
`${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/toolchains`; set
`CPKT_TOOLCHAIN_CACHE` to use another shared cache location.

Every native debug, host, and Linux release build uses a single pinned Bootlin collection:
its GCC driver, GNU linker, binutils/debug tools, libc sysroot, and target
runtime. Configuration verifies the compiler triple; package inspection reads
the configured target tools from the same collection. Native memory checking
uses host-installed Valgrind; fuzzing uses a cached, pinned AFL++ GCC-plugin
build tied to the Bootlin x86_64 collection. Valgrind is not a
compiler-based uninitialized-memory analysis, but it provides the native leak
and invalid-memory gate without a non-portable LLVM distribution.

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

## Broader Confidence

`make test-all` is the deterministic broad local confidence gate. It runs the
debug gate, host release tests, curl/auth host tests unless explicitly skipped
by the release pipeline, cross target tests, host sanitizers where supported,
Valgrind, deterministic local e2e, and fuzz smoke.

Benchmark gates are intentionally not part of `test-all` or the normal
`prerelease` graph. Run `make bench-check`, `make bench-gate`,
`make lua-bench-gate`, or `make prerelease-hardening` when performance is the
surface under review or when preparing a release decision that explicitly
requires benchmark evidence.

For a focused test while iterating on a narrow change, build the required
targets first and state that the result is focused diagnostic coverage, not the
debug lifecycle gate:

```sh
cmake --build --preset debug --target lonejson_tests
ctest --test-dir build/debug -R '<test-regex>' --output-on-failure
```
