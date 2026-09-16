#!/usr/bin/env bash
set -euo pipefail

# CMake canonicalizes the dynamic loader before linking a native executable.
# The checker must therefore accept a CMAKE_SYSROOT below a cache symlink.

repo_root=${1:?usage: test_bootlin_runtime_symlink_cache.sh REPO_ROOT}
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

real_cache="$tmp_dir/real-cache"
cache_link="$tmp_dir/toolchain-cache"
collection_root="$real_cache/roots/x86-64--glibc--stable-2026.08-1"
real_sysroot="$collection_root/sysroot"
linked_sysroot="$cache_link/roots/x86-64--glibc--stable-2026.08-1/sysroot"
loader="$real_sysroot/lib/ld-linux-x86-64.so.2"
fake_readelf="$tmp_dir/readelf"
build_dir="$tmp_dir/build"
executable="$tmp_dir/consumer"

mkdir -p "$real_sysroot/lib" "$build_dir"
ln -s "$real_cache" "$cache_link"
touch "$executable"

cat >"$loader" <<EOF
#!/usr/bin/env bash
if [[ "\${1:-}" == --list ]]; then
  printf 'libc.so.6 => %s/lib/libc.so.6 (0x00000000)\n' "$linked_sysroot"
fi
EOF
chmod +x "$loader"

cat >"$fake_readelf" <<EOF
#!/usr/bin/env bash
case "\${1:-}" in
  -l) printf 'Requesting program interpreter: %s]\n' "$loader" ;;
  -d) printf '0x000000000000000f (RPATH) Library rpath: [%s/lib]\n' "$linked_sysroot" ;;
  *) exit 2 ;;
esac
EOF
chmod +x "$fake_readelf"

cat >"$build_dir/CMakeCache.txt" <<EOF
CMAKE_READELF:FILEPATH=$fake_readelf
CMAKE_SYSROOT:PATH=$linked_sysroot
LONEJSON_BOOTLIN_LIBC:STRING=gnu
LONEJSON_BOOTLIN_TOOLCHAIN_ROOT:PATH=$cache_link/roots/x86-64--glibc--stable-2026.08-1
EOF

"$repo_root/tests/test_bootlin_runtime.sh" "$build_dir" "$executable"
