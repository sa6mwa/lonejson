#!/usr/bin/env bash
set -euo pipefail

# Prerelease proves the ordinary and binary graph. Only final release adds the
# focused version fixture, clean, and all-component source reconstruction.

repo_root=$1
makefile="$repo_root/Makefile"
cmakelists="$repo_root/CMakeLists.txt"

require_text() {
  local text=$1
  if ! grep -F -- "$text" "$makefile" >/dev/null; then
    printf 'missing required Makefile text: %s\n' "$text" >&2
    exit 1
  fi
}

reject_text() {
  local text=$1
  if grep -F -- "$text" "$makefile" >/dev/null; then
    printf 'forbidden Makefile text: %s\n' "$text" >&2
    exit 1
  fi
}

require_text 'release-pipeline'
require_text 'prerelease: release-pipeline'
require_text 'prerelease-hardening: prerelease'
reject_text '+$(TIME_STEP) hardening/bench-check $(MAKE) bench-check'
require_text 'release-pipeline:'
require_text '+$(TIME_STEP) prerelease/format $(MAKE) format'
require_text '+$(TIME_STEP) prerelease/test $(MAKE) test'
reject_text 'LONEJSON_TEST_ALL_HOST_CURL'
require_text '+$(TIME_STEP) prerelease/release-matrix $(MAKE) release-matrix'
require_text '+$(TIME_STEP) release/source-smoke $(MAKE) package-source-smoke'
require_text '+$(TIME_STEP) release/package-verify $(MAKE) package-verify'
require_text 'verify-release-privacy: package-verify'
require_text 'lifecycle-version-contract:'
require_text 'bash ./tests/test_release_version_override.sh "$(CURDIR)"'
require_text 'release:'
require_text '+$(TIME_STEP) release/version-contract $(MAKE) lifecycle-version-contract'
require_text '+$(TIME_STEP) release/clean $(MAKE) clean'
require_text '+$(TIME_STEP) release/pipeline $(MAKE) release-pipeline'

reject_text 'prerelease: test-all'
reject_text '+$(TIME_STEP) release/prerelease'
reject_text '+$(TIME_STEP) release/release-matrix'
require_text '+$(TIME_STEP) bench-check $(MAKE) bench-check'
reject_text './scripts/verify_release_privacy.sh "$(RELEASE_CHECKSUMS)"'

if grep -F 'NAME lonejson_release_version_override_tests' "$cmakelists" >/dev/null; then
  printf 'lifecycle version contract must not be registered as a CTest test\n' >&2
  exit 1
fi

line_number() {
  local text=$1
  grep -nF -- "$text" "$makefile" | head -n 1 | cut -d: -f1
}

format_line=$(line_number '+$(TIME_STEP) prerelease/format $(MAKE) format')
test_line=$(line_number '+$(TIME_STEP) prerelease/test $(MAKE) test')
matrix_line=$(line_number '+$(TIME_STEP) prerelease/release-matrix $(MAKE) release-matrix')
source_smoke_line=$(line_number '+$(TIME_STEP) release/source-smoke $(MAKE) package-source-smoke')
package_verify_line=$(line_number '+$(TIME_STEP) release/package-verify $(MAKE) package-verify')
clean_line=$(line_number '+$(TIME_STEP) release/clean $(MAKE) clean')
version_contract_line=$(line_number '+$(TIME_STEP) release/version-contract $(MAKE) lifecycle-version-contract')
pipeline_line=$(line_number '+$(TIME_STEP) release/pipeline $(MAKE) release-pipeline')

if (( format_line >= test_line || test_line >= matrix_line )); then
  printf 'release-pipeline must run format, native proof, then the binary matrix\n' >&2
  exit 1
fi

if (( version_contract_line >= clean_line || clean_line >= pipeline_line || pipeline_line >= source_smoke_line || source_smoke_line >= package_verify_line )); then
  printf 'release must run lifecycle-version-contract, then clean, pipeline, source-smoke and package-verify\n' >&2
  exit 1
fi

# The incremental binary rehearsal must not reconstruct the all-component source.
if grep -F 'cmake --build --preset package-source' "$repo_root/scripts/run_release_matrix.sh" >/dev/null; then
  printf 'binary matrix must not reconstruct the source archive\n' >&2; exit 1
fi

# Execute the real public targets with recording sub-makes. A failed version
# contract must prevent clean and every later release command.
mkdir -p "$repo_root/build"
workspace=$(mktemp -d "$repo_root/build/test-release-graph.XXXXXX")
trap 'rm -rf "$workspace"' EXIT
cat >"$workspace/make" <<'MOCK_MAKE'
#!/usr/bin/env bash
printf '%s\n' "$*" >>"$TEST_RELEASE_GRAPH_LOG"
if [[ "$1" == "${TEST_RELEASE_GRAPH_FAIL:-}" ]]; then exit 19; fi
if [[ "$1" == release-pipeline ]]; then
  exec "$TEST_REAL_MAKE" -s -C "$TEST_REPO_ROOT" MAKE="$0" TIME_STEP="$TEST_TIME_STEP" release-pipeline
fi
MOCK_MAKE
cat >"$workspace/time" <<'MOCK_TIME'
#!/usr/bin/env bash
shift
exec "$@"
MOCK_TIME
chmod +x "$workspace/make" "$workspace/time"
export TEST_RELEASE_GRAPH_LOG="$workspace/graph"
export TEST_REAL_MAKE="$(command -v make)" TEST_REPO_ROOT="$repo_root" TEST_TIME_STEP="$workspace/time"
proof=(format test asan tsan valgrind test-e2e fuzz-smoke bench-check release-matrix)
make -s -C "$repo_root" MAKE="$workspace/make" TIME_STEP="$workspace/time" prerelease
printf '%s\n' "${proof[@]}" >"$workspace/expected"
diff -u "$workspace/expected" "$workspace/graph"
: >"$workspace/graph"
make -s -C "$repo_root" MAKE="$workspace/make" TIME_STEP="$workspace/time" release
printf '%s\n' lifecycle-version-contract clean release-pipeline "${proof[@]}" package-source-smoke package-verify >"$workspace/expected"
diff -u "$workspace/expected" "$workspace/graph"
: >"$workspace/graph"
if TEST_RELEASE_GRAPH_FAIL=lifecycle-version-contract make -s -C "$repo_root" MAKE="$workspace/make" TIME_STEP="$workspace/time" release >"$workspace/failure.log" 2>&1; then
  printf 'release continued after failed version contract\n' >&2; exit 1
fi
printf '%s\n' lifecycle-version-contract >"$workspace/expected"
diff -u "$workspace/expected" "$workspace/graph"

# Every proof stage is executed once, and every failure prevents later stages.
for target in prerelease prerelease-hardening; do
  : >"$workspace/graph"
  make -s -C "$repo_root" MAKE="$workspace/make" TIME_STEP="$workspace/time" "$target"
  printf '%s\n' "${proof[@]}" >"$workspace/expected"
  diff -u "$workspace/expected" "$workspace/graph"
done
for index in "${!proof[@]}"; do
  : >"$workspace/graph"
  if TEST_RELEASE_GRAPH_FAIL="${proof[index]}" make -s -C "$repo_root" \
      MAKE="$workspace/make" TIME_STEP="$workspace/time" prerelease >"$workspace/failure.log" 2>&1; then
    printf 'prerelease continued after failed %s\n' "${proof[index]}" >&2; exit 1
  fi
  printf '%s\n' "${proof[@]:0:index+1}" >"$workspace/expected"
  diff -u "$workspace/expected" "$workspace/graph"
done

release_steps=(lifecycle-version-contract clean release-pipeline "${proof[@]}" package-source-smoke package-verify)
for index in "${!release_steps[@]}"; do
  : >"$workspace/graph"
  if TEST_RELEASE_GRAPH_FAIL="${release_steps[index]}" make -s -C "$repo_root" \
      MAKE="$workspace/make" TIME_STEP="$workspace/time" release >"$workspace/failure.log" 2>&1; then
    printf 'release continued after failed %s\n' "${release_steps[index]}" >&2; exit 1
  fi
  printf '%s\n' "${release_steps[@]:0:index+1}" >"$workspace/expected"
  diff -u "$workspace/expected" "$workspace/graph"
done

# Slice verification selects core/header/ABI checks without full builds or
# world gates. Recording tools exercise the actual public recipe.
mkdir -p "$workspace/bin"
for tool in cmake ctest; do
  cat >"$workspace/bin/$tool" <<'TOOL'
#!/usr/bin/env bash
name=${0##*/}
printf '%s %s\n' "$name" "$*" >>"$TEST_RELEASE_GRAPH_LOG"
[[ "$name" != "${TEST_SLICE_FAIL:-}" ]]
TOOL
  chmod +x "$workspace/bin/$tool"
done
: >"$workspace/graph"
PATH="$workspace/bin:$PATH" make -s -C "$repo_root" MAKE="$workspace/make" finalize-slice
cat >"$workspace/expected" <<'EXPECTED'
format
cmake --build --preset debug --target lonejson_tests lonejson_map_cache_no_tls_tests lonejson_short_names_tests lonejson_short_names_disabled_tests lonejson_static_link_tests lonejson_shared_link_tests
ctest --preset debug --output-on-failure -R ^lonejson_(tests|map_cache_no_tls_tests|short_names_tests|short_names_disabled_tests|static_link_tests|shared_link_tests|shared_soversion_tests|single_header_version_tests|header_abi_version_tests|single_header_release_version_tests|library_exports_tests)$
lua-test
format-check
EXPECTED
diff -u "$workspace/expected" "$workspace/graph"
cp "$workspace/expected" "$workspace/slice-expected"
for tool in cmake ctest; do
  : >"$workspace/graph"
  if PATH="$workspace/bin:$PATH" TEST_SLICE_FAIL="$tool" make -s -C "$repo_root" \
      MAKE="$workspace/make" finalize-slice >"$workspace/failure.log" 2>&1; then
    printf 'finalize-slice ignored failed %s\n' "$tool" >&2; exit 1
  fi
  count=2; [[ "$tool" != ctest ]] || count=3
  head -n "$count" "$workspace/slice-expected" >"$workspace/expected"
  diff -u "$workspace/expected" "$workspace/graph"
done
