SHELL := bash
.DEFAULT_GOAL := help
MAKEFLAGS += --no-builtin-rules

DEBUG_PRESET := debug
HOST_PRESET := host
ASAN_PRESET := asan
TSAN_PRESET := tsan
MSAN_PRESET := msan
FUZZ_PRESET := fuzz
LONEJSON_HAVE_CLANG ?= $(shell if command -v clang >/dev/null 2>&1; then printf '1'; else printf '0'; fi)
LONEJSON_HAVE_TSAN ?= $(shell bash "$(CURDIR)/scripts/check_clang_sanitizer_support.sh" thread)
LONEJSON_HAVE_MSAN ?= $(shell bash "$(CURDIR)/scripts/check_clang_sanitizer_support.sh" memory)
RELEASE_BUILD_PRESETS := \
	linux-gnu-release \
	linux-musl-release \
	aarch64-linux-gnu-release \
	aarch64-linux-musl-release \
	armhf-linux-gnu-release \
	armhf-linux-musl-release \
	$(shell if [ -x "$${OSXCROSS_ROOT:-$$HOME/.local/cross/osxcross}/bin/arm64-apple-darwin25-clang" ]; then printf '%s' arm64-apple-darwin-release; fi)
CROSS_RELEASE_PRESETS := \
	aarch64-linux-gnu-release \
	aarch64-linux-musl-release \
	armhf-linux-gnu-release \
	armhf-linux-musl-release
LUA ?= lua
LUAROCKS ?= luarocks
GENERATED_FIXTURE_DIR := $(CURDIR)/build/generated/fixtures
COMPOSE := $(shell if command -v nerdctl >/dev/null 2>&1; then printf '%s' 'nerdctl compose'; elif command -v docker >/dev/null 2>&1; then printf '%s' 'docker compose'; fi)
RELEASE_VERSION := $(shell ./scripts/release_version.sh)
DIST_DIR := $(CURDIR)/dist
RELEASE_SOURCE_TARBALL := $(DIST_DIR)/lonejson-$(RELEASE_VERSION).tar.gz
RELEASE_HEADER_GZ := $(DIST_DIR)/lonejson-$(RELEASE_VERSION).h.gz
RELEASE_ROCKSPEC := $(DIST_DIR)/lonejson-$(RELEASE_VERSION)-1.rockspec
RELEASE_PACK_DIR := $(DIST_DIR)/.pack
RELEASE_PACK_STAGE_DIR := $(RELEASE_PACK_DIR)/lonejson-$(RELEASE_VERSION)
RELEASE_PACK_SOURCE_TARBALL := $(RELEASE_PACK_DIR)/lonejson-$(RELEASE_VERSION).tar.gz
RELEASE_PACK_ROCKSPEC := $(RELEASE_PACK_DIR)/lonejson-$(RELEASE_VERSION)-1.rockspec
RELEASE_ROCK := $(DIST_DIR)/lonejson-$(RELEASE_VERSION)-1.src.rock
RELEASE_CHECKSUMS := $(DIST_DIR)/lonejson-$(RELEASE_VERSION)-CHECKSUMS
PERF_CORPUS := tests/fixtures/vendor/json_test_suite/test_parsing
PERF_LATEST := $(CURDIR)/perflogs/latest.json
PERF_HISTORY := $(CURDIR)/perflogs/history.jsonl
PERF_BASELINE := $(CURDIR)/perflogs/baseline.json
PERF_ARCHIVE_DIR := $(CURDIR)/perflogs/runs
LUA_PERF_LATEST := $(CURDIR)/perflogs/lua/latest.json
LUA_PERF_HISTORY := $(CURDIR)/perflogs/lua/history.jsonl
LUA_PERF_BASELINE := $(CURDIR)/perflogs/lua/baseline.json
LUA_PERF_ARCHIVE_DIR := $(CURDIR)/perflogs/lua/runs
PERF_ITERATIONS ?= 40
LUA_PERF_ITERATIONS ?= 30
FUZZ_TIME ?= 30
FUZZ_LONG_TIME ?= 300
FUZZ_VALIDATE_MAX_LEN ?= 131072
FUZZ_MAPPED_MAX_LEN ?= 262144
FUZZ_ARRAY_STREAM_MAX_LEN ?= 262144
FUZZ_JSON_VALUE_MAX_LEN ?= 524288
FUZZ_VALUE_VISITOR_MAX_LEN ?= 524288
FUZZ_VALUE_REWRITE_MAX_LEN ?= 524288
FUZZ_READER_STREAM_GENERATOR_MAX_LEN ?= 262144
FUZZ_WRITER_GENERATOR_MAX_LEN ?= 262144
FUZZ_WRITER_VALUE_STREAM_MAX_LEN ?= 262144
FUZZ_PROTOCOL_FRAMING_MAX_LEN ?= 262144
FUZZ_FIXED_STRING_PATHS_MAX_LEN ?= 65536
FUZZ_ALLOC_CEILING_MAX_LEN ?= 65536
FUZZ_PARSER_BOUNDARIES_MAX_LEN ?= 131072
FUZZ_LARGE_SEEDS := \
	fuzz/corpus/mapped/person_large_payload.json \
	fuzz/corpus/json_value/large_selector_payload.json \
	fuzz/corpus/value_visitor/large_unicode_payload.json
FUZZ_GENERATED_DIR := fuzz/generated
FUZZ_VALIDATE_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/validate
FUZZ_MAPPED_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/mapped
FUZZ_ARRAY_STREAM_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/array_stream
FUZZ_JSON_VALUE_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/json_value
FUZZ_VALUE_VISITOR_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/value_visitor
FUZZ_VALUE_REWRITE_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/value_rewrite
FUZZ_READER_STREAM_GENERATOR_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/reader_stream_generator
FUZZ_WRITER_GENERATOR_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/writer_generator
FUZZ_WRITER_VALUE_STREAM_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/writer_value_stream
FUZZ_PROTOCOL_FRAMING_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/protocol_framing
FUZZ_FIXED_STRING_PATHS_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/fixed_string_paths
FUZZ_ALLOC_CEILING_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/alloc_ceiling
FUZZ_PARSER_BOUNDARIES_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/parser_boundaries
FUZZ_VALIDATE_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/validate
FUZZ_MAPPED_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/mapped
FUZZ_ARRAY_STREAM_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/array_stream
FUZZ_JSON_VALUE_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/json_value
FUZZ_VALUE_VISITOR_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/value_visitor
FUZZ_VALUE_REWRITE_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/value_rewrite
FUZZ_READER_STREAM_GENERATOR_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/reader_stream_generator
FUZZ_WRITER_GENERATOR_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/writer_generator
FUZZ_WRITER_VALUE_STREAM_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/writer_value_stream
FUZZ_PROTOCOL_FRAMING_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/protocol_framing
FUZZ_FIXED_STRING_PATHS_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/fixed_string_paths
FUZZ_ALLOC_CEILING_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/alloc_ceiling
FUZZ_PARSER_BOUNDARIES_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/parser_boundaries
LUA_ROCK_TREE := build/luarocks
LUA_ROCKSPEC := $(LUA_ROCK_TREE)/lonejson-$(RELEASE_VERSION)-1.rockspec
LUA_ROCK_STAMP := $(LUA_ROCK_TREE)/.installed.stamp
LUA_ROCK_BUILD_LOCK := $(LUA_ROCK_TREE)/.build.lock
LUA_ROCK_EXTRA_CFLAGS ?= -O3 -DNDEBUG -D_FILE_OFFSET_BITS=64 -fno-semantic-interposition
LONEJSON_LUA_LIBDIR ?= $(CURDIR)/build/$(DEBUG_PRESET)
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
	lua/lonejson/init.lua

SANITIZER_CTEST_EXCLUDE := lonejson_(bench_baseline_history_tests|bench_retry_confirm_tests|lua_legacy_uservalue_tests|lua_schema_cache_tests|lua_encode_stats_tests|c_pkt_systems_fetch_retry_tests|cmake_threads_optional_tests|run_release_matrix_darwin_target_tests)

.PHONY: \
	help \
	build \
	build-host \
	build-release \
	release-lua-artifacts \
	release-source-artifact \
	release-source-smoke \
	release-darwin-smoke-bundle \
	release \
	lua-rock \
	lua-test \
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
	test-host \
	test-host-curl \
	test-cross \
	test-all \
	test-all-bindings \
	asan \
	tsan \
	msan \
	fuzz-smoke \
	fuzz \
	fuzz-long \
	stack-usage \
	format \
	deps-host \
	deps-x86_64-linux-gnu \
	deps-x86_64-linux-musl \
	deps-aarch64-linux-gnu \
	deps-aarch64-linux-musl \
	deps-armhf-linux-gnu \
	deps-armhf-linux-musl \
	deps-arm64-apple-darwin \
	deps-cross \
	deps-all \
	certs \
	compose-up \
	compose-down \
	compose-logs \
	curl-examples \
	test-curl-e2e \
	clean \
	clean-dist

help:
	@printf '%s\n' \
		'make build                  Configure and build the full debug tree (tests and standalone examples, excluding lua-* and curl-examples).' \
		'make build-host             Configure and build the host-native release preset.' \
		'make build-release          Configure and build the full shipped release test matrix.' \
		'make release                Run the release matrix and package generation.' \
		'make release-source-smoke   Unpack the source release tarball into a temp tree, then run host C/Lua tests and Lua artifact packaging there.' \
		'make release-darwin-smoke-bundle Build the Darwin smoke ZIP with example and link-smoke binaries.' \
		'make lua-rock               Generate a local rockspec in build/luarocks and install the Lua module there.' \
		'make lua-test               Build the Lua module and run the Lua integration test.' \
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
		'make test                   Build and run the debug test preset.' \
		'make test-host              Build and run the host-native test preset.' \
		'make test-host-curl         Build and run the host-native curl-enabled test preset.' \
		'make test-cross             Configure, build, and run all cross release test presets serially.' \
		'make test-all               Run debug, host, host-curl, cross, ASan, benchmark gates, and fuzz-smoke serially; TSan/MSan are included when clang is available.' \
		'make test-all-bindings      Run test-all plus the optional Lua binding suite.' \
		'make asan                   Build and run the ASan/UBSan preset.' \
		'make tsan                   Build the TSan preset and run the pure-C CTest subset that does not depend on external unsanitized runtimes.' \
		'make msan                   Build the MSan preset and run the pure-C CTest subset that does not depend on external unsanitized runtimes.' \
		'make fuzz-smoke             Build all libFuzzer targets and run a seeded 1s smoke pass for each.' \
		'make fuzz                   Build all libFuzzer targets and run a seeded 30s pass for each with explicit large-input caps; missing large synthetic seeds are regenerated automatically.' \
		'make fuzz-long              Run the same fuzz targets with a several-minute soak per target.' \
		'make stack-usage            Build with compiler stack-usage reporting and print the report.' \
		'make format                 Run clang-format over the C sources.' \
		'make deps-host              Download and extract the host-native c.pkt.systems dependency bundle.' \
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
		'make compose-up             Start the local nginx + sink HTTP/HTTPS test rig.' \
		'make compose-down           Stop and remove the local compose stack.' \
		'make compose-logs           Tail logs from the local compose stack.' \
		'make curl-examples          Build the curl examples against the host c.pkt.systems dependency bundle.' \
		'make test-curl-e2e          Build and run the curl examples against the local HTTPS rig.' \
		'make release-source-artifact Build the source-only release tarball in dist/.' \
		'make clean                  Remove build/, dist/, .deps/, examples/bin/, and generated Lua module artifacts.' \
		'make clean-dist             Remove dist/ release artifacts only.'

build:
	cmake --preset $(DEBUG_PRESET)
	cmake --build --preset $(DEBUG_PRESET)
	./scripts/stage_standalone_examples.sh

build-host:
	cmake --preset $(HOST_PRESET)
	cmake --build --preset $(HOST_PRESET)

build-release: deps-all
	@set -e; for preset in $(RELEASE_BUILD_PRESETS); do \
		cmake --preset "$$preset"; \
		cmake --build --preset "$$preset"; \
	done

$(DIST_DIR):
	mkdir -p "$(DIST_DIR)"

$(RELEASE_PACK_DIR):
	mkdir -p "$(RELEASE_PACK_DIR)"

$(RELEASE_PACK_STAGE_DIR): Makefile $(LUA_ROCK_SOURCES) | $(RELEASE_PACK_DIR)
	./scripts/stage_lua_rock_sources.sh "$(CURDIR)" "$(RELEASE_PACK_STAGE_DIR)" "$(RELEASE_VERSION)"

$(RELEASE_PACK_SOURCE_TARBALL): $(RELEASE_PACK_STAGE_DIR)
	rm -f "$(RELEASE_PACK_DIR)/lonejson-$(RELEASE_VERSION).tar" "$(RELEASE_PACK_SOURCE_TARBALL)"
	cd "$(RELEASE_PACK_DIR)" && tar -cf "lonejson-$(RELEASE_VERSION).tar" "lonejson-$(RELEASE_VERSION)"
	gzip -9 -f "$(RELEASE_PACK_DIR)/lonejson-$(RELEASE_VERSION).tar"

$(RELEASE_ROCKSPEC): lonejson.rockspec.in scripts/render_release_rockspec.sh | $(DIST_DIR)
	lib_ext="$$($(LUAROCKS) config variables.LIB_EXTENSION)"; ./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "$(RELEASE_ROCKSPEC)" "" "" "$$lib_ext"

$(RELEASE_PACK_ROCKSPEC): Makefile $(RELEASE_PACK_SOURCE_TARBALL)
	cd "$(RELEASE_PACK_STAGE_DIR)" && lib_ext="$$($(LUAROCKS) config variables.LIB_EXTENSION)" && ./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "../$(notdir $(RELEASE_PACK_ROCKSPEC))" "file://$(notdir $(RELEASE_PACK_SOURCE_TARBALL))" "" "$$lib_ext"

$(RELEASE_ROCK): $(RELEASE_PACK_ROCKSPEC) $(RELEASE_ROCKSPEC) scripts/package_lua_src_rock.sh scripts/smoke_lua_src_rock.sh
	./scripts/package_lua_src_rock.sh "$(RELEASE_ROCK)" "$(RELEASE_PACK_ROCKSPEC)" "$(RELEASE_PACK_SOURCE_TARBALL)"
	cmake --preset $(DEBUG_PRESET)
	cmake --build --preset $(DEBUG_PRESET) --target lonejson_shared
	./scripts/smoke_lua_src_rock.sh "$(RELEASE_ROCK)" "$(LONEJSON_LUA_LIBDIR)"
	rm -rf "$(RELEASE_PACK_DIR)"

release-lua-artifacts: $(RELEASE_ROCKSPEC) $(RELEASE_ROCK)

release-source-artifact:
	cmake --preset $(HOST_PRESET)
	cmake --build --preset package-source

$(RELEASE_SOURCE_TARBALL):
	$(MAKE) release-source-artifact

release-source-smoke: release-source-artifact
	./scripts/test_release_from_source.sh "$(CURDIR)" "$(DIST_DIR)/lonejson-$(RELEASE_VERSION).tar.gz"
	cmake --build --preset package-checksums

release-darwin-smoke-bundle: deps-arm64-apple-darwin
	cmake --preset arm64-apple-darwin-release
	cmake --build --preset arm64-apple-darwin-release --target package-darwin-smoke-bundle

release:
	./scripts/run_release_matrix.sh

bench:
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	if [ -f "$(PERF_BASELINE)" ]; then \
		./build/$(HOST_PRESET)/lonejson_bench compare "$(PERF_BASELINE)" "$(PERF_LATEST)" && \
		./build/$(HOST_PRESET)/lonejson_bench gate "$(PERF_BASELINE)" "$(PERF_LATEST)"; \
	fi

bench-check:
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
	export LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" && \
	export DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" && \
	cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$$c_latest" "$$c_history" "$$c_runs" "$(PERF_ITERATIONS)" && \
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

bench-freeze-baseline:
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	./build/$(HOST_PRESET)/lonejson_bench freeze-baseline "$(PERF_HISTORY)" "$(PERF_BASELINE)"

bench-compare:
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	./build/$(HOST_PRESET)/lonejson_bench compare "$(PERF_BASELINE)" "$(PERF_LATEST)"

bench-baseline-history:
	@$(MAKE) --no-print-directory lua-rock >/dev/null
	@eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && \
		$(LUA) scripts/bench_baseline_history.lua --repo "$(CURDIR)"

bench-gate:
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	./build/$(HOST_PRESET)/lonejson_bench gate "$(PERF_BASELINE)" "$(PERF_LATEST)"

test: build
	ctest --preset $(DEBUG_PRESET)
	$(MAKE) lua-test

test-host: build-host
	ctest --preset $(HOST_PRESET)
	$(MAKE) lua-test

test-host-curl: deps-host
	bundle_root="$$(./scripts/detect_c_pkt_systems_bundle.sh)" && cmake --preset host-curl -D LONEJSON_C_PKT_SYSTEMS_ROOT="$$bundle_root"
	cmake --build --preset host-curl
	ctest --preset host-curl

test-cross: deps-cross
	@set -e; for preset in $(CROSS_RELEASE_PRESETS); do \
		cmake --preset "$$preset"; \
		cmake --build --preset "$$preset"; \
		ctest --preset "$$preset" --output-on-failure; \
	done

test-all:
	$(MAKE) test
	$(MAKE) test-host
	$(MAKE) test-host-curl
	$(MAKE) test-cross
	$(MAKE) asan
ifeq ($(LONEJSON_HAVE_TSAN),1)
	$(MAKE) tsan
else
	@printf '%s\n' 'Skipping tsan: unsupported toolchain'
endif
ifeq ($(LONEJSON_HAVE_MSAN),1)
	$(MAKE) msan
else
	@printf '%s\n' 'Skipping msan: unsupported toolchain'
endif
	$(MAKE) bench-check
	$(MAKE) fuzz-smoke

test-all-bindings:
	$(MAKE) test-all
	$(MAKE) lua-test

lua-rock: $(LUA_ROCK_STAMP)

$(LUA_ROCKSPEC): $(LUA_ROCK_SOURCES)
	mkdir -p "$(LUA_ROCK_TREE)"
	lib_ext="$$($(LUAROCKS) config variables.LIB_EXTENSION)"; ./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "$(LUA_ROCKSPEC)" "git+file://$(CURDIR)" "" "$$lib_ext"

$(LUA_ROCK_STAMP): $(LUA_ROCKSPEC) $(LUA_ROCK_SOURCES)
	cmake --build --preset $(DEBUG_PRESET) --target lonejson_shared
	flock "$(LUA_ROCK_BUILD_LOCK)" bash -lc 'set -e; CFLAGS="$${CFLAGS:+$$CFLAGS }$(LUA_ROCK_EXTRA_CFLAGS)" LONEJSON_LIBDIR="$(LONEJSON_LUA_LIBDIR)" "$(LUAROCKS)" make --tree "$(LUA_ROCK_TREE)" "$(LUA_ROCKSPEC)"; rm -rf $(LUA_ROCK_BUILD_BYPRODUCTS); touch "$(LUA_ROCK_STAMP)"'

lua-test: lua-rock
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) tests/test_lua.lua
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) tests/test_lua_fuzz.lua

lua-bench:
	@$(MAKE) lua-rock
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	if [ -f "$(LUA_PERF_BASELINE)" ]; then \
		LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua compare "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)" && \
		LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua gate "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"; \
	fi

lua-bench-freeze-baseline: lua-rock
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua freeze-baseline "$(LUA_PERF_HISTORY)" "$(LUA_PERF_BASELINE)"

lua-bench-compare:
	@$(MAKE) lua-rock
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua compare "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"

lua-bench-gate:
	@$(MAKE) lua-rock
	eval "$$($(LUAROCKS) path --tree $(LUA_ROCK_TREE))" && LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	LD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${LD_LIBRARY_PATH:-}" DYLD_LIBRARY_PATH="$(LONEJSON_LUA_LIBDIR):$${DYLD_LIBRARY_PATH:-}" $(LUA) bench/lonejson_lua_bench.lua gate "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"

asan:
	cmake --preset $(ASAN_PRESET)
	cmake --build --preset $(ASAN_PRESET)
	ctest --preset $(ASAN_PRESET)

tsan:
	cmake --preset $(TSAN_PRESET)
	cmake --build --preset $(TSAN_PRESET)
	ctest --preset $(TSAN_PRESET) -E "$(SANITIZER_CTEST_EXCLUDE)"

msan:
	cmake --preset $(MSAN_PRESET)
	cmake --build --preset $(MSAN_PRESET)
	MSAN_OPTIONS=halt_on_error=1:abort_on_error=1:exit_code=86 ctest --preset $(MSAN_PRESET) -E "$(SANITIZER_CTEST_EXCLUDE)"

fuzz:
	@missing=0; for seed in $(FUZZ_LARGE_SEEDS); do \
		if [ ! -s "$$seed" ]; then \
			missing=1; \
			break; \
		fi; \
	done; \
	if [ "$$missing" -ne 0 ]; then \
		./scripts/generate_fuzz_large_seeds.sh; \
	fi
	cmake --preset $(FUZZ_PRESET)
	cmake --build --preset $(FUZZ_PRESET) --target lonejson_fuzz_validate lonejson_fuzz_mapped_parse lonejson_fuzz_array_stream lonejson_fuzz_json_value lonejson_fuzz_value_visitor lonejson_fuzz_value_rewrite lonejson_fuzz_reader_stream_generator lonejson_fuzz_writer_generator_backpressure lonejson_fuzz_writer_value_stream lonejson_fuzz_protocol_framing lonejson_fuzz_fixed_string_paths lonejson_fuzz_alloc_ceiling lonejson_fuzz_parser_boundaries
	cmake -D LONEJSON_COMPILE_COMMANDS="$(CURDIR)/build/$(FUZZ_PRESET)/compile_commands.json" -D LONEJSON_SOURCE_FILE="$(CURDIR)/src/lonejson.c" -P cmake/check_fuzz_instrumentation.cmake
	cmake -E rm -rf "$(FUZZ_VALIDATE_CORPUS_DIR)" "$(FUZZ_MAPPED_CORPUS_DIR)" "$(FUZZ_ARRAY_STREAM_CORPUS_DIR)" "$(FUZZ_JSON_VALUE_CORPUS_DIR)" "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)" "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)" "$(FUZZ_READER_STREAM_GENERATOR_CORPUS_DIR)" "$(FUZZ_WRITER_GENERATOR_CORPUS_DIR)" "$(FUZZ_WRITER_VALUE_STREAM_CORPUS_DIR)" "$(FUZZ_PROTOCOL_FRAMING_CORPUS_DIR)" "$(FUZZ_FIXED_STRING_PATHS_CORPUS_DIR)" "$(FUZZ_ALLOC_CEILING_CORPUS_DIR)" "$(FUZZ_PARSER_BOUNDARIES_CORPUS_DIR)"
	cmake -E make_directory "$(FUZZ_VALIDATE_GENERATED_DIR)" "$(FUZZ_MAPPED_GENERATED_DIR)" "$(FUZZ_ARRAY_STREAM_GENERATED_DIR)" "$(FUZZ_JSON_VALUE_GENERATED_DIR)" "$(FUZZ_VALUE_VISITOR_GENERATED_DIR)" "$(FUZZ_VALUE_REWRITE_GENERATED_DIR)" "$(FUZZ_READER_STREAM_GENERATOR_GENERATED_DIR)" "$(FUZZ_WRITER_GENERATOR_GENERATED_DIR)" "$(FUZZ_WRITER_VALUE_STREAM_GENERATED_DIR)" "$(FUZZ_PROTOCOL_FRAMING_GENERATED_DIR)" "$(FUZZ_FIXED_STRING_PATHS_GENERATED_DIR)" "$(FUZZ_ALLOC_CEILING_GENERATED_DIR)" "$(FUZZ_PARSER_BOUNDARIES_GENERATED_DIR)"
	cmake -E make_directory "$(FUZZ_VALIDATE_CORPUS_DIR)/vendor" "$(FUZZ_VALIDATE_CORPUS_DIR)/spec" "$(FUZZ_VALIDATE_CORPUS_DIR)/languages"
	cmake -E make_directory "$(FUZZ_MAPPED_CORPUS_DIR)/mapped" "$(FUZZ_MAPPED_CORPUS_DIR)/spec" "$(FUZZ_MAPPED_CORPUS_DIR)/languages"
	cmake -E make_directory "$(FUZZ_ARRAY_STREAM_CORPUS_DIR)/array_stream" "$(FUZZ_ARRAY_STREAM_CORPUS_DIR)/mapped" "$(FUZZ_ARRAY_STREAM_CORPUS_DIR)/spec"
	cmake -E make_directory "$(FUZZ_JSON_VALUE_CORPUS_DIR)/json_value" "$(FUZZ_JSON_VALUE_CORPUS_DIR)/mapped" "$(FUZZ_JSON_VALUE_CORPUS_DIR)/value_visitor"
	cmake -E make_directory "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/value_visitor" "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/json_value" "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/languages"
	cmake -E make_directory "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)/value_rewrite" "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)/json_value" "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)/mapped" "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)/spec"
	cmake -E make_directory "$(FUZZ_READER_STREAM_GENERATOR_CORPUS_DIR)/mapped" "$(FUZZ_READER_STREAM_GENERATOR_CORPUS_DIR)/spec" "$(FUZZ_READER_STREAM_GENERATOR_CORPUS_DIR)/languages"
	cmake -E make_directory "$(FUZZ_WRITER_GENERATOR_CORPUS_DIR)/mapped" "$(FUZZ_WRITER_GENERATOR_CORPUS_DIR)/json_value" "$(FUZZ_WRITER_GENERATOR_CORPUS_DIR)/spec"
	cmake -E make_directory "$(FUZZ_WRITER_VALUE_STREAM_CORPUS_DIR)/json_value" "$(FUZZ_WRITER_VALUE_STREAM_CORPUS_DIR)/value_visitor" "$(FUZZ_WRITER_VALUE_STREAM_CORPUS_DIR)/spec"
	cmake -E make_directory "$(FUZZ_PROTOCOL_FRAMING_CORPUS_DIR)/protocol_framing"
	cmake -E make_directory "$(FUZZ_FIXED_STRING_PATHS_CORPUS_DIR)/fixed_string_paths"
	cmake -E make_directory "$(FUZZ_ALLOC_CEILING_CORPUS_DIR)/alloc_ceiling"
	cmake -E make_directory "$(FUZZ_PARSER_BOUNDARIES_CORPUS_DIR)/parser_boundaries"
	cp -R tests/fixtures/vendor/json_test_suite/test_parsing/. "$(FUZZ_VALIDATE_CORPUS_DIR)/vendor/"
	cp -R tests/fixtures/spec/. "$(FUZZ_VALIDATE_CORPUS_DIR)/spec/"
	cp -R tests/fixtures/languages/. "$(FUZZ_VALIDATE_CORPUS_DIR)/languages/"
	cp -R fuzz/corpus/mapped/. "$(FUZZ_MAPPED_CORPUS_DIR)/mapped/"
	cp -R tests/fixtures/spec/. "$(FUZZ_MAPPED_CORPUS_DIR)/spec/"
	cp -R tests/fixtures/languages/. "$(FUZZ_MAPPED_CORPUS_DIR)/languages/"
	cp -R fuzz/corpus/array_stream/. "$(FUZZ_ARRAY_STREAM_CORPUS_DIR)/array_stream/"
	cp -R fuzz/corpus/mapped/. "$(FUZZ_ARRAY_STREAM_CORPUS_DIR)/mapped/"
	cp -R tests/fixtures/spec/. "$(FUZZ_ARRAY_STREAM_CORPUS_DIR)/spec/"
	cp -R fuzz/corpus/json_value/. "$(FUZZ_JSON_VALUE_CORPUS_DIR)/json_value/"
	cp -R fuzz/corpus/mapped/. "$(FUZZ_JSON_VALUE_CORPUS_DIR)/mapped/"
	cp -R fuzz/corpus/value_visitor/. "$(FUZZ_JSON_VALUE_CORPUS_DIR)/value_visitor/"
	cp -R fuzz/corpus/value_visitor/. "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/value_visitor/"
	cp -R fuzz/corpus/json_value/. "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/json_value/"
	cp -R tests/fixtures/languages/. "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/languages/"
	cp -R fuzz/corpus/value_rewrite/. "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)/value_rewrite/"
	cp -R fuzz/corpus/json_value/. "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)/json_value/"
	cp -R fuzz/corpus/mapped/. "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)/mapped/"
	cp -R tests/fixtures/spec/. "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)/spec/"
	cp -R fuzz/corpus/mapped/. "$(FUZZ_READER_STREAM_GENERATOR_CORPUS_DIR)/mapped/"
	cp -R tests/fixtures/spec/. "$(FUZZ_READER_STREAM_GENERATOR_CORPUS_DIR)/spec/"
	cp -R tests/fixtures/languages/. "$(FUZZ_READER_STREAM_GENERATOR_CORPUS_DIR)/languages/"
	cp -R fuzz/corpus/mapped/. "$(FUZZ_WRITER_GENERATOR_CORPUS_DIR)/mapped/"
	cp -R fuzz/corpus/json_value/. "$(FUZZ_WRITER_GENERATOR_CORPUS_DIR)/json_value/"
	cp -R tests/fixtures/spec/. "$(FUZZ_WRITER_GENERATOR_CORPUS_DIR)/spec/"
	cp -R fuzz/corpus/json_value/. "$(FUZZ_WRITER_VALUE_STREAM_CORPUS_DIR)/json_value/"
	cp -R fuzz/corpus/value_visitor/. "$(FUZZ_WRITER_VALUE_STREAM_CORPUS_DIR)/value_visitor/"
	cp -R tests/fixtures/spec/. "$(FUZZ_WRITER_VALUE_STREAM_CORPUS_DIR)/spec/"
	cp -R fuzz/corpus/protocol_framing/. "$(FUZZ_PROTOCOL_FRAMING_CORPUS_DIR)/protocol_framing/"
	cp -R fuzz/corpus/fixed_string_paths/. "$(FUZZ_FIXED_STRING_PATHS_CORPUS_DIR)/fixed_string_paths/"
	cp -R fuzz/corpus/alloc_ceiling/. "$(FUZZ_ALLOC_CEILING_CORPUS_DIR)/alloc_ceiling/"
	cp -R fuzz/corpus/parser_boundaries/. "$(FUZZ_PARSER_BOUNDARIES_CORPUS_DIR)/parser_boundaries/"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/validate"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/mapped"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/array_stream"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/json_value"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/value_visitor"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/value_rewrite"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/reader_stream_generator"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/writer_generator"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/writer_value_stream"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/protocol_framing"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/fixed_string_paths"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/alloc_ceiling"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/parser_boundaries"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_validate -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_VALIDATE_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/validate/ "$(FUZZ_VALIDATE_GENERATED_DIR)" "$(FUZZ_VALIDATE_CORPUS_DIR)/vendor" "$(FUZZ_VALIDATE_CORPUS_DIR)/spec" "$(FUZZ_VALIDATE_CORPUS_DIR)/languages"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_mapped_parse -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_MAPPED_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/mapped/ "$(FUZZ_MAPPED_GENERATED_DIR)" "$(FUZZ_MAPPED_CORPUS_DIR)/mapped" "$(FUZZ_MAPPED_CORPUS_DIR)/spec" "$(FUZZ_MAPPED_CORPUS_DIR)/languages"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_array_stream -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_ARRAY_STREAM_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/array_stream/ "$(FUZZ_ARRAY_STREAM_GENERATED_DIR)" "$(FUZZ_ARRAY_STREAM_CORPUS_DIR)/array_stream" "$(FUZZ_ARRAY_STREAM_CORPUS_DIR)/mapped" "$(FUZZ_ARRAY_STREAM_CORPUS_DIR)/spec"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_json_value -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_JSON_VALUE_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/json_value/ "$(FUZZ_JSON_VALUE_GENERATED_DIR)" "$(FUZZ_JSON_VALUE_CORPUS_DIR)/json_value" "$(FUZZ_JSON_VALUE_CORPUS_DIR)/mapped" "$(FUZZ_JSON_VALUE_CORPUS_DIR)/value_visitor"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_value_visitor -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_VALUE_VISITOR_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/value_visitor/ "$(FUZZ_VALUE_VISITOR_GENERATED_DIR)" "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/value_visitor" "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/json_value" "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/languages"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_value_rewrite -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_VALUE_REWRITE_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/value_rewrite/ "$(FUZZ_VALUE_REWRITE_GENERATED_DIR)" "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)/value_rewrite" "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)/json_value" "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)/mapped" "$(FUZZ_VALUE_REWRITE_CORPUS_DIR)/spec"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_reader_stream_generator -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_READER_STREAM_GENERATOR_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/reader_stream_generator/ "$(FUZZ_READER_STREAM_GENERATOR_GENERATED_DIR)" "$(FUZZ_READER_STREAM_GENERATOR_CORPUS_DIR)/mapped" "$(FUZZ_READER_STREAM_GENERATOR_CORPUS_DIR)/spec" "$(FUZZ_READER_STREAM_GENERATOR_CORPUS_DIR)/languages"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_writer_generator_backpressure -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_WRITER_GENERATOR_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/writer_generator/ "$(FUZZ_WRITER_GENERATOR_GENERATED_DIR)" "$(FUZZ_WRITER_GENERATOR_CORPUS_DIR)/mapped" "$(FUZZ_WRITER_GENERATOR_CORPUS_DIR)/json_value" "$(FUZZ_WRITER_GENERATOR_CORPUS_DIR)/spec"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_writer_value_stream -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_WRITER_VALUE_STREAM_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/writer_value_stream/ "$(FUZZ_WRITER_VALUE_STREAM_GENERATED_DIR)" "$(FUZZ_WRITER_VALUE_STREAM_CORPUS_DIR)/json_value" "$(FUZZ_WRITER_VALUE_STREAM_CORPUS_DIR)/value_visitor" "$(FUZZ_WRITER_VALUE_STREAM_CORPUS_DIR)/spec"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_protocol_framing -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_PROTOCOL_FRAMING_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/protocol_framing/ "$(FUZZ_PROTOCOL_FRAMING_GENERATED_DIR)" "$(FUZZ_PROTOCOL_FRAMING_CORPUS_DIR)/protocol_framing"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_fixed_string_paths -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_FIXED_STRING_PATHS_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/fixed_string_paths/ "$(FUZZ_FIXED_STRING_PATHS_GENERATED_DIR)" "$(FUZZ_FIXED_STRING_PATHS_CORPUS_DIR)/fixed_string_paths"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_alloc_ceiling -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_ALLOC_CEILING_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/alloc_ceiling/ "$(FUZZ_ALLOC_CEILING_GENERATED_DIR)" "$(FUZZ_ALLOC_CEILING_CORPUS_DIR)/alloc_ceiling"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_parser_boundaries -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_PARSER_BOUNDARIES_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/parser_boundaries/ "$(FUZZ_PARSER_BOUNDARIES_GENERATED_DIR)" "$(FUZZ_PARSER_BOUNDARIES_CORPUS_DIR)/parser_boundaries"

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

deps-host:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -P cmake/fetch_c_pkt_systems.cmake

deps-x86_64-linux-gnu:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=x86_64-linux-gnu -P cmake/fetch_c_pkt_systems.cmake

deps-x86_64-linux-musl:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=x86_64-linux-musl -P cmake/fetch_c_pkt_systems.cmake

deps-aarch64-linux-gnu:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=aarch64-linux-gnu -P cmake/fetch_c_pkt_systems.cmake

deps-aarch64-linux-musl:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=aarch64-linux-musl -P cmake/fetch_c_pkt_systems.cmake

deps-armhf-linux-gnu:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=armhf-linux-gnu -P cmake/fetch_c_pkt_systems.cmake

deps-armhf-linux-musl:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=armhf-linux-musl -P cmake/fetch_c_pkt_systems.cmake

deps-arm64-apple-darwin:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=arm64-apple-darwin -P cmake/fetch_c_pkt_systems.cmake

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

compose-up:
	@test -n "$(COMPOSE)" || (printf '%s\n' 'Neither nerdctl nor docker was found in PATH.' >&2; exit 1)
	$(MAKE) certs
	./scripts/ensure_large_fixtures.sh "$(LUA)" "./scripts/generate_large_fixtures.lua" "$(GENERATED_FIXTURE_DIR)"
	$(LUA) ./scripts/generate_large_fixtures.lua ./docker/nginx/generated/variants
	$(COMPOSE) -f docker-compose.yml up -d --build

compose-down:
	@test -n "$(COMPOSE)" || (printf '%s\n' 'Neither nerdctl nor docker was found in PATH.' >&2; exit 1)
	$(COMPOSE) -f docker-compose.yml down --remove-orphans

compose-logs:
	@test -n "$(COMPOSE)" || (printf '%s\n' 'Neither nerdctl nor docker was found in PATH.' >&2; exit 1)
	$(COMPOSE) -f docker-compose.yml logs -f

curl-examples: deps-host
	./scripts/build_curl_examples.sh

test-curl-e2e: curl-examples
	./scripts/test_curl_e2e.sh

clean:
	./scripts/clean.sh

clean-dist:
	./scripts/clean.sh --dist-only
