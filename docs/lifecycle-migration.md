# Lifecycle Migration Ledger

This ledger tracks the cpkt CMake lifecycle cutover until lonejson fully
converges on the shared lifecycle command and artifact contract.

| Old behavior | New lifecycle surface | Preserved behavior | Verification | Status |
| --- | --- | --- | --- | --- |
| Linux builds could bypass the default Bootlin compiler by setting an explicit host compiler. | All Linux builds use the pinned Bootlin GCC collection selected by the target toolchain. | Native debug, host release, fuzz, and cross builds still use the same target IDs and dependency bundles. | `lonejson_compiler_selection_tests` asserts compiler, linker, sysroot, libc, and explicit host compiler override handling. | Complete |
| Release dependency bundles were installed under `.deps/c.pkt.systems`. | Generated dependency installs live under `.cache/c.pkt.systems`; compiler collections remain in the shared `${CPKT_TOOLCHAIN_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/toolchains}` cache. | Existing c.pkt.systems bundle fetch and validation behavior is preserved. | Fetch, release-matrix, clean, package privacy, and archive verification tests cover the generated-state path. | Complete |
| Host release presets used short names `linux-gnu-release` and `linux-musl-release`. | Release presets use canonical target IDs: `x86_64-linux-gnu-release` and `x86_64-linux-musl-release`. | Compatibility can remain at the Make/script layer only where already documented. | Preset and release-matrix tests assert canonical names and explicit target metadata. | Complete |
| Package verification and privacy checks had partially separate command surfaces. | `package-verify` is the complete checksum-manifest privacy, relocatability, archive, and loader-metadata gate. | Focused aliases `verify-release-archives` and `verify-release-privacy` remain compatibility entry points. | Release artifact/archive verification tests and negative fixtures. | Pending |
| Lua discovery accepted multiple Lua versions and LuaJIT. | Lua tooling is Lua 5.5 only, with `lua-env`, `lua-rock`, `lua-test`, and release Lua artifact checks. | Existing Lua facade behavior is preserved. | Lua runtime discovery, Make surface, rockspec, source-rock, and release artifact tests. | Complete |
| Compose-backed e2e had project-specific command names only. | Standard `test-e2e`, `dev-up`, `dev-down`, `dev-reset`, `dev-ps`, and `dev-logs` surfaces wrap the existing deterministic local rig. | Existing curl/OIDC/M2M e2e tests remain available as focused targets. | Make help and e2e wrapper tests. | Complete |
