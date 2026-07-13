#!/usr/bin/env bash
set -euo pipefail

# Lifecycle-managed, reproducible Linux toolchain resolver.  The downloaded
# collections intentionally live outside a checkout so they can be shared by
# c.pkt.systems projects and refreshed without workstation-specific paths.

die() {
  printf 'cpkt-toolchains: %s\n' "$*" >&2
  exit 1
}

cache_root() {
  if [[ -n "${CPKT_TOOLCHAIN_CACHE:-}" ]]; then
    printf '%s\n' "$CPKT_TOOLCHAIN_CACHE"
  elif [[ -n "${XDG_CACHE_HOME:-}" ]]; then
    printf '%s/c.pkt.systems/toolchains\n' "$XDG_CACHE_HOME"
  elif [[ -n "${HOME:-}" ]]; then
    printf '%s/.cache/c.pkt.systems/toolchains\n' "$HOME"
  else
    die 'HOME, XDG_CACHE_HOME, or CPKT_TOOLCHAIN_CACHE is required'
  fi
}

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | awk '{print $1}'
  else
    die 'sha256sum or shasum is required'
  fi
}

download_file() {
  local url=$1 destination=$2
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 --connect-timeout 20 --output "$destination" "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$destination" "$url"
  else
    die 'curl or wget is required to download Bootlin toolchains'
  fi
}

toolchain_meta() {
  case "$1" in
    x86_64-linux-gnu)
      printf '%s\n' 'x86-64|x86-64--glibc--stable-2025.08-1|760acd5c3159448b618e237b61935335baada74fe0cdc0d7611826cb49b41c8c|x86_64-linux|x86_64-buildroot-linux-gnu/sysroot'
      ;;
    x86_64-linux-musl)
      printf '%s\n' 'x86-64|x86-64--musl--stable-2025.08-1|09fca3aa89540f1b01b5f4210d488cbeb00f522044c53e9989b1dd8a38076912|x86_64-linux|x86_64-buildroot-linux-musl/sysroot'
      ;;
    aarch64-linux-gnu)
      printf '%s\n' 'aarch64|aarch64--glibc--stable-2025.08-1|dfb47eee874eef9e8a7fc042eee4e0a183f444b6bcde6a82fef8f009918389c9|aarch64-linux|aarch64-buildroot-linux-gnu/sysroot'
      ;;
    aarch64-linux-musl)
      printf '%s\n' 'aarch64|aarch64--musl--stable-2025.08-1|defba831ffa1175236f137069333e21ed46d4d19feb5080a90cf248b6fc2cb08|aarch64-linux|aarch64-buildroot-linux-musl/sysroot'
      ;;
    armhf-linux-gnu)
      printf '%s\n' 'armv7-eabihf|armv7-eabihf--glibc--stable-2025.08-1|97d6fbaf19832002f3d6aa8fd31b2d29c1dc7b0752f4ae8ed35860fd33c1f9b4|arm-linux|arm-buildroot-linux-gnueabihf/sysroot'
      ;;
    armhf-linux-musl)
      printf '%s\n' 'armv7-eabihf|armv7-eabihf--musl--stable-2025.08-1|2f3a34458c3a8b961bd09f89669130fcdc4c1dbc6e31ada720527e4ad3741c11|arm-linux|arm-buildroot-linux-musleabihf/sysroot'
      ;;
    *) die "unsupported or non-downloadable target: $1" ;;
  esac
}

toolchain_values() {
  local target=$1 meta arch name sha256 prefix sysroot_rel
  meta="$(toolchain_meta "$target")"
  IFS='|' read -r arch name sha256 prefix sysroot_rel <<<"$meta"
  printf '%s|%s|%s|%s|%s|%s\n' "$arch" "$name" "$sha256" "$prefix" "$sysroot_rel" "$(cache_root)/roots/$name"
}

compiler_file() {
  "$1" -print-file-name="$2"
}

existing_compiler_file() {
  local path dir
  path="$(compiler_file "$1" "$2")"
  [[ "$path" != "$2" && -f "$path" ]] || return 1
  dir="$(CDPATH= cd -- "$(dirname -- "$path")" && pwd -P)"
  printf '%s/%s\n' "$dir" "$(basename -- "$path")"
}

toolchain_ready() {
  local root=$1 prefix=$2 sysroot=$3
  [[ -x "$root/bin/$prefix-gcc" ]] &&
    [[ -x "$root/bin/$prefix-g++" ]] &&
    [[ -x "$root/bin/$prefix-ld" ]] &&
    [[ -x "$root/bin/$prefix-ar" ]] &&
    [[ -x "$root/bin/$prefix-ranlib" ]] &&
    [[ -x "$root/bin/$prefix-strip" ]] &&
    [[ -x "$root/bin/$prefix-nm" ]] &&
    [[ -x "$root/bin/$prefix-objcopy" ]] &&
    [[ -x "$root/bin/$prefix-objdump" ]] &&
    [[ -x "$root/bin/$prefix-addr2line" ]] &&
    [[ -x "$root/bin/$prefix-gdb" ]] &&
    [[ -x "$root/bin/$prefix-readelf" ]] &&
    [[ -f "$sysroot/usr/include/stdio.h" || -f "$sysroot/include/stdio.h" ]] &&
    [[ -e "$sysroot/usr/lib/libc.so" || -e "$sysroot/lib/libc.so" ||
       -e "$sysroot/lib/libc.so.6" ]] &&
    existing_compiler_file "$root/bin/$prefix-g++" libstdc++.a >/dev/null &&
    existing_compiler_file "$root/bin/$prefix-g++" libgcc.a >/dev/null
}

ensure_target() {
  local target=$1 values arch name sha256 prefix sysroot_rel root archive_dir archive tmp extract
  values="$(toolchain_values "$target")"
  IFS='|' read -r arch name sha256 prefix sysroot_rel root <<<"$values"
  if toolchain_ready "$root" "$prefix" "$root/$sysroot_rel"; then
    return
  fi

  archive_dir="$(cache_root)/archives"
  archive="$archive_dir/$name.tar.xz"
  mkdir -p "$archive_dir" "$(cache_root)/roots"
  if [[ ! -f "$archive" ]]; then
    tmp="$archive.tmp.$$"
    trap 'rm -f "$tmp"' EXIT HUP INT TERM
    download_file "https://toolchains.bootlin.com/downloads/releases/toolchains/$arch/tarballs/$name.tar.xz" "$tmp"
    [[ "$(sha256_file "$tmp")" == "$sha256" ]] || die "checksum mismatch for $name.tar.xz"
    mv "$tmp" "$archive"
    trap - EXIT HUP INT TERM
  elif [[ "$(sha256_file "$archive")" != "$sha256" ]]; then
    die "cached archive checksum mismatch for $archive"
  fi

  extract="$(cache_root)/roots/.extract-$name.$$"
  trap 'rm -rf "$extract"' EXIT HUP INT TERM
  mkdir -p "$extract"
  tar -C "$extract" -xf "$archive"
  [[ -d "$extract/$name/bin" ]] || die "unexpected archive layout for $name.tar.xz"
  rm -rf "$root"
  mv "$extract/$name" "$root"
  trap - EXIT HUP INT TERM
  toolchain_ready "$root" "$prefix" "$root/$sysroot_rel" || die "incomplete extracted toolchain: $root"
}

report_target() {
  local target=$1 values arch name sha256 prefix sysroot_rel root cc cxx
  values="$(toolchain_values "$target")"
  IFS='|' read -r arch name sha256 prefix sysroot_rel root <<<"$values"
  toolchain_ready "$root" "$prefix" "$root/$sysroot_rel" || die "missing $target; run: $0 ensure $target"
  printf 'target=%s\n' "$target"
  printf 'libc=%s\n' "${target##*-}"
  printf 'cache=%s\n' "$(cache_root)"
  printf 'source=bootlin\n'
  printf 'root=%s\n' "$root"
  printf 'prefix=%s\n' "$prefix"
  printf 'sysroot=%s\n' "$root/$sysroot_rel"
  cc="$root/bin/$prefix-gcc"
  cxx="$root/bin/$prefix-g++"
  printf 'cc=%s\n' "$cc"
  printf 'cxx=%s\n' "$cxx"
  printf 'ld=%s\n' "$root/bin/$prefix-ld"
  printf 'ar=%s\n' "$root/bin/$prefix-ar"
  printf 'ranlib=%s\n' "$root/bin/$prefix-ranlib"
  printf 'strip=%s\n' "$root/bin/$prefix-strip"
  printf 'nm=%s\n' "$root/bin/$prefix-nm"
  printf 'objcopy=%s\n' "$root/bin/$prefix-objcopy"
  printf 'objdump=%s\n' "$root/bin/$prefix-objdump"
  printf 'addr2line=%s\n' "$root/bin/$prefix-addr2line"
  printf 'gdb=%s\n' "$root/bin/$prefix-gdb"
  printf 'readelf=%s\n' "$root/bin/$prefix-readelf"
  printf 'target_triple=%s\n' "${sysroot_rel%/sysroot}"
  printf 'bin=%s\n' "$root/${sysroot_rel%/sysroot}/bin"
  printf 'libstdcxx_a=%s\n' "$(existing_compiler_file "$cxx" libstdc++.a)"
  printf 'libgcc_a=%s\n' "$(existing_compiler_file "$cxx" libgcc.a)"
}

print_env() {
  local target=$1 description key value
  description="$(report_target "$target")"
  for key in root prefix sysroot cc cxx ld ar ranlib strip nm objcopy objdump addr2line gdb readelf libstdcxx_a libgcc_a; do
    value="$(printf '%s\n' "$description" | sed -n "s/^${key}=//p")"
    [[ -z "$value" ]] || printf 'export %s=%q\n' "CPKT_TOOLCHAIN_${key^^}" "$value"
  done
  printf 'export CPKT_TARGET=%q\n' "$target"
  printf 'export CC=%q\n' "$(printf '%s\n' "$description" | sed -n 's/^cc=//p')"
  printf 'export CXX=%q\n' "$(printf '%s\n' "$description" | sed -n 's/^cxx=//p')"
  printf 'export LD=%q\n' "$(printf '%s\n' "$description" | sed -n 's/^ld=//p')"
  printf 'export AR=%q\n' "$(printf '%s\n' "$description" | sed -n 's/^ar=//p')"
  printf 'export RANLIB=%q\n' "$(printf '%s\n' "$description" | sed -n 's/^ranlib=//p')"
  printf 'export STRIP=%q\n' "$(printf '%s\n' "$description" | sed -n 's/^strip=//p')"
  printf 'export NM=%q\n' "$(printf '%s\n' "$description" | sed -n 's/^nm=//p')"
}

usage() {
  cat <<'EOF'
usage: cpkt-toolchains.sh <ensure|discover|env> <target|all>

Pinned Bootlin collections are stored in:
  ${CPKT_TOOLCHAIN_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/toolchains}
EOF
}

case "${1:-}" in
  ensure)
    [[ $# -eq 2 ]] || die 'usage: cpkt-toolchains.sh ensure <target|all>'
    if [[ "$2" == all ]]; then
      for target in x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl armhf-linux-gnu armhf-linux-musl; do
        ensure_target "$target"
      done
    else
      ensure_target "$2"
    fi
    ;;
  discover)
    [[ $# -eq 2 ]] || die 'usage: cpkt-toolchains.sh discover <target>'
    report_target "$2"
    ;;
  env)
    [[ $# -eq 2 ]] || die 'usage: cpkt-toolchains.sh env <target>'
    print_env "$2"
    ;;
  -h|--help|'') usage ;;
  *) die "unknown command: $1" ;;
esac
