#!/usr/bin/env bash
set -euo pipefail
root=$1
mkdir -p "$root/build"
workspace=$(mktemp -d "$root/build/test-workspaces.XXXXXX")
trap 'rm -rf "$workspace"' EXIT
mkdir -p "$workspace/input"
printf 'public\n' >"$workspace/input/README.md"
printf '%s\n' README.md RELEASE_MANIFEST >"$workspace/input/RELEASE_MANIFEST"
GIT_DIR="$workspace/no-git" "$root/scripts/stage_release_sources.sh" \
  "$workspace/input" "$workspace/source" 0.0.0
[[ -f "$workspace/source/README.md" ]]
ln -s "$root/include" "$workspace/source-link"
for invalid in "$root" "$root/include" "$root/build" "$root/build///" \
    "$workspace/../../include" "$workspace/source-link/stage"; do
  if bash "$root/scripts/require_build_workspace.sh" "$invalid" >"$workspace/failure.log" 2>&1; then
    printf 'unsafe workspace accepted: %s\n' "$invalid" >&2; exit 1
  fi
  grep -F 'refusing unsafe workspace' "$workspace/failure.log" >/dev/null
done
[[ -f "$root/include/lonejson.h" ]]

# CMake packaging must reject an escaped stage before destructive cleanup.
fixture="$workspace/repo"
escaped="$workspace/escaped"
mkdir -p "$fixture/scripts" "$fixture/cmake" "$fixture/build" "$escaped/source"
cp "$root/scripts/require_build_workspace.sh" "$root/scripts/stage_release_sources.sh" "$fixture/scripts/"
printf retained >"$fixture/build/sentinel"
ln -s "$fixture" "$fixture/build/root-alias"
ln -s "$fixture/build" "$fixture/build/build-alias"
for invalid in "$fixture/build/root-alias/build" "$fixture/build/build-alias"; do
  if bash "$fixture/scripts/require_build_workspace.sh" "$invalid" >"$workspace/failure.log" 2>&1; then
    printf 'workspace resolving to entire build directory accepted: %s\n' "$invalid" >&2; exit 1
  fi
  grep -F 'refusing unsafe workspace' "$workspace/failure.log" >/dev/null
  if "$fixture/scripts/stage_release_sources.sh" "$workspace/input" "$invalid" 0.0.0 \
      >"$workspace/staging-failure.log" 2>&1; then
    printf 'unsafe source staging accepted: %s\n' "$invalid" >&2; exit 1
  fi
  grep -F 'refusing unsafe workspace' "$workspace/staging-failure.log" >/dev/null
  [[ $(cat "$fixture/build/sentinel") == retained ]]
done
# Existing aliases to build descendants and not-yet-created workspaces remain valid.
mkdir -p "$fixture/build/owned"
ln -s "$fixture/build/owned" "$fixture/build/owned-alias"
for valid in "$fixture/build/owned-alias" "$fixture/build/root-alias/build/new/deep" \
    "$fixture/build/build-alias/new/deep" "$fixture/build/new/deep"; do
  "$fixture/scripts/require_build_workspace.sh" "$valid"
  "$fixture/scripts/stage_release_sources.sh" "$workspace/input" "$valid" 0.0.0
  [[ $(cat "$valid/README.md") == public ]]
  [[ $(cat "$fixture/build/sentinel") == retained ]]
done
for module in lonejson_build_workspace lonejson_dist_dir package_source package_archive package_darwin_smoke_bundle package_single_header; do
  cp "$root/cmake/$module.cmake" "$fixture/cmake/"
done
printf retained >"$escaped/source/sentinel"
ln -s "$escaped" "$fixture/build/packaging"
ln -s "$escaped" "$fixture/build/escaped"
for module in package_source package_archive package_darwin_smoke_bundle package_single_header; do
  if cmake -DLONEJSON_ROOT="$fixture" -DLONEJSON_VERSION=0.0.0 \
      -DLONEJSON_BINARY_DIR="$fixture/build/escaped" \
      -DLONEJSON_TARGET_ID=arm64-apple-darwin \
      -DLONEJSON_PUBLIC_HEADER="$root/include/lonejson.h" \
      -DLONEJSON_INTERNAL_IMPL="$root/src/lonejson_impl.h" \
      -DLONEJSON_SINGLE_HEADER_BUILD="$fixture/build/escaped/include/header.h" \
      -DLONEJSON_SINGLE_HEADER_BUILD_ALIAS="$fixture/build/escaped/alias/header.h" \
      -DLONEJSON_SINGLE_HEADER_DIST_GZ="$fixture/dist/header.h.gz" \
      -DLONEJSON_BUILD_WITH_CURL=ON -DLONEJSON_BUILD_WITH_OPENSSL=ON \
      -DLONEJSON_BUILD_WITH_JWT=ON -DLONEJSON_BUILD_WITH_OIDC=ON \
      -P "$fixture/cmake/$module.cmake" >"$workspace/$module.log" 2>&1; then
    echo "unsafe CMake packaging workspace accepted: $module" >&2; exit 1
  fi
  grep -F 'Packaging workspace rejected:' "$workspace/$module.log" >/dev/null
  [[ $(cat "$escaped/source/sentinel") == retained ]]
done
# A default artifact directory cannot silently redirect output outside its repo.
rmdir "$fixture/dist"
ln -s "$escaped" "$fixture/dist"
cat >"$workspace/dist-guard.cmake" <<EOF_CMAKE
include("$fixture/cmake/lonejson_dist_dir.cmake")
lonejson_prepare_dist_dir()
EOF_CMAKE
# Refresh the copied helper after its independent staging tests above.
cp "$root/cmake/lonejson_dist_dir.cmake" "$fixture/cmake/"
if cmake -DLONEJSON_ROOT="$fixture" -P "$workspace/dist-guard.cmake" \
    >"$workspace/dist-guard.log" 2>&1; then
  echo 'default dist symlink accepted' >&2; exit 1
fi
grep -F 'refusing default dist symlink' "$workspace/dist-guard.log" >/dev/null
[[ $(cat "$escaped/source/sentinel") == retained ]]

# Model a filesystem boundary by rejecting rename, then verify publication,
# replacement, and failures without requiring privileged temporary mounts.
cat >"$workspace/publish.cmake" <<'EOF_CMAKE'
include("${LONEJSON_ROOT}/cmake/lonejson_dist_dir.cmake")
macro(file operation)
  if("${operation}" STREQUAL "RENAME")
    message(FATAL_ERROR "Invalid cross-device link")
  endif()
  _file(${ARGV})
endmacro()
if(FORCE_RENAME)
  file(RENAME "${STAGED}" "${DESTINATION}")
endif()
lonejson_publish_artifact("${STAGED}" "${DESTINATION}")
EOF_CMAKE
mkdir -p "$workspace/publish-build" "$workspace/publish-dist"
staged="$workspace/publish-build/artifact.gz"
destination="$workspace/publish-dist/artifact.gz"
printf retained >"$staged"
if cmake -DLONEJSON_ROOT="$root" -DSTAGED="$staged" \
    -DDESTINATION="$destination" -DFORCE_RENAME=ON \
    -P "$workspace/publish.cmake" >"$workspace/rename-failure.log" 2>&1; then
  echo 'cross-filesystem rename fault was not injected' >&2; exit 1
fi
grep -F 'Invalid cross-device link' "$workspace/rename-failure.log" >/dev/null
[[ $(cat "$staged") == retained && ! -e "$destination" ]]
for content in first replacement; do
  printf '%s\n' "$content" | gzip -n >"$staged"
  cp "$staged" "$workspace/expected.gz"
  cmake -DLONEJSON_ROOT="$root" -DSTAGED="$staged" \
    -DDESTINATION="$destination" -P "$workspace/publish.cmake"
  cmp "$workspace/expected.gz" "$destination"
  [[ ! -e "$staged" ]]
done
printf retained >"$staged"
ln -s "$escaped/source/sentinel" "$workspace/publish-dist/link.gz"
for invalid in "$workspace/publish-dist/link.gz" "$workspace/publish-dist" \
    "$destination/child.gz"; do
  if cmake -DLONEJSON_ROOT="$root" -DSTAGED="$staged" \
      -DDESTINATION="$invalid" -P "$workspace/publish.cmake" \
      >"$workspace/publish-failure.log" 2>&1; then
    echo "unsafe or failed artifact publication accepted: $invalid" >&2; exit 1
  fi
  [[ $(cat "$staged") == retained ]]
  [[ $(cat "$escaped/source/sentinel") == retained ]]
  cmp "$workspace/expected.gz" "$destination"
done
