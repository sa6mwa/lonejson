#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

fake_bin="$tmp_dir/toolchain/bin"
build_dir="$tmp_dir/build"
mkdir -p "$fake_bin" "$build_dir"

printf '#!/usr/bin/env bash\nexit 0\n' >"$fake_bin/arm64-apple-darwin25-clang"
chmod +x "$fake_bin/arm64-apple-darwin25-clang"

cat >"$fake_bin/arm64-apple-darwin25-otool" <<'EOF'
#!/usr/bin/env bash
mode=$1
file=$2
case "$(basename -- "$file"):$mode" in
  valid.dylib:-D)
    printf '%s:\n@rpath/liblonejson.19.dylib\n' "$file" ;;
  valid.dylib:-L)
    printf '%s:\n\t@rpath/liblonejson.19.dylib (compatibility version 19.0.0, current version 0.35.1)\n\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1356.0.0)\n' "$file" ;;
  valid.dylib:-l)
    printf 'Load command 0\n      cmd LC_ID_DYLIB\n' ;;
  bare.dylib:-D)
    printf '%s:\nliblonejson.19.dylib\n' "$file" ;;
  bare.dylib:-L)
    printf '%s:\n\tliblonejson.19.dylib (compatibility version 19.0.0, current version 0.35.1)\n' "$file" ;;
  bare.dylib:-l)
    printf 'Load command 0\n' ;;
  localdep.dylib:-D)
    printf '%s:\n@rpath/liblonejson.19.dylib\n' "$file" ;;
  localdep.dylib:-L)
    printf '%s:\n\t@rpath/liblonejson.19.dylib (compatibility version 19.0.0, current version 0.35.1)\n\t/home/build/libcurl.dylib (compatibility version 1.0.0, current version 1.0.0)\n' "$file" ;;
  localdep.dylib:-l)
    printf 'Load command 0\n' ;;
  badrpath.dylib:-D)
    printf '%s:\n@rpath/liblonejson.19.dylib\n' "$file" ;;
  badrpath.dylib:-L)
    printf '%s:\n\t@rpath/liblonejson.19.dylib (compatibility version 19.0.0, current version 0.35.1)\n\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1356.0.0)\n' "$file" ;;
  badrpath.dylib:-l)
    printf 'Load command 0\n          cmd LC_RPATH\n         path /tmp/release/lib (offset 12)\n' ;;
  *)
    exit 1 ;;
esac
EOF
chmod +x "$fake_bin/arm64-apple-darwin25-otool"

cat >"$build_dir/CMakeCache.txt" <<EOF
CMAKE_C_COMPILER:FILEPATH=$fake_bin/arm64-apple-darwin25-clang
CMAKE_OTOOL:FILEPATH=$fake_bin/arm64-apple-darwin25-otool
EOF

for dylib in valid bare localdep badrpath; do
  printf 'fake macho\n' >"$tmp_dir/$dylib.dylib"
done

"$repo_root/scripts/check_darwin_macho_metadata.sh" \
  "$repo_root" "$build_dir" arm64-apple-darwin "$tmp_dir/valid.dylib"

if "$repo_root/scripts/check_darwin_macho_metadata.sh" \
    "$repo_root" "$build_dir" arm64-apple-darwin "$tmp_dir/bare.dylib" \
    >"$tmp_dir/bare.log" 2>&1; then
  printf 'expected bare install name to fail\n' >&2
  exit 1
fi
grep -F 'non-rpath Darwin install name' "$tmp_dir/bare.log" >/dev/null

if "$repo_root/scripts/check_darwin_macho_metadata.sh" \
    "$repo_root" "$build_dir" arm64-apple-darwin "$tmp_dir/localdep.dylib" \
    >"$tmp_dir/localdep.log" 2>&1; then
  printf 'expected local dependency path to fail\n' >&2
  exit 1
fi
grep -F 'forbidden Darwin loader metadata' "$tmp_dir/localdep.log" >/dev/null

if "$repo_root/scripts/check_darwin_macho_metadata.sh" \
    "$repo_root" "$build_dir" arm64-apple-darwin "$tmp_dir/badrpath.dylib" \
    >"$tmp_dir/badrpath.log" 2>&1; then
  printf 'expected absolute rpath to fail\n' >&2
  exit 1
fi
grep -F 'non-loader-relative Darwin rpath' "$tmp_dir/badrpath.log" >/dev/null
