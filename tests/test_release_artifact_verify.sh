#!/usr/bin/env bash
set -euo pipefail

repo_root=$1

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

write_checksums() {
  local dist_dir=$1
  local manifest=$2
  shift
  shift
  (cd "$dist_dir" && sha256sum "$@") >"$manifest"
}

dist_dir="$tmp_dir/good/dist"
mkdir -p "$dist_dir"
good_header="$dist_dir/lonejson-1.2.3.h.gz"
printf '%s\n' '#define LONEJSON_VERSION_MAJOR 1' | gzip -9 >"$good_header"
write_checksums "$dist_dir" "$dist_dir/lonejson-1.2.3-CHECKSUMS" \
  "$(basename "$good_header")"
"$repo_root/scripts/verify_release_artifacts.sh" \
  "$repo_root" "$dist_dir/lonejson-1.2.3-CHECKSUMS" >/dev/null

dist_dir="$tmp_dir/bad-sanitizer-dist/dist"
mkdir -p "$dist_dir"
bad_sanitizer_root="$tmp_dir/bad-sanitizer/liblonejson-1.2.3-x86_64-linux-gnu/lib"
mkdir -p "$bad_sanitizer_root"
printf '%s\n' '__asan_init' >"$bad_sanitizer_root/liblonejson.so"
tar -czf "$dist_dir/liblonejson-1.2.3-x86_64-linux-gnu.tar.gz" \
  -C "$tmp_dir/bad-sanitizer" liblonejson-1.2.3-x86_64-linux-gnu
write_checksums "$dist_dir" "$dist_dir/bad-sanitizer-CHECKSUMS" \
  liblonejson-1.2.3-x86_64-linux-gnu.tar.gz
if "$repo_root/scripts/verify_release_artifacts.sh" \
    "$repo_root" "$dist_dir/bad-sanitizer-CHECKSUMS" \
    >"$tmp_dir/bad-sanitizer.out" 2>"$tmp_dir/bad-sanitizer.err"; then
  printf 'expected sanitizer marker leak to fail release artifact verification\n' >&2
  exit 1
fi
grep -q 'sanitizer or fuzzer instrumentation marker leaked' \
  "$tmp_dir/bad-sanitizer.err"

dist_dir="$tmp_dir/bad-path-dist/dist"
mkdir -p "$dist_dir"
bad_path_rockspec="$dist_dir/lonejson-1.2.3-1.rockspec"
cat >"$bad_path_rockspec" <<EOF
package = "lonejson"
version = "1.2.3-1"
source = {
  url = "file://${repo_root}/dist/lonejson-1.2.3.tar.gz",
}
EOF
write_checksums "$dist_dir" "$dist_dir/bad-path-CHECKSUMS" \
  "$(basename "$bad_path_rockspec")"
if "$repo_root/scripts/verify_release_artifacts.sh" \
    "$repo_root" "$dist_dir/bad-path-CHECKSUMS" \
    >"$tmp_dir/bad-path.out" 2>"$tmp_dir/bad-path.err"; then
  printf 'expected repository path leak to fail release artifact verification\n' >&2
  exit 1
fi
grep -q 'repository path leaked' "$tmp_dir/bad-path.err"
