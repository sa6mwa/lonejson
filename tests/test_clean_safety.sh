#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

set +e
"$repo_root/scripts/clean.sh" --root "$tmp_dir" >"$tmp_dir/stdout" 2>"$tmp_dir/stderr"
status=$?
set -e
if [[ "$status" -eq 0 ]]; then
  printf 'clean.sh accepted an arbitrary root\n' >&2
  exit 1
fi
grep -F 'refusing to clean unsafe root' "$tmp_dir/stderr" >/dev/null

script_text=$(<"$repo_root/scripts/clean.sh")
for generated_path in \
  'build' \
  'dist' \
  '.cache' \
  '.luarocks-build' \
  'examples/bin' \
  'lonejson' \
  'devenv/volumes'; do
  grep -F "\"\$root_dir\"/$generated_path" <<<"$script_text" >/dev/null
done

grep -F 'CPKT_DEPENDENCY_CACHE' <<<"$script_text" >/dev/null && {
  printf 'clean.sh must not remove the shared dependency archive cache\n' >&2
  exit 1
}
grep -F 'CPKT_TOOLCHAIN_CACHE' <<<"$script_text" >/dev/null && {
  printf 'clean.sh must not remove the shared toolchain cache\n' >&2
  exit 1
}

grep -F 'refusing to remove unexpected path' <<<"$script_text" >/dev/null

set +e
"$repo_root/scripts/clean.sh" --dist-only --root "$repo_root" \
  --dist-dir "$repo_root" >"$tmp_dir/dist-root-stdout" 2>"$tmp_dir/dist-root-stderr"
status=$?
set -e
if [[ "$status" -eq 0 ]]; then
  printf 'clean.sh accepted the repository root as a custom dist directory\n' >&2
  exit 1
fi
grep -F 'refusing to clean unsafe dist directory' "$tmp_dir/dist-root-stderr" >/dev/null

custom_dist_dir="$tmp_dir/custom-dist"
custom_build_dir="$tmp_dir/custom-build"
cmake -S "$repo_root" -B "$custom_build_dir" -G Ninja \
  -D LONEJSON_DIST_DIR="$custom_dist_dir" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF >/dev/null
cmake --build "$custom_build_dir" --target package-clean-dist >/dev/null
[[ -f "$custom_dist_dir/.lonejson-dist" ]]
printf '%s\n' generated >"$custom_dist_dir/artifact"
cmake --build "$custom_build_dir" --target package-clean-dist >/dev/null
if [[ -e "$custom_dist_dir/artifact" ]]; then
  printf 'package-clean-dist did not remove the generated custom artifact\n' >&2
  exit 1
fi

unsafe_dist_dir="$tmp_dir/unmarked-dist"
mkdir -p "$unsafe_dist_dir"
printf '%s\n' unrelated >"$unsafe_dist_dir/artifact"
if "$repo_root/scripts/clean.sh" --dist-only --root "$repo_root" \
    --dist-dir "$unsafe_dist_dir" >"$tmp_dir/unmarked-stdout" 2>"$tmp_dir/unmarked-stderr"; then
  printf 'clean.sh accepted an unmarked external custom dist directory\n' >&2
  exit 1
fi
grep -F 'refusing to clean unmarked custom dist directory' \
  "$tmp_dir/unmarked-stderr" >/dev/null
[[ -f "$unsafe_dist_dir/artifact" ]]

if "$repo_root/scripts/clean.sh" --dist-only --root "$repo_root" \
    --dist-dir "$repo_root/src" >"$tmp_dir/source-stdout" 2>"$tmp_dir/source-stderr"; then
  printf 'clean.sh accepted a project source directory as custom dist\n' >&2
  exit 1
fi
grep -F 'refusing to clean project directory as custom dist' \
  "$tmp_dir/source-stderr" >/dev/null
[[ -f "$repo_root/src/lonejson.c" ]]
