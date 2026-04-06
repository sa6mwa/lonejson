SHELL := bash
.DEFAULT_GOAL := help
MAKEFLAGS += --no-builtin-rules

DEBUG_PRESET := debug
HOST_PRESET := host
ASAN_PRESET := asan
FUZZ_PRESET := fuzz
RELEASE_BUILD_PRESETS := \
	linux-gnu-release \
	linux-musl-release \
	aarch64-linux-gnu-release \
	aarch64-linux-musl-release \
	armhf-linux-gnu-release \
	armhf-linux-musl-release
CROSS_RELEASE_PRESETS := \
	aarch64-linux-gnu-release \
	aarch64-linux-musl-release \
	armhf-linux-gnu-release \
	armhf-linux-musl-release
LUA ?= lua
GENERATED_FIXTURE_DIR := $(CURDIR)/build/generated/fixtures
COMPOSE := $(shell if command -v nerdctl >/dev/null 2>&1; then printf '%s' 'nerdctl compose'; elif command -v docker >/dev/null 2>&1; then printf '%s' 'docker compose'; fi)
RELEASE_VERSION := $(shell ./scripts/release_version.sh)
DIST_DIR := $(CURDIR)/dist
RELEASE_SOURCE_TARBALL := $(DIST_DIR)/lonejson-$(RELEASE_VERSION).tar.gz
RELEASE_HEADER_GZ := $(DIST_DIR)/lonejson-$(RELEASE_VERSION).h.gz
RELEASE_ROCKSPEC := $(DIST_DIR)/lonejson-$(RELEASE_VERSION)-1.rockspec
RELEASE_PACK_DIR := $(DIST_DIR)/.pack
RELEASE_PACK_STAGE_DIR := $(RELEASE_PACK_DIR)/src
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
FUZZ_JSON_VALUE_MAX_LEN ?= 524288
FUZZ_VALUE_VISITOR_MAX_LEN ?= 524288
FUZZ_LARGE_SEEDS := \
	fuzz/corpus/mapped/person_large_payload.json \
	fuzz/corpus/json_value/large_selector_payload.json \
	fuzz/corpus/value_visitor/large_unicode_payload.json
FUZZ_GENERATED_DIR := fuzz/generated
FUZZ_VALIDATE_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/validate
FUZZ_MAPPED_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/mapped
FUZZ_JSON_VALUE_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/json_value
FUZZ_VALUE_VISITOR_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/value_visitor
FUZZ_VALIDATE_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/validate
FUZZ_MAPPED_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/mapped
FUZZ_JSON_VALUE_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/json_value
FUZZ_VALUE_VISITOR_GENERATED_DIR := $(FUZZ_GENERATED_DIR)/value_visitor
LUA_ROCK_TREE := build/luarocks
LUA_ROCKSPEC := $(LUA_ROCK_TREE)/lonejson-$(RELEASE_VERSION)-1.rockspec
LUA_ROCK_STAMP := $(LUA_ROCK_TREE)/.installed.stamp
LUA_ROCK_BUILD_LOCK := $(LUA_ROCK_TREE)/.build.lock
LUA_ROCK_EXTRA_CFLAGS ?= -O3 -DNDEBUG -fno-semantic-interposition
LUA_ROCK_BUILD_BYPRODUCTS := \
	$(CURDIR)/lonejson \
	$(CURDIR)/src/lua/lonejson_lua.o \
	$(CURDIR)/src/lonejson.o
LUA_ROCK_SOURCES := \
	lonejson.rockspec.in \
	scripts/build_lua_rock.sh \
	scripts/render_release_rockspec.sh \
	scripts/release_version.sh \
	include/lonejson.h \
	src/lonejson.c \
	src/lonejson_impl.h \
	$(sort $(wildcard src/impl/*.h)) \
	src/lua/lonejson_lua.c \
	lua/lonejson/init.lua

.PHONY: \
	help \
	build \
	build-host \
	build-release \
	release-lua-artifacts \
	release-source-artifact \
	release-source-smoke \
	release \
	lua-rock \
	lua-test \
	lua-bench \
	lua-bench-freeze-baseline \
	lua-bench-compare \
	lua-bench-gate \
	bench \
	bench-freeze-baseline \
	bench-compare \
	bench-gate \
	test \
	test-host \
	test-host-curl \
	test-cross \
	test-all \
	test-all-bindings \
	asan \
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
		'make release                Run the Linux release matrix and package generation.' \
		'make release-source-smoke   Unpack the source release tarball into a temp tree and run make release there.' \
		'make lua-rock               Generate a local rockspec in build/luarocks and install the Lua module there.' \
		'make lua-test               Build the Lua module and run the Lua integration test.' \
		'make lua-bench              Run the standalone Lua benchmark harness, compare it, and enforce the Lua benchmark gate.' \
		'make lua-bench-freeze-baseline Freeze the last Lua benchmark history entry as the Lua baseline.' \
		'make lua-bench-compare      Run a fresh Lua benchmark, then compare against the committed Lua baseline.' \
		'make lua-bench-gate         Run a fresh Lua benchmark, then enforce the Lua benchmark gate against the committed Lua baseline.' \
		'make bench                  Build and run the host benchmark against the vendored JSON corpus, then compare/gate against the working baseline file if present.' \
		'make bench-freeze-baseline  Freeze the last history entry as the current benchmark baseline.' \
		'make bench-compare          Run a fresh C benchmark, then compare against the committed C baseline.' \
		'make bench-gate             Run a fresh C benchmark, then enforce the C benchmark gate against the committed C baseline.' \
		'make test                   Build and run the debug test preset.' \
		'make test-host              Build and run the host-native test preset.' \
		'make test-host-curl         Build and run the host-native curl-enabled test preset.' \
		'make test-cross             Configure, build, and run all cross release test presets serially.' \
		'make test-all               Run debug, host, host-curl, cross, and ASan C-centric test suites serially.' \
		'make test-all-bindings      Run test-all plus the optional Lua binding suite.' \
		'make asan                   Build and run the ASan/UBSan preset.' \
		'make fuzz                   Build all libFuzzer targets and run a seeded 30s pass for each with explicit large-input caps; missing large synthetic seeds are regenerated automatically.' \
		'make fuzz-long              Run the same fuzz targets with a several-minute soak per target.' \
		'make stack-usage            Build with compiler stack-usage reporting and print the report.' \
		'make format                 Run clang-format over the C sources.' \
		'make deps-host              Download and extract the host-native liblockdc dev bundle.' \
		'make deps-x86_64-linux-gnu  Download and extract the x86_64 glibc liblockdc dev bundle.' \
		'make deps-x86_64-linux-musl Download and extract the x86_64 musl liblockdc dev bundle.' \
		'make deps-aarch64-linux-gnu Download and extract the aarch64 glibc liblockdc dev bundle.' \
		'make deps-aarch64-linux-musl Download and extract the aarch64 musl liblockdc dev bundle.' \
		'make deps-armhf-linux-gnu   Download and extract the armhf glibc liblockdc dev bundle.' \
		'make deps-armhf-linux-musl  Download and extract the armhf musl liblockdc dev bundle.' \
		'make deps-all               Download and extract every supported liblockdc dev bundle.' \
		'make certs                  Generate the local self-signed localhost TLS cert for nginx.' \
		'make compose-up             Start the local nginx + sink HTTP/HTTPS test rig.' \
		'make compose-down           Stop and remove the local compose stack.' \
		'make compose-logs           Tail logs from the local compose stack.' \
		'make curl-examples          Build the curl examples against the host liblockdc dev bundle.' \
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

build-release:
	@set -e; for preset in $(RELEASE_BUILD_PRESETS); do \
		cmake --preset "$$preset"; \
		cmake --build --preset "$$preset"; \
	done

$(DIST_DIR):
	mkdir -p "$(DIST_DIR)"

$(RELEASE_PACK_DIR):
	mkdir -p "$(RELEASE_PACK_DIR)"

$(RELEASE_PACK_STAGE_DIR): Makefile | $(RELEASE_PACK_DIR)
	./scripts/stage_release_sources.sh "$(CURDIR)" "$(RELEASE_PACK_STAGE_DIR)" "$(RELEASE_VERSION)"

$(RELEASE_ROCKSPEC): lonejson.rockspec.in scripts/render_release_rockspec.sh | $(DIST_DIR)
	lib_ext="$$(luarocks config variables.LIB_EXTENSION)"; ./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "$(RELEASE_ROCKSPEC)" "" "" "$$lib_ext"

$(RELEASE_PACK_ROCKSPEC): Makefile $(RELEASE_PACK_STAGE_DIR) $(RELEASE_SOURCE_TARBALL)
	cd "$(RELEASE_PACK_STAGE_DIR)" && lib_ext="$$(luarocks config variables.LIB_EXTENSION)" && ./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "../$(notdir $(RELEASE_PACK_ROCKSPEC))" "file://$(RELEASE_SOURCE_TARBALL)" "" "$$lib_ext"

$(RELEASE_ROCK): $(RELEASE_PACK_ROCKSPEC) $(RELEASE_ROCKSPEC)
	rm -f "$(RELEASE_ROCK)"
	cd "$(RELEASE_PACK_STAGE_DIR)" && luarocks pack "../$(notdir $(RELEASE_PACK_ROCKSPEC))"
	mv "$(RELEASE_PACK_STAGE_DIR)/$(notdir $(RELEASE_ROCK))" "$(RELEASE_ROCK)"
	rm -rf "$(RELEASE_PACK_DIR)"

release-lua-artifacts: $(RELEASE_ROCKSPEC) $(RELEASE_ROCK)

release-source-artifact:
	cmake --preset $(HOST_PRESET)
	cmake --build --preset package-source

$(RELEASE_SOURCE_TARBALL):
	$(MAKE) release-source-artifact

release-source-smoke: release-source-artifact
	./scripts/test_release_from_source.sh "$(CURDIR)" "$(DIST_DIR)/lonejson-$(RELEASE_VERSION).tar.gz"

release:
	./scripts/run_linux_release_matrix.sh

bench:
	@cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	if [ -f "$(PERF_BASELINE)" ]; then \
		./build/$(HOST_PRESET)/lonejson_bench compare "$(PERF_BASELINE)" "$(PERF_LATEST)" && \
		./build/$(HOST_PRESET)/lonejson_bench gate "$(PERF_BASELINE)" "$(PERF_LATEST)"; \
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

test-host-curl:
	cmake --preset host-curl
	cmake --build --preset host-curl
	ctest --preset host-curl

test-cross:
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

test-all-bindings:
	$(MAKE) test-all
	$(MAKE) lua-test

lua-rock: $(LUA_ROCK_STAMP)

$(LUA_ROCKSPEC): $(LUA_ROCK_SOURCES)
	mkdir -p "$(LUA_ROCK_TREE)"
	lib_ext="$$(luarocks config variables.LIB_EXTENSION)"; ./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "$(LUA_ROCKSPEC)" "git+file://$(CURDIR)" "" "$$lib_ext"

$(LUA_ROCK_STAMP): $(LUA_ROCKSPEC) $(LUA_ROCK_SOURCES)
	flock "$(LUA_ROCK_BUILD_LOCK)" bash -lc 'set -e; CFLAGS="$${CFLAGS:+$$CFLAGS }$(LUA_ROCK_EXTRA_CFLAGS)" luarocks make --tree "$(LUA_ROCK_TREE)" "$(LUA_ROCKSPEC)"; rm -rf $(LUA_ROCK_BUILD_BYPRODUCTS); touch "$(LUA_ROCK_STAMP)"'

lua-test: lua-rock
	eval "$$(luarocks path --tree $(LUA_ROCK_TREE))" && lua tests/test_lua.lua

lua-bench:
	@$(MAKE) lua-rock
	eval "$$(luarocks path --tree $(LUA_ROCK_TREE))" && lua bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	if [ -f "$(LUA_PERF_BASELINE)" ]; then \
		lua bench/lonejson_lua_bench.lua compare "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)" && \
		lua bench/lonejson_lua_bench.lua gate "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"; \
	fi

lua-bench-freeze-baseline: lua-rock
	eval "$$(luarocks path --tree $(LUA_ROCK_TREE))" && lua bench/lonejson_lua_bench.lua freeze-baseline "$(LUA_PERF_HISTORY)" "$(LUA_PERF_BASELINE)"

lua-bench-compare:
	@$(MAKE) lua-rock
	eval "$$(luarocks path --tree $(LUA_ROCK_TREE))" && lua bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	lua bench/lonejson_lua_bench.lua compare "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"

lua-bench-gate:
	@$(MAKE) lua-rock
	eval "$$(luarocks path --tree $(LUA_ROCK_TREE))" && lua bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(LUA_PERF_ITERATIONS)" && \
	lua bench/lonejson_lua_bench.lua gate "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"

asan:
	cmake --preset $(ASAN_PRESET)
	cmake --build --preset $(ASAN_PRESET)
	ctest --preset $(ASAN_PRESET)

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
	cmake --build --preset $(FUZZ_PRESET) --target lonejson_fuzz_validate lonejson_fuzz_mapped_parse lonejson_fuzz_json_value lonejson_fuzz_value_visitor
	cmake -E rm -rf "$(FUZZ_VALIDATE_CORPUS_DIR)" "$(FUZZ_MAPPED_CORPUS_DIR)" "$(FUZZ_JSON_VALUE_CORPUS_DIR)" "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)"
	cmake -E make_directory "$(FUZZ_VALIDATE_GENERATED_DIR)" "$(FUZZ_MAPPED_GENERATED_DIR)" "$(FUZZ_JSON_VALUE_GENERATED_DIR)" "$(FUZZ_VALUE_VISITOR_GENERATED_DIR)"
	cmake -E make_directory "$(FUZZ_VALIDATE_CORPUS_DIR)/vendor" "$(FUZZ_VALIDATE_CORPUS_DIR)/spec" "$(FUZZ_VALIDATE_CORPUS_DIR)/languages"
	cmake -E make_directory "$(FUZZ_MAPPED_CORPUS_DIR)/mapped" "$(FUZZ_MAPPED_CORPUS_DIR)/spec" "$(FUZZ_MAPPED_CORPUS_DIR)/languages"
	cmake -E make_directory "$(FUZZ_JSON_VALUE_CORPUS_DIR)/json_value" "$(FUZZ_JSON_VALUE_CORPUS_DIR)/mapped" "$(FUZZ_JSON_VALUE_CORPUS_DIR)/value_visitor"
	cmake -E make_directory "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/value_visitor" "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/json_value" "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/languages"
	cp -R tests/fixtures/vendor/json_test_suite/test_parsing/. "$(FUZZ_VALIDATE_CORPUS_DIR)/vendor/"
	cp -R tests/fixtures/spec/. "$(FUZZ_VALIDATE_CORPUS_DIR)/spec/"
	cp -R tests/fixtures/languages/. "$(FUZZ_VALIDATE_CORPUS_DIR)/languages/"
	cp -R fuzz/corpus/mapped/. "$(FUZZ_MAPPED_CORPUS_DIR)/mapped/"
	cp -R tests/fixtures/spec/. "$(FUZZ_MAPPED_CORPUS_DIR)/spec/"
	cp -R tests/fixtures/languages/. "$(FUZZ_MAPPED_CORPUS_DIR)/languages/"
	cp -R fuzz/corpus/json_value/. "$(FUZZ_JSON_VALUE_CORPUS_DIR)/json_value/"
	cp -R fuzz/corpus/mapped/. "$(FUZZ_JSON_VALUE_CORPUS_DIR)/mapped/"
	cp -R fuzz/corpus/value_visitor/. "$(FUZZ_JSON_VALUE_CORPUS_DIR)/value_visitor/"
	cp -R fuzz/corpus/value_visitor/. "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/value_visitor/"
	cp -R fuzz/corpus/json_value/. "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/json_value/"
	cp -R tests/fixtures/languages/. "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/languages/"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/validate"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/mapped"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/json_value"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/value_visitor"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_validate -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_VALIDATE_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/validate/ "$(FUZZ_VALIDATE_GENERATED_DIR)" "$(FUZZ_VALIDATE_CORPUS_DIR)/vendor" "$(FUZZ_VALIDATE_CORPUS_DIR)/spec" "$(FUZZ_VALIDATE_CORPUS_DIR)/languages"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_mapped_parse -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_MAPPED_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/mapped/ "$(FUZZ_MAPPED_GENERATED_DIR)" "$(FUZZ_MAPPED_CORPUS_DIR)/mapped" "$(FUZZ_MAPPED_CORPUS_DIR)/spec" "$(FUZZ_MAPPED_CORPUS_DIR)/languages"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_json_value -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_JSON_VALUE_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/json_value/ "$(FUZZ_JSON_VALUE_GENERATED_DIR)" "$(FUZZ_JSON_VALUE_CORPUS_DIR)/json_value" "$(FUZZ_JSON_VALUE_CORPUS_DIR)/mapped" "$(FUZZ_JSON_VALUE_CORPUS_DIR)/value_visitor"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_value_visitor -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_VALUE_VISITOR_MAX_LEN) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/value_visitor/ "$(FUZZ_VALUE_VISITOR_GENERATED_DIR)" "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/value_visitor" "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/json_value" "$(FUZZ_VALUE_VISITOR_CORPUS_DIR)/languages"

fuzz-long:
	$(MAKE) fuzz FUZZ_TIME=$(FUZZ_LONG_TIME)

stack-usage:
	cmake --preset stack-usage
	cmake --build --preset stack-usage-report

format:
	cmake --preset $(DEBUG_PRESET)
	cmake --build --preset format

deps-host:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -P cmake/fetch_liblockdc.cmake

deps-x86_64-linux-gnu:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_LIBLOCKDC_ARCH=x86_64 -D LONEJSON_LIBLOCKDC_LIBC=gnu -P cmake/fetch_liblockdc.cmake

deps-x86_64-linux-musl:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_LIBLOCKDC_ARCH=x86_64 -D LONEJSON_LIBLOCKDC_LIBC=musl -P cmake/fetch_liblockdc.cmake

deps-aarch64-linux-gnu:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_LIBLOCKDC_ARCH=aarch64 -D LONEJSON_LIBLOCKDC_LIBC=gnu -P cmake/fetch_liblockdc.cmake

deps-aarch64-linux-musl:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_LIBLOCKDC_ARCH=aarch64 -D LONEJSON_LIBLOCKDC_LIBC=musl -P cmake/fetch_liblockdc.cmake

deps-armhf-linux-gnu:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_LIBLOCKDC_ARCH=armhf -D LONEJSON_LIBLOCKDC_LIBC=gnu -P cmake/fetch_liblockdc.cmake

deps-armhf-linux-musl:
	cmake -D LONEJSON_SOURCE_DIR=$(CURDIR) -D LONEJSON_LIBLOCKDC_ARCH=armhf -D LONEJSON_LIBLOCKDC_LIBC=musl -P cmake/fetch_liblockdc.cmake

deps-all: \
	deps-x86_64-linux-gnu \
	deps-x86_64-linux-musl \
	deps-aarch64-linux-gnu \
	deps-aarch64-linux-musl \
	deps-armhf-linux-gnu \
	deps-armhf-linux-musl

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

curl-examples:
	./scripts/build_curl_examples.sh

test-curl-e2e: curl-examples
	./scripts/test_curl_e2e.sh

clean:
	./scripts/clean.sh

clean-dist:
	./scripts/clean.sh --dist-only
