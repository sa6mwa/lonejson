# Native lifecycle migration

The repository follows `pkt-systems-cmake-lifecycle`: Make names commands,
Bash sequences operations, CMake owns build/install dependencies, and CTest
owns test selection, execution, assertions, and results. The migration preserves
the public C/Lua interfaces, ABI 26, pinned prerequisites, Linux target matrix,
optional Darwin artifacts, and source/SDK/Lua package names.

| Change | Observable proof | State |
| --- | --- | --- |
| Final release validates committed source and one exact lightweight tag without modifying refs. Candidate overrides are preparation-only. | Release identity rejection fixtures and executable release-graph ordering/failure checks. | Implemented |
| Required CTest selections reject empty inventories and stop on failure. Registered checks have labels and positive timeouts. | Empty-suite, metadata, and native JSON assertion regressions. | Implemented |
| Python serves small test fixtures; Bash/CMake perform assertions, service rendering, and acquisition decisions. | HTTP rewind negative cases, cache origin request counts, JSON assertions, and Podman rendering/port/path tests. | Implemented |
| One operation owns each mutable checkout workspace. Checkout-local locks and unsupported concurrent in-place tests are removed; shared verified archive acquisition retains its own locks. | Workspace/cleanup boundary tests and generated-header/version checks. | Implemented |

Binary preparation and host policy selection have converged: the six Linux
runtime lanes and seven extracted SDKs, including Darwin link/metadata proof,
passed with the required inventory. Remaining work is the final command and
CTest ownership sweep, followed by clean source reconstruction. Final tagged release and publication are
separate commitment actions.
