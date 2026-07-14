SHELL := bash
.DEFAULT_GOAL := help
MAKEFLAGS += --no-builtin-rules

DEBUG_PRESET := debug
HOST_PRESET := host
ASAN_PRESET := asan
TSAN_PRESET := tsan
VALGRIND_PRESET := valgrind
FUZZ_PRESET := fuzz
TIME_STEP := ./scripts/run_timed.sh
LONEJSON_HAVE_TSAN ?= $(shell bash "$(CURDIR)/scripts/check_bootlin_tsan_support.sh")
LONEJSON_TEST_ALL_HOST_CURL ?= 1
LONEJSON_E2E_SERVICES_READY ?= 0
RELEASE_BUILD_PRESETS := \
	x86_64-linux-gnu-release \
	x86_64-linux-musl-release \
	aarch64-linux-gnu-release \
	aarch64-linux-musl-release \
	armhf-linux-gnu-release \
	armhf-linux-musl-release \
	$(shell if ./scripts/osxcross_available.sh >/dev/null 2>&1; then printf '%s' arm64-apple-darwin-release; fi)
CROSS_RELEASE_PRESETS := \
	aarch64-linux-gnu-release \
	aarch64-linux-musl-release \
	armhf-linux-gnu-release \
	armhf-linux-musl-release
LUA ?= $(shell ./scripts/resolve_lua55.sh 2>/dev/null)
LUAROCKS ?= luarocks
GENERATED_FIXTURE_DIR := $(CURDIR)/build/generated/fixtures
ifneq ($(LONEJSON_VERSION_OVERRIDE),)
export LONEJSON_VERSION_OVERRIDE
endif
RELEASE_VERSION := $(shell ./scripts/release_version.sh)
DIST_DIR := $(CURDIR)/dist
RELEASE_SOURCE_TARBALL := $(DIST_DIR)/lonejson-$(RELEASE_VERSION).tar.gz
RELEASE_HEADER_GZ := $(DIST_DIR)/lonejson-$(RELEASE_VERSION).h.gz
RELEASE_ROCKSPEC := $(DIST_DIR)/lonejson-$(RELEASE_VERSION)-1.rockspec
RELEASE_PACK_DIR := $(DIST_DIR)/.pack
RELEASE_PACK_STAGE_DIR := $(RELEASE_PACK_DIR)/lonejson-$(RELEASE_VERSION)
RELEASE_LUA_SOURCE_TARBALL := $(DIST_DIR)/lonejson-lua-$(RELEASE_VERSION).tar.gz
RELEASE_PACK_ROCKSPEC := $(RELEASE_PACK_DIR)/lonejson-$(RELEASE_VERSION)-1.rockspec
RELEASE_ROCK := $(DIST_DIR)/lonejson-$(RELEASE_VERSION)-1.src.rock
RELEASE_CHECKSUMS := $(DIST_DIR)/lonejson-$(RELEASE_VERSION)-CHECKSUMS
PERF_CORPUS := tests/fixtures/vendor/json_test_suite/test_parsing
PERF_HOST_ID ?= $(shell ./scripts/bench_host_id.sh 2>/dev/null || printf unknown)
PERF_HOST_DIR := $(CURDIR)/perflogs/hosts/$(PERF_HOST_ID)
PERF_LATEST := $(PERF_HOST_DIR)/latest.json
PERF_HISTORY := $(PERF_HOST_DIR)/history.jsonl
PERF_BASELINE := $(PERF_HOST_DIR)/baseline.json
PERF_ARCHIVE_DIR := $(PERF_HOST_DIR)/runs
LUA_PERF_LATEST := $(PERF_HOST_DIR)/lua/latest.json
LUA_PERF_HISTORY := $(PERF_HOST_DIR)/lua/history.jsonl
LUA_PERF_BASELINE := $(PERF_HOST_DIR)/lua/baseline.json
LUA_PERF_ARCHIVE_DIR := $(PERF_HOST_DIR)/lua/runs
PERF_ITERATIONS ?= 40
LUA_PERF_ITERATIONS ?= 30
FUZZ_TIME ?= 30
FUZZ_LONG_TIME ?= 300
LUA_ROCK_TREE := build/luarocks
LUA_ROCKSPEC := $(LUA_ROCK_TREE)/lonejson-$(RELEASE_VERSION)-1.rockspec
LUA_ROCK_STAMP := $(LUA_ROCK_TREE)/.installed.stamp
LUA_ROCK_BUILD_LOCK := $(LUA_ROCK_TREE)/.build.lock
LUA_ROCK_EXTRA_CFLAGS ?= -std=gnu89 -O3 -DNDEBUG -D_FILE_OFFSET_BITS=64 -fno-semantic-interposition
LONEJSON_LUA_LIBDIR ?= $(CURDIR)/build/$(DEBUG_PRESET)
LONEJSON_LUA_BENCH_LIBDIR ?= $(CURDIR)/build/$(HOST_PRESET)
LUA_ROCK_BUILD_BYPRODUCTS := \
	$(CURDIR)/lonejson \
	$(CURDIR)/src/lua/lonejson_lua.o
LUA_ROCK_SOURCES := \
	lonejson.rockspec.in \
	scripts/build_lua_rock.sh \
	scripts/package_lua_src_rock.sh \
	scripts/render_release_rockspec.sh \
	scripts/release_version.sh \
	scripts/stage_lua_rock_sources.sh \
	include/lonejson.h \
	src/lua/lonejson_lua.c \
	src/lua/lonejson_lua.h \
	$(wildcard src/lua/*.inc.h) \
	lua/lonejson/init.lua
LUA_ROCK_LIBLONEJSON_SOURCES := \
	CMakeLists.txt \
	src/lonejson.c \
	src/lonejson_impl.h \
	src/lonejson_internal.h \
	$(wildcard src/impl/*.h)

SANITIZER_CTEST_EXCLUDE := lonejson_(bench_baseline_history_tests|bench_retry_confirm_tests|lua_legacy_uservalue_tests|lua_schema_cache_tests|lua_encode_stats_tests|lua_external_liblonejson_tests|lua_target_tests|c_pkt_systems_fetch_retry_tests|cmake_threads_optional_tests|run_release_matrix_darwin_target_tests)
HOST_POLICY_CTEST_EXCLUDE := lonejson_(discover_target_tools_tests|compiler_selection_tests|darwin_macho_metadata_tests|darwin_linker_route_tests|c_pkt_systems_fetch_retry_tests|cmake_threads_optional_tests|cmake_c_pkt_systems_root_tests|test_all_hardening_tests|bootlin_tsan_support_tests|cross_sanitizer_matrix_tests|cmake_fuzz_sanitizer_conflict_tests|cmake_fuzz_auth_optional_tests|release_werror_tests|source_release_tarball_tests|lua_legacy_uservalue_tests|lua_schema_cache_tests|lua_encode_stats_tests|lua_external_liblonejson_tests|lua_src_rock_privacy_tests|lua_public_boundary_tests|lua_surface_coverage_tests|lua_source_stage_manifest_tests|release_artifact_verify_tests|release_archive_verify_tests|lua_native_test_target_filter_tests|run_release_matrix_darwin_target_tests|release_checksum_manifest_tests|ctest_metadata_tests|short_names_tests|short_names_disabled_tests|single_header_strict_warning_tests|single_header_strict_warning_build_tests|single_header_strict_toolchain_build_tests|single_header_config_default|single_header_config_omit_protocol|single_header_config_lj_implementation|single_header_config_lj_config_aliases|single_header_config_short_names_disabled|static_link_tests|shared_link_tests|shared_soversion_tests|single_header_version_tests|header_abi_version_tests|single_header_release_version_tests|bench_gate_tests)
SANITIZER_CTEST_EXCLUDE := $(SANITIZER_CTEST_EXCLUDE)|$(HOST_POLICY_CTEST_EXCLUDE)

.PHONY: \
	help \
	finalize-slice \
	build \
	build-debug \
	build-host \
	build-release \
	cross-build \
	release-lua-artifacts \
	package \
	prerelease-artifacts \
	package-source \
	package-source-smoke \
	package-checksums \
	package-single-header \
	release-source-artifact \
	release-source-smoke \
	release-darwin-smoke-bundle \
	package-verify \
	verify-release-archives \
	verify-release-privacy \
	prerelease \
	prerelease-live \
	prerelease-hardening \
	release-pipeline \
	release-matrix \
	release \
	print-release-version \
	lua-rock \
	lua-env \
	lua-test \
	lua-fuzz \
	lua-bench \
	lua-bench-freeze-baseline \
	lua-bench-compare \
	lua-bench-gate \
	bench \
	bench-check \
	bench-freeze-baseline \
	bench-compare \
	bench-baseline-history \
	bench-gate \
	test \
	test-debug \
	test-host \
	test-host-curl \
	test-cross \
	cross-test \
	cross-sanitizers \
	test-all \
	test-all-bindings \
	test-install-tree \
	example-smoke-local \
	asan \
	tsan \
	valgrind \
	fuzz-smoke \
	fuzz \
	fuzz-long \
	stack-usage \
	format \
	deps-debug \
	deps-release \
	deps-host \
	deps-x86_64-linux-gnu \
	deps-x86_64-linux-musl \
	deps-aarch64-linux-gnu \
	deps-aarch64-linux-musl \
	deps-armhf-linux-gnu \
	deps-armhf-linux-musl \
	deps-arm64-apple-darwin \
	toolchains-x86_64-linux-gnu \
	toolchains-x86_64-linux-musl \
	toolchains-aarch64-linux-gnu \
	toolchains-aarch64-linux-musl \
	toolchains-armhf-linux-gnu \
	toolchains-armhf-linux-musl \
	toolchains-aflpp \
	toolchains-all \
	deps-cross \
	deps-all \
	certs \
	dev-up \
	dev-down \
	dev-reset \
	dev-ps \
	dev-logs \
	compose-up \
	compose-down \
	compose-ps \
	compose-logs \
	curl-examples \
	test-e2e \
	test-curl-e2e \
	test-oidc-e2e \
	test-m2m-e2e \
	clean \
	clean-dist

help:
	@printf '%s\n' \
		'make finalize-slice         Format, build, and run the make test debug gate before committing a slice.' \
		'make build                  Configure and build the full debug tree (tests and standalone examples, excluding lua-* and curl-examples).' \
		'make build-debug            Alias for make build.' \
		'make build-host             Configure and build the host-native release preset.' \
		'make build-release          Configure and build the full shipped release test matrix.' \
		'make cross-build            Configure and build every supported Linux cross-release preset.' \
		'make package                Build all release packages through make release.' \
		'make prerelease-artifacts   Compatibility alias for make release-matrix.' \
		'make package-source         Build the source-only release tarball in dist/.' \
		'make package-source-smoke   Unpack the source release tarball into a temp tree, then run host C/Lua tests and Lua artifact packaging there.' \
		'make package-checksums      Generate release checksums for existing dist artifacts.' \
		'make package-single-header  Build the version-stamped standalone-header artifact in dist/.' \
		'make release-lua-artifacts  Build the standalone Lua source package, release rockspec, and source rock in dist/.' \
		'make package-verify         Verify checksum-listed release artifacts for privacy, relocatability, and instrumentation leaks.' \
		'make verify-release-archives Alias for make package-verify.' \
		'make verify-release-privacy Alias for make package-verify.' \
		'make prerelease             Run the deterministic release pipeline without cleaning generated state first.' \
		'make prerelease-live        Refuse live external-provider release checks unless explicitly enabled.' \
		'make prerelease-hardening   Run prerelease plus explicit benchmark checks.' \
		'make release-pipeline       Internal shared release proof used by prerelease and release.' \
		'make release-matrix         Build host release tests, then package, checksum, and verify every release target without cleaning first.' \
		'make release                Clean generated state, then run the same pipeline as prerelease.' \
		'make print-release-version  Print the exact version used by package and release targets.' \
		'make release-source-smoke   Unpack the source release tarball into a temp tree, then run host C/Lua tests and Lua artifact packaging there.' \
		'make release-darwin-smoke-bundle Build the Darwin smoke ZIP with example and link-smoke binaries.' \
		'make lua-rock               Generate a local rockspec in build/luarocks and install the Lua module there.' \
		'make lua-env                Print shell exports for using the repo-local Lua rock and debug C library.' \
		'make lua-test               Build the Lua module and run the Lua integration test.' \
		'make lua-fuzz               Build the Lua module and run the Lua randomized binding fuzz smoke.' \
		'make lua-bench              Run the standalone Lua benchmark harness, compare it, and enforce the Lua benchmark gate.' \
		'make lua-bench-freeze-baseline Freeze the last Lua benchmark history entry as the Lua baseline.' \
		'make lua-bench-compare      Run a fresh Lua benchmark, then compare against the committed Lua baseline.' \
		'make lua-bench-gate         Run a fresh Lua benchmark, then enforce the Lua benchmark gate against the committed Lua baseline.' \
		'make bench                  Build and run the host benchmark against the vendored JSON corpus, then compare/gate against the working baseline file if present.' \
		'make bench-check            Run C and Lua benchmark gates using temporary result files, leaving perflogs/ clean.' \
		'make bench-freeze-baseline  Freeze the last history entry as the current benchmark baseline.' \
		'make bench-compare          Run a fresh C benchmark, then compare against the committed C baseline.' \
		'make bench-baseline-history Compare frozen C/Lua benchmark baselines across git history.' \
		'make bench-gate             Run a fresh C benchmark, then enforce the C benchmark gate against the committed C baseline.' \
		'make test                   Build debug artifacts, run CTest, then Lua tests; this is the debug lifecycle gate.' \
		'make test-debug             Alias for make test.' \
		'make test-host              Build and run the host-native test preset.' \
		'make test-host-curl         Build and run the host-native curl-enabled test preset.' \
		'make test-cross             Configure, build, and run all cross release test presets serially.' \
		'make cross-test             Standard alias for make test-cross.' \
		'make cross-sanitizers       Extra hardening: build and run the supported armhf-linux-gnu ASan/UBSan target under QEMU.' \
		'make toolchains-aflpp       Build the pinned native AFL++ GCC-plugin toolchain in the shared lifecycle cache.' \
		'make test-all               Run debug, host, host-curl, cross, host sanitizers, Valgrind, e2e, and fuzz-smoke serially.' \
		'make test-all-bindings      Compatibility alias for make lua-test; binding coverage is no longer a full world gate.' \
		'make test-install-tree      Verify checksum-listed SDK archives through installed CMake and pkg-config consumers.' \
		'make example-smoke-local    Build and stage standalone local examples.' \
		'make asan                   Build and run the ASan/UBSan preset.' \
		'make tsan                   Build the TSan preset and run the pure-C CTest subset that does not depend on external unsanitized runtimes.' \
		'make valgrind               Run the native x86_64 debug CTest suite under Valgrind Memcheck.' \
		'make fuzz-smoke             Build all AFL++ targets, run a seeded 1s smoke pass for each, and run Lua binding fuzz smoke.' \
		'make fuzz                   Build all AFL++ targets, run a seeded 30s pass for each and run Lua binding fuzz smoke; missing large synthetic seeds are regenerated automatically.' \
		'make fuzz-long              Run the same fuzz targets with a several-minute soak per target.' \
		'make stack-usage            Build with compiler stack-usage reporting and print the report.' \
		'make format                 Run clang-format over the C sources.' \
		'make deps-debug             Alias for make deps-host.' \
		'make deps-release           Alias for make deps-all.' \
		'make deps-host              Cache the verified host c.pkt.systems archive globally, then extract it under .cache/.' \
		'make toolchains-x86_64-linux-gnu Install the pinned x86_64 glibc Bootlin collection.' \
		'make toolchains-x86_64-linux-musl Install the pinned x86_64 musl Bootlin collection.' \
		'make toolchains-aarch64-linux-gnu Install the pinned aarch64 glibc Bootlin collection.' \
		'make toolchains-aarch64-linux-musl Install the pinned aarch64 musl Bootlin collection.' \
		'make toolchains-armhf-linux-gnu Install the pinned armhf glibc Bootlin collection.' \
		'make toolchains-armhf-linux-musl Install the pinned armhf musl Bootlin collection.' \
		'make toolchains-all         Install pinned Bootlin Linux toolchains in the shared lifecycle cache.' \
		'make deps-x86_64-linux-gnu  Download and extract the x86_64 glibc c.pkt.systems bundle.' \
		'make deps-x86_64-linux-musl Download and extract the x86_64 musl c.pkt.systems bundle.' \
		'make deps-aarch64-linux-gnu Download and extract the aarch64 glibc c.pkt.systems bundle.' \
		'make deps-aarch64-linux-musl Download and extract the aarch64 musl c.pkt.systems bundle.' \
		'make deps-armhf-linux-gnu   Download and extract the armhf glibc c.pkt.systems bundle.' \
		'make deps-armhf-linux-musl  Download and extract the armhf musl c.pkt.systems bundle.' \
		'make deps-arm64-apple-darwin Download and extract the arm64 Darwin c.pkt.systems bundle.' \
		'make deps-cross             Download and extract bundles required by make test-cross.' \
		'make deps-all               Download and extract every supported c.pkt.systems bundle.' \
		'make certs                  Generate the local self-signed localhost TLS cert for nginx.' \
		'make dev-up                 Start the local compose-backed e2e services through scripts/dev-up.sh.' \
		'make dev-down               Stop the local compose-backed e2e services through scripts/dev-down.sh.' \
		'make dev-reset              Stop the local compose stack and remove generated local service state.' \
		'make dev-ps                 Show the local compose-backed e2e service status.' \
		'make dev-logs               Tail logs from the local compose-backed e2e services.' \
		'make compose-up             Compatibility alias for make dev-up.' \
		'make compose-down           Compatibility alias for make dev-down.' \
		'make compose-ps             Compatibility alias for make dev-ps.' \
		'make compose-logs           Compatibility alias for make dev-logs.' \
		'make curl-examples          Build the curl examples against the host c.pkt.systems dependency bundle.' \
		'make test-e2e               Run all deterministic local e2e gates serially; set LONEJSON_*_E2E_PORT to avoid host-port conflicts.' \
		'make test-curl-e2e          Build and run the curl examples against the local HTTPS rig.' \
		'make test-oidc-e2e          Build and run OIDC/OAuth2/JWKS e2e against the local compose rig.' \
		'make test-m2m-e2e           Build and run M2M Basic/Bearer auth e2e with curl as the client.' \
		'make release-source-artifact Build the source-only release tarball in dist/.' \
		'make clean                  Remove build/, dist/, .cache/, devenv/volumes/, examples/bin/, and generated Lua module artifacts; preserve shared caches.' \
		'make clean-dist             Remove dist/ release artifacts only.'

finalize-slice:
	$(MAKE) format
	$(MAKE) test-debug

build:
	./scripts/build.sh $(DEBUG_PRESET) --stage-examples

build-debug: build

build-host:
	./scripts/build.sh $(HOST_PRESET)

build-release: deps-all
	@set -e; for preset in $(RELEASE_BUILD_PRESETS); do \
		./scripts/build.sh "$$preset"; \
	done

cross-build: deps-cross
	./scripts/cross_build.sh $(CROSS_RELEASE_PRESETS)

$(DIST_DIR):
	mkdir -p "$(DIST_DIR)"

$(RELEASE_PACK_DIR):
	mkdir -p "$(RELEASE_PACK_DIR)"

$(RELEASE_PACK_STAGE_DIR): Makefile $(LUA_ROCK_SOURCES) | $(RELEASE_PACK_DIR)
	./scripts/stage_lua_rock_sources.sh "$(CURDIR)" "$(RELEASE_PACK_STAGE_DIR)" "$(RELEASE_VERSION)"

$(RELEASE_LUA_SOURCE_TARBALL): $(RELEASE_PACK_STAGE_DIR) | $(DIST_DIR)
	rm -f "$(DIST_DIR)/lonejson-lua-$(RELEASE_VERSION).tar" "$(RELEASE_LUA_SOURCE_TARBALL)"
	cd "$(RELEASE_PACK_DIR)" && tar -cf "$(DIST_DIR)/lonejson-lua-$(RELEASE_VERSION).tar" "lonejson-$(RELEASE_VERSION)"
	gzip -9 -f "$(DIST_DIR)/lonejson-lua-$(RELEASE_VERSION).tar"

$(RELEASE_ROCKSPEC): lonejson.rockspec.in scripts/render_release_rockspec.sh | $(DIST_DIR)
	lib_ext="$$($(LUAROCKS) config variables.LIB_EXTENSION)"; ./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "$(RELEASE_ROCKSPEC)" "" "" "$$lib_ext"

$(RELEASE_PACK_ROCKSPEC): Makefile $(RELEASE_LUA_SOURCE_TARBALL)
	cd "$(RELEASE_PACK_STAGE_DIR)" && lib_ext="$$($(LUAROCKS) config variables.LIB_EXTENSION)" && ./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "../$(notdir $(RELEASE_PACK_ROCKSPEC))" "file://$(notdir $(RELEASE_LUA_SOURCE_TARBALL))" "" "$$lib_ext" "lonejson-$(RELEASE_VERSION)"

$(RELEASE_ROCK): $(RELEASE_PACK_ROCKSPEC) $(RELEASE_ROCKSPEC) scripts/package_lua_src_rock.sh scripts/validate_luarocks.sh
	./scripts/package_lua_src_rock.sh "$(RELEASE_ROCK)" "$(RELEASE_PACK_ROCKSPEC)" "$(RELEASE_LUA_SOURCE_TARBALL)"
	cmake --preset $(DEBUG_PRESET)
	cmake --build --preset $(DEBUG_PRESET) --target lonejson_shared
	./scripts/validate_luarocks.sh "$(RELEASE_ROCK)" "$(LONEJSON_LUA_LIBDIR)"
	rm -rf "$(RELEASE_PACK_DIR)"

release-lua-artifacts: $(RELEASE_ROCKSPEC) $(RELEASE_LUA_SOURCE_TARBALL) $(RELEASE_ROCK)

package:
	./scripts/package.sh

package-source: release-source-artifact

release-source-artifact:
	cmake --preset $(HOST_PRESET)
	cmake --build --preset package-source

$(RELEASE_SOURCE_TARBALL):
	$(MAKE) release-source-artifact

release-source-smoke: release-source-artifact
	./scripts/test_release_from_source.sh "$(CURDIR)" "$(DIST_DIR)/lonejson-$(RELEASE_VERSION).tar.gz"
	cmake --build --preset package-checksums

package-source-smoke: release-source-smoke

package-checksums:
	cmake --preset $(HOST_PRESET)
	cmake --build --preset package-checksums

package-single-header:
	cmake --preset $(HOST_PRESET)
	cmake --build --preset package-single-header

release-darwin-smoke-bundle: deps-arm64-apple-darwin
	cmake --preset arm64-apple-darwin-release
	cmake --build --preset arm64-apple-darwin-release --target package-darwin-smoke-bundle

prerelease-artifacts: release-matrix

package-verify:
	./scripts/package-verify.sh "$(RELEASE_CHECKSUMS)"

verify-release-archives: package-verify

verify-release-privacy:
	./scripts/verify_release_privacy.sh "$(RELEASE_CHECKSUMS)"

prerelease: release-pipeline

prerelease-live:
	@test "$${LONEJSON_ENABLE_LIVE_TESTS:-}" = "1" || (printf '%s\n' 'Set LONEJSON_ENABLE_LIVE_TESTS=1 to run live prerelease checks; no live prerelease checks are currently defined.' >&2; exit 1)

prerelease-hardening: prerelease
	+$(TIME_STEP) hardening/bench-check $(MAKE) bench-check

release-pipeline:
	+$(TIME_STEP) prerelease/format $(MAKE) format
	+$(TIME_STEP) prerelease/test-all $(MAKE) test-all LONEJSON_TEST_ALL_HOST_CURL=0
	+$(TIME_STEP) prerelease/release-matrix $(MAKE) release-matrix

release-matrix:
	./scripts/run_linux_release_matrix.sh

release:
	$(TIME_STEP) release/clean ./scripts/clean.sh
	+$(TIME_STEP) release/pipeline $(MAKE) release-pipeline

print-release-version:
	@printf '%s\n' "$(RELEASE_VERSION)"

bench:
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" ./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	if [ -f "$(PERF_BASELINE)" ]; then \
		./build/$(HOST_PRESET)/lonejson_bench compare "$(PERF_BASELINE)" "$(PERF_LATEST)" && \
		./build/$(HOST_PRESET)/lonejson_bench gate "$(PERF_BASELINE)" "$(PERF_LATEST)"; \
	fi

bench-check:
ifeq ($(and $(wildcard $(PERF_BASELINE)),$(wildcard $(LUA_PERF_BASELINE))),)
	@if [ ! -f "$(PERF_BASELINE)" ] || [ ! -f "$(LUA_PERF_BASELINE)" ]; then \
		printf '%s\n' "bench-check skipped: missing frozen benchmark baseline for host $(PERF_HOST_ID)" >&2; \
		printf '%s\n' "  C baseline: $(PERF_BASELINE)" >&2; \
		printf '%s\n' "  Lua baseline: $(LUA_PERF_BASELINE)" >&2; \
		printf '%s\n' "run 'make bench-freeze-baseline lua-bench-freeze-baseline' on this host to create one" >&2; \
		exit 0; \
	fi
else
	@$(MAKE) lua-rock
	@tmp_dir="$$(mktemp -d)"; \
	trap 'rm -rf "$$tmp_dir"' EXIT; \
	c_latest="$$tmp_dir/c-latest.json"; \
	c_history="$$tmp_dir/c-history.jsonl"; \
	c_runs="$$tmp_dir/c-runs"; \
	lua_latest="$$tmp_dir/lua-latest.json"; \
	lua_history="$$tmp_dir/lua-history.jsonl"; \
	lua_runs="$$tmp_dir/lua-runs"; \
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && \
	export LD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${LD_LIBRARY_PATH:-}" && \
	export DYLD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${DYLD_LIBRARY_PATH:-}" && \
	export LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" && \
	cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench lonejson_shared && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" ./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$$c_latest" "$$c_history" "$$c_runs" "$(PERF_ITERATIONS)" && \
	if ! ./build/$(HOST_PRESET)/lonejson_bench gate "$(PERF_BASELINE)" "$$c_latest"; then \
		./build/$(HOST_PRESET)/lonejson_bench compare "$(PERF_BASELINE)" "$$c_latest"; \
		printf '%s\n' 'C benchmark gate failed once; rerunning only failing cases once to confirm.' >&2; \
		$(LUA) bench/lonejson_lua_bench.lua confirm-c "./build/$(HOST_PRESET)/lonejson_bench" "$(PERF_BASELINE)" "$$c_latest" "$(PERF_ITERATIONS)"; \
	fi && \
	$(LUA) bench/lonejson_lua_bench.lua run "$$c_latest" "$$lua_latest" "$$lua_history" "$$lua_runs" "$(LUA_PERF_ITERATIONS)" && \
	if ! $(LUA) bench/lonejson_lua_bench.lua gate "$(LUA_PERF_BASELINE)" "$$lua_latest"; then \
		$(LUA) bench/lonejson_lua_bench.lua compare "$(LUA_PERF_BASELINE)" "$$lua_latest"; \
		printf '%s\n' 'Lua benchmark gate failed once; rerunning only failing cases once to confirm.' >&2; \
		$(LUA) bench/lonejson_lua_bench.lua confirm-lua "$$c_latest" "$(LUA_PERF_BASELINE)" "$$lua_latest" "$(LUA_PERF_ITERATIONS)"; \
	fi
endif

bench-freeze-baseline:
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	./build/$(HOST_PRESET)/lonejson_bench freeze-baseline "$(PERF_HISTORY)" "$(PERF_BASELINE)"

bench-compare:
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" ./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	./build/$(HOST_PRESET)/lonejson_bench compare "$(PERF_BASELINE)" "$(PERF_LATEST)"

bench-baseline-history:
	@$(MAKE) --no-print-directory lua-rock >/dev/null
	@cmake --preset $(HOST_PRESET) >/dev/null
	@cmake --build --preset $(HOST_PRESET) --target lonejson_shared >/dev/null
	@eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && \
		LD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${LD_LIBRARY_PATH:-}" \
		DYLD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${DYLD_LIBRARY_PATH:-}" \
		$(LUA) scripts/bench_baseline_history.lua --repo "$(CURDIR)" --host-id "$(PERF_HOST_ID)"

bench-gate:
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" ./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	./build/$(HOST_PRESET)/lonejson_bench gate "$(PERF_BASELINE)" "$(PERF_LATEST)"

test: build
	./scripts/test.sh $(DEBUG_PRESET)
	$(MAKE) lua-test

test-debug: test

test-host: build-host
	./scripts/host_test.sh

test-host-curl: deps-host
	bundle_root="$$(./scripts/detect_c_pkt_systems_bundle.sh)" && cmake --preset host-curl -D LONEJSON_C_PKT_SYSTEMS_ROOT="$$bundle_root"
	cmake --build --preset host-curl
	ctest --preset host-curl

cross-test: deps-cross
	./scripts/cross_test.sh "$(HOST_POLICY_CTEST_EXCLUDE)" $(CROSS_RELEASE_PRESETS)

test-cross: cross-test

cross-sanitizers: deps-cross
	./scripts/run_cross_sanitizer_matrix.sh

test-all:
	+$(TIME_STEP) test $(MAKE) test
	+$(TIME_STEP) test-host $(MAKE) test-host
ifeq ($(LONEJSON_TEST_ALL_HOST_CURL),1)
	+$(TIME_STEP) test-host-curl $(MAKE) test-host-curl
else
	@printf '%s\n' 'Skipping test-host-curl: release-matrix runs the full curl-enabled host release tests before packaging'
endif
	+$(TIME_STEP) test-cross $(MAKE) test-cross
	+$(TIME_STEP) asan $(MAKE) asan
ifeq ($(LONEJSON_HAVE_TSAN),1)
	+$(TIME_STEP) tsan $(MAKE) tsan
else
	@printf '%s\n' 'Skipping tsan: unsupported toolchain'
endif
	+$(TIME_STEP) valgrind $(MAKE) valgrind
	+$(TIME_STEP) test-e2e $(MAKE) test-e2e
	+$(TIME_STEP) fuzz-smoke $(MAKE) fuzz-smoke

test-all-bindings:
	$(MAKE) lua-test

test-install-tree: package-verify

example-smoke-local: build

lua-rock: $(LUA_ROCK_STAMP)

lua-env: lua-rock
	@$(LUAROCKS) path --tree "$(LUA_ROCK_TREE)"
	@printf 'export LD_LIBRARY_PATH=%q:$${LD_LIBRARY_PATH:-}\n' "$(LONEJSON_LUA_LIBDIR)"
	@printf 'export DYLD_LIBRARY_PATH=%q:$${DYLD_LIBRARY_PATH:-}\n' "$(LONEJSON_LUA_LIBDIR)"

$(LUA_ROCKSPEC): $(LUA_ROCK_SOURCES)
	mkdir -p "$(LUA_ROCK_TREE)"
	lib_ext="$$($(LUAROCKS) config variables.LIB_EXTENSION)"; ./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "$(LUA_ROCKSPEC)" "git+file://$(CURDIR)" "" "$$lib_ext"

$(LUA_ROCK_STAMP): $(LUA_ROCKSPEC) $(LUA_ROCK_SOURCES) $(LUA_ROCK_LIBLONEJSON_SOURCES)
	cmake --preset $(DEBUG_PRESET)
	cmake --build --preset $(DEBUG_PRESET) --target lonejson_shared
	flock "$(LUA_ROCK_BUILD_LOCK)" bash -lc 'set -e; CFLAGS="$${CFLAGS:+$$CFLAGS }$(LUA_ROCK_EXTRA_CFLAGS)" LONEJSON_LIBDIR="$(LONEJSON_LUA_LIBDIR)" "$(LUAROCKS)" make --tree "$(LUA_ROCK_TREE)" "$(LUA_ROCKSPEC)"; rm -rf $(LUA_ROCK_BUILD_BYPRODUCTS); touch "$(LUA_ROCK_STAMP)"'

lua-test: lua-rock
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) tests/test_lua.lua
	$(MAKE) lua-fuzz

lua-fuzz: lua-rock
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) tests/test_lua_fuzz.lua

lua-bench:
	@$(MAKE) lua-rock
	@cmake --preset $(HOST_PRESET) >/dev/null
	@cmake --build --preset $(HOST_PRESET) --target lonejson_shared >/dev/null
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" LD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	if [ -f "$(LUA_PERF_BASELINE)" ]; then \
		LD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua compare "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)" && \
		LD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua gate "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"; \
	fi

lua-bench-freeze-baseline: lua-rock
	@cmake --preset $(HOST_PRESET) >/dev/null
	@cmake --build --preset $(HOST_PRESET) --target lonejson_shared >/dev/null
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" LD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua freeze-baseline "$(LUA_PERF_HISTORY)" "$(LUA_PERF_BASELINE)"

lua-bench-compare:
	@$(MAKE) lua-rock
	@cmake --preset $(HOST_PRESET) >/dev/null
	@cmake --build --preset $(HOST_PRESET) --target lonejson_shared >/dev/null
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" LD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	LD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua compare "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"

lua-bench-gate:
	@$(MAKE) lua-rock
	@cmake --preset $(HOST_PRESET) >/dev/null
	@cmake --build --preset $(HOST_PRESET) --target lonejson_shared >/dev/null
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && LONEJSON_BENCH_HOST_ID="$(PERF_HOST_ID)" LD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	LD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_BENCH_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua gate "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"

asan:
	cmake --preset $(ASAN_PRESET)
	cmake --build --preset $(ASAN_PRESET)
	ctest --preset $(ASAN_PRESET) -E "$(SANITIZER_CTEST_EXCLUDE)"

tsan:
	cmake --preset $(TSAN_PRESET)
	cmake --build --preset $(TSAN_PRESET)
	ctest --preset $(TSAN_PRESET) -E "$(SANITIZER_CTEST_EXCLUDE)"

valgrind:
	@command -v valgrind >/dev/null 2>&1 || { printf '%s\n' 'Valgrind is required for the native memory-check gate' >&2; exit 1; }
	cmake --preset $(VALGRIND_PRESET)
	cmake --build --preset $(VALGRIND_PRESET) --target lonejson_tests
	valgrind --leak-check=full --track-origins=yes --error-exitcode=86 --quiet \
		./build/$(VALGRIND_PRESET)/lonejson_tests

fuzz: deps-host toolchains-aflpp
	bundle_root="$$(./scripts/detect_c_pkt_systems_bundle.sh)" && cmake --preset $(FUZZ_PRESET) -D LONEJSON_C_PKT_SYSTEMS_ROOT="$$bundle_root"
	cmake --build --preset $(FUZZ_PRESET) --target lonejson_fuzz_base64 lonejson_fuzz_validate lonejson_fuzz_mapped_parse lonejson_fuzz_array_stream lonejson_fuzz_json_value lonejson_fuzz_value_visitor lonejson_fuzz_path_value_visitor lonejson_fuzz_value_rewrite lonejson_fuzz_reader_stream_generator lonejson_fuzz_writer_generator_backpressure lonejson_fuzz_writer_value_stream lonejson_fuzz_protocol_framing lonejson_fuzz_fixed_string_paths lonejson_fuzz_alloc_ceiling lonejson_fuzz_parser_boundaries lonejson_fuzz_jwt
	cmake -D LONEJSON_COMPILE_COMMANDS="$(CURDIR)/build/$(FUZZ_PRESET)/compile_commands.json" -D LONEJSON_SOURCE_FILE="$(CURDIR)/src/lonejson.c" -D LONEJSON_AFL_COMPILER="$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^cc=//p')" -P cmake/check_fuzz_instrumentation.cmake
	$(TIME_STEP) fuzz/base64 ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" base64 ./build/$(FUZZ_PRESET)/lonejson_fuzz_base64 fuzz/corpus/base64
	$(TIME_STEP) fuzz/validate ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" validate ./build/$(FUZZ_PRESET)/lonejson_fuzz_validate tests/fixtures/vendor/json_test_suite/test_parsing tests/fixtures/spec tests/fixtures/languages
	$(TIME_STEP) fuzz/mapped ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" mapped ./build/$(FUZZ_PRESET)/lonejson_fuzz_mapped_parse fuzz/corpus/mapped tests/fixtures/spec
	$(TIME_STEP) fuzz/array ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" array ./build/$(FUZZ_PRESET)/lonejson_fuzz_array_stream fuzz/corpus/array_stream fuzz/corpus/mapped
	$(TIME_STEP) fuzz/json ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" json ./build/$(FUZZ_PRESET)/lonejson_fuzz_json_value fuzz/corpus/json_value fuzz/corpus/mapped
	$(TIME_STEP) fuzz/visitor ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" visitor ./build/$(FUZZ_PRESET)/lonejson_fuzz_value_visitor fuzz/corpus/value_visitor fuzz/corpus/json_value
	$(TIME_STEP) fuzz/path ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" path ./build/$(FUZZ_PRESET)/lonejson_fuzz_path_value_visitor fuzz/corpus/path_value_visitor fuzz/corpus/value_visitor
	$(TIME_STEP) fuzz/rewrite ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" rewrite ./build/$(FUZZ_PRESET)/lonejson_fuzz_value_rewrite fuzz/corpus/value_rewrite fuzz/corpus/json_value
	$(TIME_STEP) fuzz/reader ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" reader ./build/$(FUZZ_PRESET)/lonejson_fuzz_reader_stream_generator fuzz/corpus/mapped tests/fixtures/spec
	$(TIME_STEP) fuzz/writer ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" writer ./build/$(FUZZ_PRESET)/lonejson_fuzz_writer_generator_backpressure fuzz/corpus/mapped fuzz/corpus/json_value
	$(TIME_STEP) fuzz/value-stream ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" value-stream ./build/$(FUZZ_PRESET)/lonejson_fuzz_writer_value_stream fuzz/corpus/json_value fuzz/corpus/value_visitor
	$(TIME_STEP) fuzz/protocol ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" protocol ./build/$(FUZZ_PRESET)/lonejson_fuzz_protocol_framing fuzz/corpus/protocol_framing
	$(TIME_STEP) fuzz/fixed ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" fixed ./build/$(FUZZ_PRESET)/lonejson_fuzz_fixed_string_paths fuzz/corpus/fixed_string_paths
	$(TIME_STEP) fuzz/alloc ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" alloc ./build/$(FUZZ_PRESET)/lonejson_fuzz_alloc_ceiling fuzz/corpus/alloc_ceiling
	$(TIME_STEP) fuzz/parser ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" parser ./build/$(FUZZ_PRESET)/lonejson_fuzz_parser_boundaries fuzz/corpus/parser_boundaries
	$(TIME_STEP) fuzz/jwt ./scripts/fuzz.sh "$$($(CURDIR)/scripts/cpkt-aflpp.sh discover | sed -n 's/^afl_fuzz=//p')" "$(FUZZ_TIME)" jwt ./build/$(FUZZ_PRESET)/lonejson_fuzz_jwt fuzz/corpus/jwt
	+$(TIME_STEP) fuzz/lua $(MAKE) lua-fuzz

fuzz-long:
	$(MAKE) fuzz FUZZ_TIME=$(FUZZ_LONG_TIME)

fuzz-smoke:
	$(MAKE) fuzz FUZZ_TIME=1

stack-usage:
	cmake --preset stack-usage
	cmake --build --preset stack-usage-report

format:
	cmake --preset $(DEBUG_PRESET)
	cmake --build --preset format

deps-debug: deps-host

toolchains-aflpp:
	./scripts/cpkt-aflpp.sh ensure

deps-release: deps-all

deps-host:
	./scripts/deps.sh x86_64-linux-gnu

toolchains-x86_64-linux-gnu:
	./scripts/cpkt-toolchains.sh ensure x86_64-linux-gnu

toolchains-x86_64-linux-musl:
	./scripts/cpkt-toolchains.sh ensure x86_64-linux-musl

toolchains-aarch64-linux-gnu:
	./scripts/cpkt-toolchains.sh ensure aarch64-linux-gnu

toolchains-aarch64-linux-musl:
	./scripts/cpkt-toolchains.sh ensure aarch64-linux-musl

toolchains-armhf-linux-gnu:
	./scripts/cpkt-toolchains.sh ensure armhf-linux-gnu

toolchains-armhf-linux-musl:
	./scripts/cpkt-toolchains.sh ensure armhf-linux-musl

toolchains-all:
	./scripts/cpkt-toolchains.sh ensure all

deps-x86_64-linux-gnu: toolchains-x86_64-linux-gnu
	./scripts/deps.sh x86_64-linux-gnu

deps-x86_64-linux-musl: toolchains-x86_64-linux-musl
	./scripts/deps.sh x86_64-linux-musl

deps-aarch64-linux-gnu: toolchains-aarch64-linux-gnu
	./scripts/deps.sh aarch64-linux-gnu

deps-aarch64-linux-musl: toolchains-aarch64-linux-musl
	./scripts/deps.sh aarch64-linux-musl

deps-armhf-linux-gnu: toolchains-armhf-linux-gnu
	./scripts/deps.sh armhf-linux-gnu

deps-armhf-linux-musl: toolchains-armhf-linux-musl
	./scripts/deps.sh armhf-linux-musl

deps-arm64-apple-darwin:
	./scripts/deps.sh arm64-apple-darwin

deps-cross: \
	deps-aarch64-linux-gnu \
	deps-aarch64-linux-musl \
	deps-armhf-linux-gnu \
	deps-armhf-linux-musl

deps-all: \
	deps-x86_64-linux-gnu \
	deps-x86_64-linux-musl \
	deps-aarch64-linux-gnu \
	deps-aarch64-linux-musl \
	deps-armhf-linux-gnu \
	deps-armhf-linux-musl \
	deps-arm64-apple-darwin

certs:
	./scripts/ensure_test_certs.sh

dev-up:
	LUA="$(LUA)" ./scripts/dev-up.sh

dev-down:
	./scripts/dev-down.sh

dev-reset:
	./scripts/dev-reset.sh

dev-ps:
	./scripts/dev-ps.sh

dev-logs:
	./scripts/dev-logs.sh

compose-up:
	LUA="$(LUA)" ./scripts/dev-up.sh

compose-down:
	./scripts/dev-down.sh

compose-ps:
	./scripts/dev-ps.sh

compose-logs:
	./scripts/dev-logs.sh

curl-examples: deps-host
	./scripts/build_curl_examples.sh

ifeq ($(LONEJSON_E2E_SERVICES_READY),1)
test-curl-e2e: curl-examples
else
test-curl-e2e: compose-up curl-examples
endif
	./scripts/test_curl_e2e.sh

test-e2e:
	./scripts/test-e2e.sh

ifeq ($(LONEJSON_E2E_SERVICES_READY),1)
test-oidc-e2e: deps-host
else
test-oidc-e2e: compose-up deps-host
endif
	bundle_root="$$(./scripts/detect_c_pkt_systems_bundle.sh)" && cmake --preset host-curl -D LONEJSON_C_PKT_SYSTEMS_ROOT="$$bundle_root"
	cmake --build --preset host-curl --target lonejson_oidc_fixture_server
	./scripts/test_oidc_e2e.sh

test-m2m-e2e: deps-host
	bundle_root="$$(./scripts/detect_c_pkt_systems_bundle.sh)" && cmake --preset host-curl -D LONEJSON_C_PKT_SYSTEMS_ROOT="$$bundle_root"
	cmake --build --preset host-curl --target lonejson_m2m_fixture_server
	./scripts/test_m2m_e2e.sh

clean:
	./scripts/clean.sh

clean-dist:
	./scripts/clean.sh --dist-only
