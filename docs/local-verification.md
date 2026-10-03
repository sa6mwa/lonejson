# Local Verification

`make prerelease` runs ordinary proof and the binary release matrix incrementally.
`make release` first verifies the lightweight-tag contract, cleans generated
state, and runs that proof graph. It then reconstructs the all-component source
archive and verifies the complete checksum manifest. Source reconstruction stays
out of the incremental binary matrix. The extracted archive runs host C and Lua
tests and builds its Lua release artifacts. Ordinary source-package checks use
its `VERSION` and `RELEASE_MANIFEST`, even inside a Git checkout, without changing
the parent checkout's tags.

The release pipeline runs native debug/Lua tests, sanitizers, Valgrind,
deterministic e2e, fuzz smoke, and benchmark checks before the binary matrix.
The matrix owns host release and cross-target testing once per configuration;
it does not repeat a preceding `test-cross` run. `test-all` remains a standalone
broad confidence gate. `prerelease-hardening` is an alias for `prerelease`, since
that graph already includes the native hardening and benchmark gates.

For routine edits, `make finalize-slice` formats and builds only the core,
map-cache, short-name and link-test targets, runs core/header/ABI checks and the
Lua suite, then checks formatting. It reuses configured builds and does not
stage examples or run the repository-wide policy fixtures. Run the matching
lifecycle or packaging fixtures separately when changing those surfaces.

The tag-mutating `make lifecycle-version-contract` checks Make and the version
resolver on the current checkout, using only a signing-disabled lightweight
`v99.99.99` fixture. Ordinary CTest separately checks CMake override behavior and
rejection of annotated/signed tag objects without modifying Git refs.

Temporary test and verifier workspaces live under `build/`, resolved from each
script's physical repository location. `make format-check` checks all maintained
C sources and headers without changing them. Shared libraries and Lua modules
use explicit export allowlists; build and extracted-SDK checks compare exact
symbol tables with target tools and reject private downstream linkage.

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
building and verifying binary SDKs and Lua artifacts; host-only LuaRocks/tooling
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

Every non-shipped Linux executable built by CMake—tests, fixture servers,
examples, benchmarks, and fuzzers—also records that collection's ELF loader
and private transitive `RPATH`. CTest and e2e scripts execute them directly;
they do not set `LD_LIBRARY_PATH`. Linux Lua behavior runs through the
embedded Lua runner linked against the selected bundle. Benchmarks load a
separate production-configured module; test-only uservalue, integer, and
statistics definitions remain confined to the test module. Native Darwin Lua
tests and benchmarks use its native LuaRocks runtime and the host build;
source-rock packaging remains host-side tooling.

`make package-verify` rejects every checksum-listed artifact if a packaged
library, archive, or consumer metadata contains a Bootlin cache or stable
collection path. Development interpreter and RPATH settings are therefore
confined to local executables and cannot enter a release upload.

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
debug gate, host release tests, curl/auth host tests, cross target tests,
host sanitizers where supported, Valgrind, deterministic local e2e, and fuzz
smoke.

For release-candidate rehearsals, `LONEJSON_VERSION_OVERRIDE=X.Y.Z` is accepted
by `scripts/release_version.sh`, Make, and CMake. A CMake environment override
is intentionally one-shot and is not written into `CMakeCache.txt`; use
`-D LONEJSON_VERSION_OVERRIDE=X.Y.Z` only when the build directory itself should
retain that override.

`test-all`, `prerelease`, and `release` run `bench-check`
against frozen C and Lua baselines for the current host. Missing host baselines
produce an explicit skip rather than an invented comparison. Use `bench-gate`
or `lua-bench-gate` for focused performance verification.

For a focused test while iterating on a narrow change, build the required
targets first and state that the result is focused diagnostic coverage, not the
debug lifecycle gate:

```sh
cmake --build --preset debug --target lonejson_tests
ctest --test-dir build/debug -R '<test-regex>' --output-on-failure
```

Linux source-rock validation installs the just-built public C SDK under
`build/lua-sdk`, compiles the module against its installed headers, and runs
its Lua correctness/fuzz tests through a target-built embedding runner. Host
Lua and LuaRocks provide tooling. Compiler and supported linker warnings are
errors in Debug and Release configurations.

`make lua-env` prints LuaRocks search paths and the installed SDK's
`LONEJSON_LIBDIR`; it leaves loader selection to the target runner. To execute
a Lua script against the installed rock on Linux, use:

```sh
scripts/run_installed_lua.sh "$PWD/build/luarocks" "$PWD/build/lua-sdk/lib" lua path/to/script.lua
```

LuaRocks builds keep their tool scratch beneath `build/` too.


Darwin cross builds select the newest complete installed `arm64-apple-darwin25.x`
prefix through the shared resolver. Set `CPKT_OSXCROSS_HOST` to pin an exact
installed prefix. Configure, packaging, smoke consumers, and verification use
the selected compiler and sibling tools. lonejson consumes prebuilt SDK bundles
and generates no Mach interfaces, so it does not provision or require host MIG.

`make package` builds and verifies packages incrementally. `make release` owns
clean reconstruction and the full final gate. Native `make valgrind` enables
curl, OpenSSL, JWT and OIDC and runs the main, curl rewind, and Lua tests through
CTest Memcheck serially with a 30 minute timeout. Binary SDK manifests record
curl and OpenSSL as external optional facade requirements; core libraries do
not embed either dependency or require them for ordinary linking.

CMake packaging validates physical ownership of each staging workspace before
cleanup. The default `dist/` must be a real directory; a symlink is rejected.
Explicit custom artifact destinations retain their ownership-marker policy.
Completed artifacts are copied to their final destinations, including those
on another filesystem; packaging intermediates remain under `build/`.
