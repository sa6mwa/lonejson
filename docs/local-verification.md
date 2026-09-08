# Local Verification

Both `make prerelease` and `make release` run the source-archive smoke gate after
the release matrix, followed by package verification. The extracted archive runs
host C and Lua tests and builds its Lua release artifacts. Ordinary source-package
checks use the archive's `VERSION` and `RELEASE_MANIFEST`, even when extracted
inside a Git checkout; they must not change the parent checkout's tags. The
ordinary source-tarball regression test exercises benchmark and checksum checks
in that nested layout before the full release pipeline runs.

## Compiler and Cross Toolchains

On native Apple Silicon macOS, plain CMake, native presets, the Darwin release
preset, curl examples, and LuaRocks use the active Apple compiler and macOS SDK
reported by `xcrun`. Install Xcode or Command Line Tools first. Native builds
do not require osxcross. Linux hosts use osxcross for Darwin artifacts and
Bootlin for Linux targets; Apple SDKs are never downloaded by the project.
The native preset toolchain is now `cmake/toolchains/native.cmake`; refresh an
existing build cache with `cmake --fresh --preset <preset>` after updating.

Linux collections are pinned in `scripts/cpkt-toolchains.sh` to Bootlin stable
`2026.08-1` (GCC 15.3.0), including archive SHA-256 checksums. Updates replace
the complete collection, not just the compiler. AFL++ cache identities include
the Bootlin collection so upgrades cannot reuse an incompatible GCC plugin.
Standalone curl examples, LuaRocks bindings (including source rocks), and
compiler-using test fixtures resolve the same pinned collection. Linux LuaRocks
builds override its compiler setting; a host compiler is never a fallback.
After a Bootlin upgrade, configure existing build directories with
`cmake --fresh --preset <preset>` before building. Configuration rejects an old
collection cache because CMake's automatic compiler-change restart can discard
preset feature settings and silently reduce test coverage.

CMake selects the pinned Bootlin collection matching the native Linux host's
processor and libc for debug, host, and Lua workflows. Explicit Linux target
toolchain files provision their matching pinned Bootlin collection into the
lifecycle cache during configuration. To inspect status or warm every Linux
collection before a matrix build:

```sh
make toolchains-all
```

The default cache is
`${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/toolchains`; set
`CPKT_TOOLCHAIN_CACHE` to use another shared cache location.

`scripts/cpkt-toolchains.sh discover` reports every lifecycle target without
downloading it. The Darwin entry reports native Apple tools on macOS and local
osxcross status on Linux.

Use `make cross-build` to configure/build the Linux cross presets and `make
cross-test` (or the compatibility name `make test-cross`) to execute their
QEMU-backed test coverage. `make package-single-header` creates the separate
version-stamped single-header artifact without running a full release.

`make release-matrix` repeats the runnable QEMU-backed cross coverage while
building and verifying every release artifact; host-only LuaRocks/tooling
checks remain native. A missing required runner is a failure, not a skip.

Pinned c.pkt.systems SDK archives are separate immutable cache entries under
`${CPKT_DEPENDENCY_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/deps}`.
`make deps-host` and the target-specific dependency targets verify each archive
by SHA-256 before reuse, then extract it into the checkout-local `.cache/`
tree. `make clean` removes only that extracted local state and preserves the
shared archive and toolchain caches.

Every Linux native debug, host, and release build uses a single pinned Bootlin collection:
its GCC driver, GNU linker, binutils/debug tools, libc sysroot, and target
runtime. Configuration verifies the compiler triple; package inspection reads
the configured target tools from the same collection. Native memory checking
uses host-installed Valgrind; fuzzing uses a cached, pinned AFL++ GCC-plugin
build tied to the Bootlin x86_64 collection. Valgrind is not a
compiler-based uninitialized-memory analysis, but it provides the native leak
and invalid-memory gate without a non-portable LLVM distribution.
ThreadSanitizer support is probed by `make tsan` after resolving the selected
native Bootlin target, so broad gates do not make a parse-time skip decision
before the toolchain exists.

Fuzzing requires no root privileges or host sysctl changes. The native input
driver disables its own Linux dumpability before processing each input, which
prevents Apport or other piped core collectors from delaying crash reporting.
The runner checks that driver contract before bypassing AFL++'s global
core-pattern warning. Signals still identify crashes; AFL++ retains the input
instead of producing a core dump. Replay a retained input by passing its path
to the same fuzz executable. This protection applies to input execution, not
faults in the loader before the driver starts.

`make fuzz-smoke` uses a fixed RNG seed and a 1000-execution AFL++ budget per
target instead of a one-second race against host speed. AFL++ performs seed
calibration and checks the budget at its own boundaries, so the final execution
count may exceed the budget. A fixed one-second per-input timeout distinguishes
hangs; this is a failure bound, not a required completion delay. Standard
`make fuzz` and `make fuzz-long` retain their duration budgets. These are fuzz
campaigns, not a promise of bit-identical mutations across hosts.

Every fuzz gate first tests the real AFL++ engine against prescribed successful,
SIGSEGV, SIGABRT, and indefinitely blocked inputs. A prescribed mutation proves
that a discovered crash is retained unchanged and causes the runner to fail.
Crashing or hanging initial seeds also fail and remain in the staged seed set.
Saved crashes and hangs fail the gate even when AFL++ itself exits successfully; findings remain
under `build/fuzz/afl/<target>/output/` for investigation.

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

For release-candidate rehearsals, `LONEJSON_VERSION_OVERRIDE=X.Y.Z` is accepted
by `scripts/release_version.sh`, Make, and CMake. A CMake environment override
is intentionally one-shot and is not written into `CMakeCache.txt`; use
`-D LONEJSON_VERSION_OVERRIDE=X.Y.Z` only when the build directory itself should
retain that override.

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
