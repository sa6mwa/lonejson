#!/usr/bin/env bash
# Provision native AFL++ GCC-plugin instrumentation for the pkt.systems lifecycle.
set -euo pipefail

version=5.02c
revision=2
archive_name="AFLplusplus-${version}.tar.gz"
archive_sha256=118415843e5d289d63bd6d8f2252c18212978f15ac9e86acbbc75766cd45acde
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
bootlin="$repo_root/scripts/cpkt-toolchains.sh"

die() { printf 'cpkt-aflpp: %s\n' "$*" >&2; exit 1; }
cache() {
  if [[ -n "${CPKT_TOOLCHAIN_CACHE:-}" ]]; then printf '%s\n' "$CPKT_TOOLCHAIN_CACHE"
  elif [[ -n "${XDG_CACHE_HOME:-}" ]]; then printf '%s/c.pkt.systems/toolchains\n' "$XDG_CACHE_HOME"
  elif [[ -n "${HOME:-}" ]]; then printf '%s/.cache/c.pkt.systems/toolchains\n' "$HOME"
  else die 'HOME, XDG_CACHE_HOME, or CPKT_TOOLCHAIN_CACHE is required'; fi
}
lock_timeout() {
  local timeout=${CPKT_TOOLCHAIN_LOCK_TIMEOUT:-600}
  [[ "$timeout" =~ ^[1-9][0-9]*$ ]] ||
    die 'CPKT_TOOLCHAIN_LOCK_TIMEOUT must be a positive integer number of seconds'
  printf '%s\n' "$timeout"
}
root() {
  local description archive
  description=$("$bootlin" discover x86_64-linux-gnu)
  archive=$(value archive "$description")
  [[ "$archive" =~ ^[a-zA-Z0-9._-]+\.tar\.xz$ ]] || die 'Bootlin resolver returned no valid archive identity'
  printf '%s/roots/aflplusplus-%s-%s\n' "$(cache)" "$version" "${archive%.tar.xz}"
}
value() { sed -n "s/^$1=//p" <<<"$2" | tail -1; }
ready() {
  local r=$1
  local published_root=${2:-$r}
  [[ -x "$r/bin/afl-fuzz" && -x "$r/bin/cpkt-afl-gcc" && -x "$r/bin/cpkt-afl-g++" &&
     -f "$r/lib/afl/afl-gcc-pass.so" && -f "$r/lib/afl/afl-compiler-rt.o" &&
     -f "$r/.cpkt-aflpp-revision-$revision" ]] &&
    grep -F "export AFL_PATH=$published_root/lib/afl" "$r/bin/cpkt-afl-gcc" >/dev/null &&
    grep -F "export AFL_PATH=$published_root/lib/afl" "$r/bin/cpkt-afl-g++" >/dev/null
}

ensure() {
  [[ "$(uname -s)" = Linux ]] || die 'AFL++ GCC-plugin fuzzing is native Linux-only'
  case "$(uname -m)" in x86_64|amd64) ;; *) die 'native x86_64 Linux is required; no cross, emulator, or QEMU runner is supported' ;; esac
  local r c archive desc cc cxx bootlin_root tmp src dl helper lock_dir lock_file lock_fd lock_wait_seconds
  r=$(root); c=$(cache); archive="$c/archives/$archive_name"
  ready "$r" && return
  lock_dir="$c/locks"
  lock_file="$lock_dir/${r##*/}.lock"
  lock_wait_seconds="$(lock_timeout)"
  command -v flock >/dev/null 2>&1 || die 'flock is required for shared AFL++ cache provisioning'
  mkdir -p "$c/archives" "$lock_dir"
  exec {lock_fd}>"$lock_file"
  flock -w "$lock_wait_seconds" "$lock_fd" ||
    die "timed out after ${lock_wait_seconds}s waiting for shared AFL++ cache lock: $lock_file"

  # Another checkout may have finished publishing this immutable AFL++ root
  # while this process waited for the shared-cache lock.
  if ready "$r"; then
    flock -u "$lock_fd"
    exec {lock_fd}>&-
    return
  fi

  [[ -x "$bootlin" ]] || die "Bootlin resolver missing: $bootlin"
  "$bootlin" ensure x86_64-linux-gnu >/dev/null
  desc=$("$bootlin" discover x86_64-linux-gnu)
  cc=$(value cc "$desc"); cxx=$(value cxx "$desc"); bootlin_root=$(value root "$desc")
  [[ -x "$cc" && -x "$cxx" && -f "$bootlin_root/include/gmp.h" ]] || die 'Bootlin GCC plugin headers are incomplete'
  if ! [[ -f "$archive" ]] || ! printf '%s  %s\n' "$archive_sha256" "$archive" | sha256sum -c - >/dev/null 2>&1; then
    rm -f "$archive"; dl="$archive.tmp.$$"
    if command -v curl >/dev/null; then
      curl -fL --retry 3 --connect-timeout 20 -o "$dl" "https://github.com/AFLplusplus/AFLplusplus/archive/refs/tags/v${version}.tar.gz" || { rm -f "$dl"; die 'AFL++ download failed'; }
    elif command -v wget >/dev/null; then
      wget -O "$dl" "https://github.com/AFLplusplus/AFLplusplus/archive/refs/tags/v${version}.tar.gz" || { rm -f "$dl"; die 'AFL++ download failed'; }
    else die 'curl or wget is required to download AFL++'; fi
    printf '%s  %s\n' "$archive_sha256" "$dl" | sha256sum -c - >/dev/null || { rm -f "$dl"; die 'AFL++ checksum mismatch'; }
    mv "$dl" "$archive"
  fi
  tmp="$c/.aflplusplus.$$"; trap 'rm -rf "${tmp:-}"' EXIT HUP INT TERM
  mkdir -p "$tmp/extract" "$tmp/root/bin" "$tmp/root/lib/afl"
  tar -xzf "$archive" -C "$tmp/extract"; src="$tmp/extract/AFLplusplus-$version"
  [[ -d "$src" ]] || die "unexpected archive layout: $archive_name"
  # The published root is immutable but has a stable cache path.  AFL++
  # embeds HELPER_PATH in its compiler driver, so it must never refer to the
  # temporary extraction root that is moved away after provisioning.
  helper="$r/lib/afl"
  (
    cd "$src"
    # The lifecycle only consumes afl-fuzz, afl-showmap, and the GCC-plugin
    # driver below.  Asking AFL++ 5.02c for its unused auxiliary utilities
    # leaves afl-tmin without a generated Python object on fresh builds.
    make -j1 NO_PYTHON=1 CC="$cc" CXX="$cxx" PREFIX="$tmp/root" HELPER_PATH="$helper" BIN_PATH="$tmp/root/bin" afl-fuzz afl-showmap
    "$cc" -O3 -funroll-loops -fPIC -Wall -g -Iinclude -Iinstrumentation "-DAFL_PATH=\"$helper\"" "-DBIN_PATH=\"$r/bin\"" -DLLVM_BINDIR=\"\" -DVERSION=\"++$version\" -DLLVM_LIBDIR=\"\" -DLLVM_VERSION=\"\" -DAFL_CLANG_FLTO=\"\" -DAFL_REAL_LD=\"\" -DAFL_CLANG_LDPATH=\"\" -DAFL_CLANG_FUSELD=\"\" "-DCLANG_BIN=\"$cc\"" "-DCLANGPP_BIN=\"$cxx\"" -DUSE_BINDIR=1 -Wno-unused-function -Wno-deprecated -c src/afl-common.c -o instrumentation/afl-common.o
    "$cc" -O3 -funroll-loops -fPIC -Wall -g -Iinclude -Iinstrumentation "-DAFL_PATH=\"$helper\"" "-DBIN_PATH=\"$r/bin\"" -DLLVM_BINDIR=\"\" -DVERSION=\"++$version\" -DLLVM_LIBDIR=\"\" -DLLVM_VERSION=\"\" -DAFL_CLANG_FLTO=\"\" -DAFL_REAL_LD=\"\" -DAFL_CLANG_LDPATH=\"\" -DAFL_CLANG_FUSELD=\"\" "-DCLANG_BIN=\"$cc\"" "-DCLANGPP_BIN=\"$cxx\"" -DUSE_BINDIR=1 -Wno-unused-function -Wno-deprecated "-DAFL_INCLUDE_PATH=\"$r/include/afl\"" src/afl-cc.c instrumentation/afl-common.o -o afl-cc -DLLVM_MINOR=0 -DLLVM_MAJOR=0 -DCFLAGS_OPT=\"\" -lm
    ln -sf afl-cc afl-gcc-fast; ln -sf afl-cc afl-g++-fast
    make -j1 -f GNUmakefile.gcc_plugin CC="$cc" CXX="$cxx" PREFIX="$tmp/root" HELPER_PATH="$helper" BIN_PATH="$tmp/root/bin" CXXFLAGS="-O3 -g -funroll-loops -I$bootlin_root/include" LDFLAGS="-L$bootlin_root/lib -Wl,-rpath,$bootlin_root/lib"
    install -m755 afl-fuzz afl-showmap afl-cc "$tmp/root/bin/"
    ln -sf afl-cc "$tmp/root/bin/afl-gcc-fast"; ln -sf afl-cc "$tmp/root/bin/afl-g++-fast"
    install -m755 afl-gcc-pass.so afl-gcc-cmplog-pass.so afl-gcc-cmptrs-pass.so "$tmp/root/lib/afl/"
    install -m644 afl-compiler-rt.o dynamic_list.txt "$tmp/root/lib/afl/"
  )
  cat > "$tmp/root/bin/cpkt-afl-gcc" <<EOF
#!/usr/bin/env bash
set -euo pipefail
export AFL_PATH=$(printf '%q' "$helper")
export AFL_CC=$(printf '%q' "$cc")
exec $(printf '%q' "$r/bin/afl-gcc-fast") "\$@"
EOF
  cat > "$tmp/root/bin/cpkt-afl-g++" <<EOF
#!/usr/bin/env bash
set -euo pipefail
export AFL_PATH=$(printf '%q' "$helper")
export AFL_CC=$(printf '%q' "$cc")
export AFL_CXX=$(printf '%q' "$cxx")
exec $(printf '%q' "$r/bin/afl-g++-fast") "\$@"
EOF
  chmod +x "$tmp/root/bin/cpkt-afl-gcc" "$tmp/root/bin/cpkt-afl-g++"; touch "$tmp/root/.cpkt-aflpp-revision-$revision"
  ready "$tmp/root" "$r" || die 'incomplete AFL++ build'
  rm -rf "$r"; mv "$tmp/root" "$r"; trap - EXIT HUP INT TERM; rm -rf "$tmp"
  flock -u "$lock_fd"
  exec {lock_fd}>&-
}

report() {
  ensure
  local r
  r=$(root)
  printf 'version=%s\ncache=%s\nsource=aflplusplus\nroot=%s\nafl_fuzz=%s\nafl_showmap=%s\ncc=%s\ncxx=%s\nhelper=%s\n' "$version" "$(cache)" "$r" "$r/bin/afl-fuzz" "$r/bin/afl-showmap" "$r/bin/cpkt-afl-gcc" "$r/bin/cpkt-afl-g++" "$r/lib/afl"
}
env_out() {
  local d cc cxx r
  ensure; d=$("$bootlin" discover x86_64-linux-gnu); cc=$(value cc "$d"); cxx=$(value cxx "$d"); r=$(root)
  printf 'export CPKT_AFLPP_ROOT=%q\nexport AFL_PATH=%q\nexport AFL_CC=%q\nexport AFL_CXX=%q\nexport CC=%q\nexport CXX=%q\n' "$r" "$r/lib/afl" "$cc" "$cxx" "$r/bin/cpkt-afl-gcc" "$r/bin/cpkt-afl-g++"
}

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
  return 0
fi

case "${1:-}" in
  ensure) [[ $# -eq 1 ]] || die 'usage: cpkt-aflpp.sh ensure'; ensure ;;
  discover) [[ $# -eq 1 ]] || die 'usage: cpkt-aflpp.sh discover'; report ;;
  env) [[ $# -eq 1 ]] || die 'usage: cpkt-aflpp.sh env'; env_out ;;
  *) die 'usage: cpkt-aflpp.sh {ensure|discover|env}' ;;
esac
