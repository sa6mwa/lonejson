SHELL := bash
.DEFAULT_GOAL := help
MAKEFLAGS += --no-builtin-rules

DEBUG_PRESET := debug
HOST_PRESET := host
ASAN_PRESET := asan
FUZZ_PRESET := fuzz
LUA ?= lua
COMPOSE := $(shell if command -v nerdctl >/dev/null 2>&1; then printf '%s' 'nerdctl compose'; elif command -v docker >/dev/null 2>&1; then printf '%s' 'docker compose'; fi)
RELEASE_VERSION := $(shell ./scripts/release_version.sh)
DIST_DIR := $(CURDIR)/dist
RELEASE_HEADER := $(DIST_DIR)/lonejson-$(RELEASE_VERSION).h
RELEASE_HEADER_GZ := $(RELEASE_HEADER).gz
RELEASE_ROCKSPEC := $(DIST_DIR)/lonejson-$(RELEASE_VERSION)-1.rockspec
RELEASE_PACK_DIR := $(DIST_DIR)/.pack
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
PERF_ITERATIONS ?= 100
FUZZ_RUNS ?= 1000
FUZZ_VALIDATE_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/validate
FUZZ_MAPPED_CORPUS_DIR := build/$(FUZZ_PRESET)/corpus/mapped
LUA_ROCK_TREE := build/luarocks
LUA_ROCKSPEC := $(LUA_ROCK_TREE)/lonejson-$(RELEASE_VERSION)-1.rockspec
LUA_ROCK_STAMP := $(LUA_ROCK_TREE)/.installed.stamp
LUA_ROCK_BUILD_LOCK := $(LUA_ROCK_TREE)/.build.lock
LUA_ROCK_SOURCES := \
	lonejson.rockspec.in \
	scripts/render_release_rockspec.sh \
	scripts/release_version.sh \
	include/lonejson.h \
	src/lua/lonejson_lua.c \
	lua/lonejson/init.lua

.PHONY: \
	help \
	build \
	build-host \
	release \
lua-rock \
	lua-test \
	lua-bench \
	lua-bench-freeze-baseline \
	lua-bench-compare \
	bench \
	bench-freeze-baseline \
	bench-compare \
	test \
	test-host \
	asan \
	fuzz \
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
	generate-fixtures \
	compose-up \
	compose-down \
	compose-logs \
	curl-examples \
	test-curl-e2e \
	clean

help:
	@printf '%s\n' \
		'make build                  Configure and build the full debug tree (tests and standalone examples, excluding lua-* and curl-examples).' \
		'make build-host             Configure and build the host-native release preset.' \
		'make release                Package dist/lonejson-<version>.h.gz together with the Lua rockspec and source rock artifacts.' \
		'make lua-rock               Generate a local rockspec in build/luarocks and install the Lua module there.' \
		'make lua-test               Build the Lua module and run the Lua integration test.' \
		'make lua-bench              Run the standalone Lua benchmark harness and compare it to the C latest run.' \
		'make lua-bench-freeze-baseline Freeze the last Lua benchmark history entry as the Lua baseline.' \
		'make lua-bench-compare      Compare perflogs/lua/latest.json against perflogs/lua/baseline.json.' \
		'make bench                  Build and run the host benchmark against the vendored JSON corpus.' \
		'make bench-freeze-baseline  Freeze the last history entry as the current benchmark baseline.' \
		'make bench-compare          Compare perflogs/latest.json against perflogs/baseline.json.' \
		'make test                   Build and run the debug test preset.' \
		'make test-host              Build and run the host-native test preset.' \
		'make asan                   Build and run the ASan/UBSan preset.' \
		'make fuzz                   Build the libFuzzer targets and run a seeded smoke pass.' \
		'make stack-usage            Build with compiler stack-usage reporting and print the report.' \
		'make format                 Run clang-format over the C header/bench/tests/examples.' \
		'make deps-host              Download and extract the host-native liblockdc dev bundle.' \
		'make deps-x86_64-linux-gnu  Download and extract the x86_64 glibc liblockdc dev bundle.' \
		'make deps-x86_64-linux-musl Download and extract the x86_64 musl liblockdc dev bundle.' \
		'make deps-aarch64-linux-gnu Download and extract the aarch64 glibc liblockdc dev bundle.' \
		'make deps-aarch64-linux-musl Download and extract the aarch64 musl liblockdc dev bundle.' \
		'make deps-armhf-linux-gnu   Download and extract the armhf glibc liblockdc dev bundle.' \
		'make deps-armhf-linux-musl  Download and extract the armhf musl liblockdc dev bundle.' \
		'make deps-all               Download and extract every supported liblockdc dev bundle.' \
		'make certs                  Generate the local self-signed localhost TLS cert for nginx.' \
		'make generate-fixtures      Generate large deterministic JSON fixtures for tests and local HTTP.' \
		'make compose-up             Start the local nginx + sink HTTP/HTTPS test rig.' \
		'make compose-down           Stop and remove the local compose stack.' \
		'make compose-logs           Tail logs from the local compose stack.' \
		'make curl-examples          Build the curl examples against the host liblockdc dev bundle.' \
		'make test-curl-e2e          Build and run the curl examples against the local HTTPS rig.' \
		'make clean                  Remove build/, .deps/, dist/, examples/bin/, and generated Lua module artifacts.'

build:
	cmake --preset $(DEBUG_PRESET)
	cmake --build --preset $(DEBUG_PRESET)
	./scripts/stage_standalone_examples.sh

build-host:
	cmake --preset $(HOST_PRESET)
	cmake --build --preset $(HOST_PRESET)

release: $(RELEASE_HEADER_GZ) $(RELEASE_ROCKSPEC) $(RELEASE_ROCK) $(RELEASE_CHECKSUMS)
	@printf '%s\n' \
		"release version: $(RELEASE_VERSION)" \
		"header: $(RELEASE_HEADER_GZ)" \
		"rock:   $(RELEASE_ROCKSPEC)" \
		"packed: $(RELEASE_ROCK)" \
		"sha256: $(RELEASE_CHECKSUMS)"

$(DIST_DIR):
	mkdir -p "$(DIST_DIR)"

$(RELEASE_PACK_DIR):
	mkdir -p "$(RELEASE_PACK_DIR)"

$(RELEASE_HEADER_GZ): include/lonejson.h | $(DIST_DIR)
	rm -f "$(RELEASE_HEADER)"
	gzip -n -c include/lonejson.h >"$(RELEASE_HEADER_GZ)"

$(RELEASE_ROCKSPEC): lonejson.rockspec.in scripts/render_release_rockspec.sh | $(DIST_DIR)
	./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "$(RELEASE_ROCKSPEC)"

$(RELEASE_PACK_ROCKSPEC): lonejson.rockspec.in scripts/render_release_rockspec.sh | $(RELEASE_PACK_DIR)
	./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "$(RELEASE_PACK_ROCKSPEC)" "git+file://$(CURDIR)"

$(RELEASE_ROCK): $(RELEASE_PACK_ROCKSPEC) $(RELEASE_ROCKSPEC)
	rm -f "$(RELEASE_ROCK)"
	cd "$(RELEASE_PACK_DIR)" && luarocks pack "$(notdir $(RELEASE_PACK_ROCKSPEC))"
	mv "$(RELEASE_PACK_DIR)/$(notdir $(RELEASE_ROCK))" "$(RELEASE_ROCK)"
	rm -rf "$(RELEASE_PACK_DIR)"

$(RELEASE_CHECKSUMS): $(RELEASE_HEADER_GZ) $(RELEASE_ROCKSPEC) $(RELEASE_ROCK)
	cd "$(DIST_DIR)" && \
	sha256sum \
		"$(notdir $(RELEASE_HEADER_GZ))" \
		"$(notdir $(RELEASE_ROCKSPEC))" \
		"$(notdir $(RELEASE_ROCK))" \
	> "$(notdir $(RELEASE_CHECKSUMS))"

bench:
	@bundle_root="$$(./scripts/detect_liblockdc_bundle.sh)" && \
	cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON -D LONEJSON_BENCH_BUNDLE_ROOT="$$bundle_root" && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	./build/$(HOST_PRESET)/lonejson_bench run "$(PERF_CORPUS)" "$(PERF_LATEST)" "$(PERF_HISTORY)" "$(PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	if [ -f "$(PERF_BASELINE)" ]; then ./build/$(HOST_PRESET)/lonejson_bench compare "$(PERF_BASELINE)" "$(PERF_LATEST)"; fi

bench-freeze-baseline:
	@bundle_root="$$(./scripts/detect_liblockdc_bundle.sh)" && \
	cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON -D LONEJSON_BENCH_BUNDLE_ROOT="$$bundle_root" && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	./build/$(HOST_PRESET)/lonejson_bench freeze-baseline "$(PERF_HISTORY)" "$(PERF_BASELINE)"

bench-compare:
	@bundle_root="$$(./scripts/detect_liblockdc_bundle.sh)" && \
	cmake --preset $(HOST_PRESET) -D LONEJSON_BUILD_BENCHMARKS=ON -D LONEJSON_BENCH_BUNDLE_ROOT="$$bundle_root" && \
	cmake --build --preset $(HOST_PRESET) --target lonejson_bench && \
	./build/$(HOST_PRESET)/lonejson_bench compare "$(PERF_BASELINE)" "$(PERF_LATEST)"

test: build
	ctest --preset $(DEBUG_PRESET)
	$(MAKE) lua-test

test-host: build-host
	ctest --preset $(HOST_PRESET)
	$(MAKE) lua-test

lua-rock:
lua-rock: $(LUA_ROCK_STAMP)

$(LUA_ROCKSPEC): $(LUA_ROCK_SOURCES)
	mkdir -p "$(LUA_ROCK_TREE)"
	./scripts/render_release_rockspec.sh "$(RELEASE_VERSION)" "$(LUA_ROCKSPEC)" "git+file://$(CURDIR)"

$(LUA_ROCK_STAMP): $(LUA_ROCKSPEC) $(LUA_ROCK_SOURCES)
	flock "$(LUA_ROCK_BUILD_LOCK)" bash -lc 'set -e; luarocks make --tree "$(LUA_ROCK_TREE)" "$(LUA_ROCKSPEC)"; touch "$(LUA_ROCK_STAMP)"'

lua-test: lua-rock
	eval "$$(luarocks path --tree $(LUA_ROCK_TREE))" && lua tests/test_lua.lua

lua-bench: lua-rock
	@if [ ! -f "$(PERF_LATEST)" ]; then $(MAKE) bench PERF_ITERATIONS=$(PERF_ITERATIONS); fi
	eval "$$(luarocks path --tree $(LUA_ROCK_TREE))" && lua bench/lonejson_lua_bench.lua run "$(PERF_LATEST)" "$(LUA_PERF_LATEST)" "$(LUA_PERF_HISTORY)" "$(LUA_PERF_ARCHIVE_DIR)" "$(PERF_ITERATIONS)" && \
	if [ -f "$(LUA_PERF_BASELINE)" ]; then lua bench/lonejson_lua_bench.lua compare "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"; fi

lua-bench-freeze-baseline: lua-rock
	eval "$$(luarocks path --tree $(LUA_ROCK_TREE))" && lua bench/lonejson_lua_bench.lua freeze-baseline "$(LUA_PERF_HISTORY)" "$(LUA_PERF_BASELINE)"

lua-bench-compare: lua-rock
	eval "$$(luarocks path --tree $(LUA_ROCK_TREE))" && lua bench/lonejson_lua_bench.lua compare "$(LUA_PERF_BASELINE)" "$(LUA_PERF_LATEST)"

asan:
	cmake --preset $(ASAN_PRESET)
	cmake --build --preset $(ASAN_PRESET)
	ctest --preset $(ASAN_PRESET)

fuzz:
	cmake --preset $(FUZZ_PRESET)
	cmake --build --preset $(FUZZ_PRESET) --target lonejson_fuzz_validate lonejson_fuzz_mapped_parse
	cmake -E rm -rf "$(FUZZ_VALIDATE_CORPUS_DIR)" "$(FUZZ_MAPPED_CORPUS_DIR)"
	cmake -E make_directory "$(FUZZ_VALIDATE_CORPUS_DIR)/vendor" "$(FUZZ_VALIDATE_CORPUS_DIR)/spec" "$(FUZZ_VALIDATE_CORPUS_DIR)/languages"
	cmake -E make_directory "$(FUZZ_MAPPED_CORPUS_DIR)/mapped" "$(FUZZ_MAPPED_CORPUS_DIR)/spec" "$(FUZZ_MAPPED_CORPUS_DIR)/languages"
	cp -R tests/fixtures/vendor/json_test_suite/test_parsing/. "$(FUZZ_VALIDATE_CORPUS_DIR)/vendor/"
	cp -R tests/fixtures/spec/. "$(FUZZ_VALIDATE_CORPUS_DIR)/spec/"
	cp -R tests/fixtures/languages/. "$(FUZZ_VALIDATE_CORPUS_DIR)/languages/"
	cp -R fuzz/corpus/mapped/. "$(FUZZ_MAPPED_CORPUS_DIR)/mapped/"
	cp -R tests/fixtures/spec/. "$(FUZZ_MAPPED_CORPUS_DIR)/spec/"
	cp -R tests/fixtures/languages/. "$(FUZZ_MAPPED_CORPUS_DIR)/languages/"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/validate"
	cmake -E make_directory "build/$(FUZZ_PRESET)/artifacts/mapped"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_validate -runs=$(FUZZ_RUNS) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/validate/ "$(FUZZ_VALIDATE_CORPUS_DIR)/vendor" "$(FUZZ_VALIDATE_CORPUS_DIR)/spec" "$(FUZZ_VALIDATE_CORPUS_DIR)/languages"
	./build/$(FUZZ_PRESET)/lonejson_fuzz_mapped_parse -runs=$(FUZZ_RUNS) -artifact_prefix=build/$(FUZZ_PRESET)/artifacts/mapped/ "$(FUZZ_MAPPED_CORPUS_DIR)/mapped" "$(FUZZ_MAPPED_CORPUS_DIR)/spec" "$(FUZZ_MAPPED_CORPUS_DIR)/languages"

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

generate-fixtures:
	$(LUA) ./scripts/generate_large_fixtures.lua ./build/generated/fixtures
	$(LUA) ./scripts/generate_large_fixtures.lua ./docker/nginx/generated/variants

compose-up:
	@test -n "$(COMPOSE)" || (printf '%s\n' 'Neither nerdctl nor docker was found in PATH.' >&2; exit 1)
	$(MAKE) certs
	$(MAKE) generate-fixtures
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
	cmake -E rm -rf build .deps dist examples/bin lonejson
