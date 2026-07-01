#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?usage: verify_release_archives.sh REPO_ROOT [CHECKSUMS] [BUILD_ROOT]}
checksums=${2:-}
build_root=${3:-}

repo_root="$(CDPATH= cd -- "$repo_root" && pwd)"
if [[ -z "$checksums" ]]; then
  version="$("$repo_root/scripts/release_version.sh")"
  checksums="$repo_root/dist/lonejson-$version-CHECKSUMS"
fi
if [[ -z "$build_root" ]]; then
  build_root="$repo_root/build"
fi
if [[ ! -f "$checksums" ]]; then
  printf 'missing release checksum manifest: %s\n' "$checksums" >&2
  exit 1
fi

dist_dir="$(CDPATH= cd -- "$(dirname -- "$checksums")" && pwd)"
if [[ -d "$build_root" ]]; then
  build_root="$(CDPATH= cd -- "$build_root" && pwd)"
fi

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'missing required command: %s\n' "$1" >&2
    exit 1
  fi
}

require_file() {
  if [[ ! -e "$1" ]]; then
    printf 'missing required file: %s\n' "$1" >&2
    exit 1
  fi
}

target_preset() {
  local target_id=$1
  case "$target_id" in
    x86_64-linux-gnu) printf '%s\n' linux-gnu-release ;;
    x86_64-linux-musl) printf '%s\n' linux-musl-release ;;
    aarch64-linux-gnu) printf '%s\n' aarch64-linux-gnu-release ;;
    aarch64-linux-musl) printf '%s\n' aarch64-linux-musl-release ;;
    armhf-linux-gnu) printf '%s\n' armhf-linux-gnu-release ;;
    armhf-linux-musl) printf '%s\n' armhf-linux-musl-release ;;
    arm64-apple-darwin) printf '%s\n' arm64-apple-darwin-release ;;
    *)
      printf 'unknown release target id: %s\n' "$target_id" >&2
      exit 1
      ;;
  esac
}

target_cmake_system_name() {
  local target_id=$1
  case "$target_id" in
    *apple-darwin) printf '%s\n' Darwin ;;
    *) printf '%s\n' Linux ;;
  esac
}

target_darwin_deployment_target() {
  printf '%s\n' "${LONEJSON_MACOS_DEPLOYMENT_TARGET:-15.0}"
}

target_raw_compile_flags() {
  local target_id=$1
  case "$target_id" in
    arm64-apple-darwin)
      printf '%s\n' "-mmacosx-version-min=$(target_darwin_deployment_target)"
      printf '%s\n' "-Wno-fuse-ld-path"
      ;;
    *) printf '%s\n' "" ;;
  esac
}

target_raw_link_flags() {
  local target_id=$1
  case "$target_id" in
    arm64-apple-darwin)
      if [[ -z "${LINKER:-}" ]]; then
        printf 'missing target linker for %s\n' "$target_id" >&2
        exit 1
      fi
      printf '%s\n' "-fuse-ld=$LINKER"
      ;;
    *) printf '%s\n' "" ;;
  esac
}

load_target_tools() {
  local preset=$1
  local target_id=$2

  eval "$("$repo_root/scripts/discover_target_tools.sh" \
    --build-dir "$build_root/$preset" \
    --target-id "$target_id")"
}

run_with_target_path() {
  local target_id=$1
  shift

  if [[ "${target_id#*apple-darwin}" != "$target_id" && -n "${LINKER:-}" ]]; then
    local linker_dir
    linker_dir="$(dirname -- "$LINKER")"
    PATH="$linker_dir:$PATH" "$@"
  else
    "$@"
  fi
}

require_linux_origin_rpath() {
  local archive=$1
  local dynamic_metadata=$2
  local rpath_values old_ifs rpath_value

  rpath_values="$(printf '%s\n' "$dynamic_metadata" |
    sed -n 's/.*\(RUNPATH\|RPATH\).*: \[\(.*\)\].*/\2/p')"
  if [[ -z "$rpath_values" ]]; then
    return
  fi
  old_ifs=$IFS
  IFS=':'
  for rpath_value in $rpath_values; do
    if [[ "$rpath_value" != '$ORIGIN' &&
        "${rpath_value#'$ORIGIN'/}" == "$rpath_value" ]]; then
      IFS=$old_ifs
      printf 'non-origin rpath in %s: %s\n' "$archive" "$rpath_value" >&2
      exit 1
    fi
  done
  IFS=$old_ifs
}

archive_target_id() {
  local artifact=$1
  local target_id
  for target_id in \
      x86_64-linux-gnu \
      x86_64-linux-musl \
      aarch64-linux-gnu \
      aarch64-linux-musl \
      armhf-linux-gnu \
      armhf-linux-musl \
      arm64-apple-darwin; do
    case "$artifact" in
      liblonejson-*-"$target_id".tar.gz)
        printf '%s\n' "$target_id"
        return
        ;;
    esac
  done
}

extract_archive() {
  local archive=$1
  local tmp_dir=$2
  local package_root

  tar -xzf "$archive" -C "$tmp_dir"
  package_root="$(find "$tmp_dir" -mindepth 1 -maxdepth 1 -type d | sort | head -n 1)"
  if [[ -z "$package_root" ]]; then
    printf 'failed to inspect archive: %s\n' "$archive" >&2
    exit 1
  fi
  printf '%s\n' "$package_root"
}

require_archive_contract() {
  local archive=$1
  local target_id=$2
  local preset=$3
  local tmp_dir package_root shared_lib dynamic_metadata rpath_paths dependency_manifest

  load_target_tools "$preset" "$target_id"

  tar -tzf "$archive" | while IFS= read -r entry; do
    entry_without_root="${entry#*/}"
    case "$entry_without_root" in
      "" | \
      "include/" | \
      "include/lonejson.h" | \
      "lib/" | \
      "lib/cmake/" | \
      "lib/cmake/lonejson/" | \
      "lib/cmake/lonejson/lonejsonConfig.cmake" | \
      "lib/cmake/lonejson/lonejsonConfigVersion.cmake" | \
      "lib/liblonejson.a" | \
      "lib/liblonejson.so" | \
      "lib/liblonejson.so."* | \
      "lib/liblonejson.dylib" | \
      "lib/liblonejson."*".dylib" | \
      "lib/pkgconfig/" | \
      "lib/pkgconfig/lonejson.pc" | \
      "share/" | \
      "share/lonejson/" | \
      "share/lonejson/dependencies.json" | \
      "share/doc/" | \
      "share/doc/liblonejson/" | \
      "share/doc/liblonejson/LICENSE" | \
      "share/doc/liblonejson/README.md")
        ;;
      *)
        printf 'unexpected file in release archive %s: %s\n' "$archive" "$entry_without_root" >&2
        exit 1
        ;;
    esac
  done

  tmp_dir="$(mktemp -d)"
  package_root="$(extract_archive "$archive" "$tmp_dir")"

  require_file "$package_root/lib/pkgconfig/lonejson.pc"
  require_file "$package_root/lib/cmake/lonejson/lonejsonConfig.cmake"
  require_file "$package_root/lib/cmake/lonejson/lonejsonConfigVersion.cmake"
  dependency_manifest="$package_root/share/lonejson/dependencies.json"
  require_file "$dependency_manifest"
  if grep -RE 'libcurl|libssl|c\.pkt\.systems|\.deps/|/home/|/build/' \
      "$package_root/lib/pkgconfig/lonejson.pc" \
      "$package_root/lib/cmake/lonejson" >/dev/null; then
    printf 'forbidden dependency or path leak in release metadata for %s\n' "$archive" >&2
    exit 1
  fi
  if grep -Eq '^(Requires|Requires.private):.*(curl|ssl|crypto|openssl|OpenSSL)' "$package_root/lib/pkgconfig/lonejson.pc"; then
    printf 'unexpected curl/OpenSSL pkg-config dependency in %s\n' "$archive" >&2
    exit 1
  fi
  if ! grep -Eq '^Libs\.private:.*[[:space:]]-lcrypto([[:space:]]|$)' "$package_root/lib/pkgconfig/lonejson.pc"; then
    printf 'missing static libcrypto pkg-config dependency in %s\n' "$archive" >&2
    exit 1
  fi
  if ! grep -F 'INTERFACE_LINK_LIBRARIES crypto' \
      "$package_root/lib/cmake/lonejson/lonejsonConfig.cmake" >/dev/null; then
    printf 'missing static libcrypto CMake link dependency in %s\n' "$archive" >&2
    exit 1
  fi
  if grep -E '\.deps/|/home/|/build/|file://' "$dependency_manifest" >/dev/null; then
    printf 'forbidden path leak in dependency manifest for %s\n' "$archive" >&2
    exit 1
  fi
  for required_metadata in \
      '"schema": "pkt.systems.dependencies.v1"' \
      '"name": "c.pkt.systems"' \
      '"version": "0.6.0"' \
      "\"target_id\": \"$target_id\"" \
      '"source_url": "https://github.com/sa6mwa/c.pkt.systems/releases/download/v0.6.0/c.pkt.systems-0.6.0-' \
      '"sha256": "' \
      '"bundled": false' \
      '"external": true' \
      '"curl"' \
      '"openssl"'; do
    if ! grep -F "$required_metadata" "$dependency_manifest" >/dev/null; then
      printf 'missing dependency manifest metadata in %s: %s\n' "$archive" "$required_metadata" >&2
      exit 1
    fi
  done

  if [[ "${target_id#*apple-darwin}" != "$target_id" ]]; then
    shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson*.dylib' | sort | head -n 1)"
    if [[ -z "$shared_lib" ]]; then
      printf 'missing packaged shared library in %s\n' "$archive" >&2
      exit 1
    fi
    if [[ -z "$OTOOL" ]]; then
      printf 'missing target otool for %s\n' "$target_id" >&2
      exit 1
    fi
    "$repo_root/scripts/check_darwin_macho_metadata.sh" \
      "$repo_root" \
      "$build_root/$preset" \
      "$target_id" \
      "$shared_lib" \
      "$archive"
    dynamic_metadata="$("$OTOOL" -L "$shared_lib"; "$OTOOL" -l "$shared_lib")"
    case "$dynamic_metadata" in
      *libcurl* | *libssl* | *libcrypto* | *OpenSSL* | *c.pkt.systems* | *".deps/"* | *"$repo_root"* | *"/home/"* | *"/build/"*)
        printf 'forbidden dependency or path leak in %s\n' "$archive" >&2
        exit 1
        ;;
    esac
    rpath_paths="$(printf '%s\n' "$dynamic_metadata" | sed -n 's/^[[:space:]]*path \([^ ]*\).*/\1/p')"
    if [[ -n "$rpath_paths" ]] && printf '%s\n' "$rpath_paths" | grep -Ev '^@loader_path(/|$)' >/dev/null; then
      printf 'non-loader-relative rpath in %s\n' "$archive" >&2
      exit 1
    fi
  else
    shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson.so*' ! -type l | sort | head -n 1)"
    if [[ -z "$shared_lib" ]]; then
      printf 'missing packaged shared library in %s\n' "$archive" >&2
      exit 1
    fi
    if [[ -z "$READELF" ]]; then
      printf 'missing target readelf for %s\n' "$target_id" >&2
      exit 1
    fi
    dynamic_metadata="$("$READELF" -d "$shared_lib")"
    case "$dynamic_metadata" in
      *libcurl* | *libssl* | *libcrypto* | *OpenSSL* | *c.pkt.systems* | *".deps/"* | *"$repo_root"* | *"/home/"* | *"/build/"*)
        printf 'forbidden dependency or path leak in %s\n' "$archive" >&2
        exit 1
        ;;
    esac
    require_linux_origin_rpath "$archive" "$dynamic_metadata"
  fi

  rm -rf "$tmp_dir"
}

require_archive_consumer_metadata() {
  local archive=$1
  local target_id=$2
  local preset=$3
  local tmp_dir package_root consumer_source pkg_config_flags raw_compile_flags
  local raw_link_flags cmake_source_dir cmake_build_dir darwin_deployment_target
  local cmake_system_name

  load_target_tools "$preset" "$target_id"

  tmp_dir="$(mktemp -d)"
  package_root="$(extract_archive "$archive" "$tmp_dir")"

  consumer_source="$tmp_dir/consumer.c"
  cat >"$consumer_source" <<'EOF'
#include <lonejson.h>
#include <stddef.h>

int main(void) {
    lonejson *runtime;
    lonejson_error error;

    lonejson_error_init(&error);
    runtime = lonejson_new(NULL, &error);
    if (runtime == NULL) {
        return 1;
    }
    if (lonejson_validate_cstr(runtime, "{\"ok\":true}", &error) != LONEJSON_STATUS_OK) {
        lonejson_free(runtime);
        return 1;
    }
    lonejson_free(runtime);
    return 0;
}
EOF

  require_command pkg-config
  pkg_config_flags="$(PKG_CONFIG_PATH="$package_root/lib/pkgconfig" pkg-config --cflags --libs lonejson)"
  raw_compile_flags="$(target_raw_compile_flags "$target_id")"
  raw_link_flags="$(target_raw_link_flags "$target_id")"
  # shellcheck disable=SC2086
  run_with_target_path "$target_id" "$CC" "$consumer_source" $raw_compile_flags $pkg_config_flags $raw_link_flags -o "$tmp_dir/pkg-config-consumer"

  cmake_source_dir="$tmp_dir/cmake-consumer"
  cmake_build_dir="$tmp_dir/cmake-build"
  mkdir -p "$cmake_source_dir"
  cp "$consumer_source" "$cmake_source_dir/main.c"
  cat >"$cmake_source_dir/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.21)
project(lonejson_archive_consumer C)
find_package(lonejson CONFIG REQUIRED)
add_executable(lonejson_archive_consumer main.c)
target_link_libraries(lonejson_archive_consumer PRIVATE lonejson::lonejson)
EOF

  if [[ "${target_id#*apple-darwin}" != "$target_id" ]]; then
    darwin_deployment_target="$(target_darwin_deployment_target)"
    cmake_args=(
      -S "$cmake_source_dir"
      -B "$cmake_build_dir"
      -G Ninja
      -D "CMAKE_PREFIX_PATH=$package_root"
      -D "lonejson_DIR=$package_root/lib/cmake/lonejson"
      -D "CMAKE_TOOLCHAIN_FILE=$repo_root/cmake/toolchains/arm64-apple-darwin.cmake"
      -D "LONEJSON_MACOS_DEPLOYMENT_TARGET=$darwin_deployment_target"
      -D "CMAKE_OSX_DEPLOYMENT_TARGET=$darwin_deployment_target"
    )
  else
    cmake_system_name="$(target_cmake_system_name "$target_id")"
    cmake_args=(
      -S "$cmake_source_dir"
      -B "$cmake_build_dir"
      -G Ninja
      -D "CMAKE_PREFIX_PATH=$package_root"
      -D "lonejson_DIR=$package_root/lib/cmake/lonejson"
      -D "CMAKE_C_COMPILER=$CC"
      -D "CMAKE_SYSTEM_NAME=$cmake_system_name"
      -D CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
    )
  fi
  run_with_target_path "$target_id" cmake "${cmake_args[@]}"
  run_with_target_path "$target_id" cmake --build "$cmake_build_dir"

  rm -rf "$tmp_dir"
}

require_curl_symbol() {
  local archive=$1
  local target_id=$2
  local preset=$3
  local tmp_dir package_root shared_lib static_lib

  tmp_dir="$(mktemp -d)"
  package_root="$(extract_archive "$archive" "$tmp_dir")"

  if [[ "${target_id#*apple-darwin}" != "$target_id" ]]; then
    shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson*.dylib' | sort | head -n 1)"
  else
    shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson.so*' ! -type l | sort | head -n 1)"
  fi
  if [[ -z "$shared_lib" ]]; then
    printf 'missing packaged shared library in %s\n' "$archive" >&2
    exit 1
  fi
  static_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson.a' | sort | head -n 1)"
  if [[ -z "$static_lib" ]]; then
    printf 'missing packaged static library in %s\n' "$archive" >&2
    exit 1
  fi

  "$repo_root/scripts/check_curl_abi_symbols.sh" \
    "$repo_root" \
    "$build_root/$preset" \
    "$target_id" \
    "$shared_lib" \
    "$static_lib" \
    "$archive"

  rm -rf "$tmp_dir"
}

require_jwt_symbol() {
  local archive=$1
  local target_id=$2
  local preset=$3
  local tmp_dir package_root shared_lib static_lib

  tmp_dir="$(mktemp -d)"
  package_root="$(extract_archive "$archive" "$tmp_dir")"

  if [[ "${target_id#*apple-darwin}" != "$target_id" ]]; then
    shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson*.dylib' | sort | head -n 1)"
  else
    shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson.so*' ! -type l | sort | head -n 1)"
  fi
  if [[ -z "$shared_lib" ]]; then
    printf 'missing packaged shared library in %s\n' "$archive" >&2
    exit 1
  fi
  static_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson.a' | sort | head -n 1)"
  if [[ -z "$static_lib" ]]; then
    printf 'missing packaged static library in %s\n' "$archive" >&2
    exit 1
  fi

  "$repo_root/scripts/check_jwt_abi_symbols.sh" \
    "$repo_root" \
    "$build_root/$preset" \
    "$target_id" \
    "$shared_lib" \
    "$static_lib" \
    "$archive"

  rm -rf "$tmp_dir"
}

require_command cmake
require_command pkg-config
require_command tar

verified=0
while read -r _hash artifact; do
  target_id="$(archive_target_id "$artifact")"
  [[ -n "$target_id" ]] || continue
  archive="$dist_dir/$artifact"
  if [[ ! -f "$archive" ]]; then
    printf 'checksum-listed archive missing under dist: %s\n' "$artifact" >&2
    exit 1
  fi
  preset="$(target_preset "$target_id")"
  require_archive_contract "$archive" "$target_id" "$preset"
  require_curl_symbol "$archive" "$target_id" "$preset"
  require_jwt_symbol "$archive" "$target_id" "$preset"
  require_archive_consumer_metadata "$archive" "$target_id" "$preset"
  verified=$((verified + 1))
done <"$checksums"

printf 'release archive verification passed: %s (%d binary SDK archive(s))\n' \
  "$checksums" "$verified"
