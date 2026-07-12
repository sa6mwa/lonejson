#!/usr/bin/env bash
set -euo pipefail

# Lifecycle-managed LLVM toolchain for diagnostics that require compiler-rt:
# MemorySanitizer and libFuzzer. Normal builds use the Bootlin GCC collections.

version='22.1.6'
clang_resource_version='22'
clang_runtime_triple='x86_64-unknown-linux-gnu'
archive_name="LLVM-${version}-Linux-X64.tar.xz"
archive_sha256='c5ac8ef89ca39d30cb32e9b83772f995dd891c685ebc188d593c943a64d5f8b5'
root_name="llvm-${version}-linux-x64"
url="https://github.com/llvm/llvm-project/releases/download/llvmorg-${version}/${archive_name}"

die() { printf 'cpkt-llvm: %s\n' "$*" >&2; exit 1; }

cache_root() {
  if [[ -n "${CPKT_TOOLCHAIN_CACHE:-}" ]]; then printf '%s\n' "$CPKT_TOOLCHAIN_CACHE"
  elif [[ -n "${XDG_CACHE_HOME:-}" ]]; then printf '%s/c.pkt.systems/toolchains\n' "$XDG_CACHE_HOME"
  elif [[ -n "${HOME:-}" ]]; then printf '%s/.cache/c.pkt.systems/toolchains\n' "$HOME"
  else die 'HOME, XDG_CACHE_HOME, or CPKT_TOOLCHAIN_CACHE is required'; fi
}

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | awk '{print $1}'
  else die 'sha256sum or shasum is required'; fi
}

root_path() { printf '%s/roots/%s\n' "$(cache_root)" "$root_name"; }

toolchain_ready() {
  local root=$1
  [[ -x "$root/bin/clang" ]] && [[ -x "$root/bin/clang++" ]] &&
    [[ -x "$root/bin/llvm-ar" ]] && [[ -x "$root/bin/ld.lld" ]] &&
    [[ -x "$root/bin/llvm-nm" ]] && [[ -x "$root/bin/llvm-objcopy" ]] &&
    [[ -x "$root/bin/llvm-objdump" ]] && [[ -x "$root/bin/llvm-readelf" ]] &&
    [[ -f "$root/lib/clang/${clang_resource_version}/lib/${clang_runtime_triple}/libclang_rt.fuzzer.a" ]] &&
    [[ -f "$root/lib/clang/${clang_resource_version}/lib/${clang_runtime_triple}/libclang_rt.msan.a" ]]
}

ensure() {
  local root archive_dir archive tmp extract
  root="$(root_path)"
  toolchain_ready "$root" && return
  archive_dir="$(cache_root)/archives"
  archive="$archive_dir/$archive_name"
  mkdir -p "$archive_dir" "$(cache_root)/roots"
  if [[ ! -f "$archive" ]]; then
    tmp="$archive.tmp.$$"
    trap 'rm -f "$tmp"' EXIT HUP INT TERM
    if command -v curl >/dev/null 2>&1; then curl -fL --retry 3 --connect-timeout 20 --output "$tmp" "$url"
    elif command -v wget >/dev/null 2>&1; then wget -O "$tmp" "$url"
    else die 'curl or wget is required to download LLVM'; fi
    [[ "$(sha256_file "$tmp")" == "$archive_sha256" ]] || die "checksum mismatch for $archive_name"
    mv "$tmp" "$archive"
    trap - EXIT HUP INT TERM
  elif [[ "$(sha256_file "$archive")" != "$archive_sha256" ]]; then
    die "cached archive checksum mismatch for $archive"
  fi
  extract="$(cache_root)/roots/.extract-$root_name.$$"
  trap 'rm -rf "$extract"' EXIT HUP INT TERM
  mkdir -p "$extract"
  tar -C "$extract" -xf "$archive"
  [[ -d "$extract/LLVM-${version}-Linux-X64" ]] || die "unexpected archive layout for $archive_name"
  rm -rf "$root"
  mv "$extract/LLVM-${version}-Linux-X64" "$root"
  trap - EXIT HUP INT TERM
  toolchain_ready "$root" || die "incomplete extracted LLVM toolchain: $root"
}

report() {
  local root
  root="$(root_path)"
  toolchain_ready "$root" || die "missing LLVM ${version}; run: $0 ensure"
  printf 'version=%s\ncache=%s\nsource=llvm-project\nroot=%s\n' "$version" "$(cache_root)" "$root"
  printf 'cc=%s\ncxx=%s\nld=%s\nar=%s\nnm=%s\nobjcopy=%s\nobjdump=%s\nreadelf=%s\n' \
    "$root/bin/clang" "$root/bin/clang++" "$root/bin/ld.lld" "$root/bin/llvm-ar" "$root/bin/llvm-nm" "$root/bin/llvm-objcopy" "$root/bin/llvm-objdump" "$root/bin/llvm-readelf"
  printf 'fuzzer_runtime=%s\nmsan_runtime=%s\n' \
    "$root/lib/clang/${clang_resource_version}/lib/${clang_runtime_triple}/libclang_rt.fuzzer.a" \
    "$root/lib/clang/${clang_resource_version}/lib/${clang_runtime_triple}/libclang_rt.msan.a"
}

case "${1:-}" in
  ensure) [[ $# -eq 1 ]] || die 'usage: cpkt-llvm.sh ensure'; ensure ;;
  discover) [[ $# -eq 1 ]] || die 'usage: cpkt-llvm.sh discover'; report ;;
  -h|--help|'') printf 'usage: %s <ensure|discover>\n' "$0" ;;
  *) die "unknown command: $1" ;;
esac
