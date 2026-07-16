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
    x86_64-linux-gnu) printf '%s\n' x86_64-linux-gnu-release ;;
    x86_64-linux-musl) printf '%s\n' x86_64-linux-musl-release ;;
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

target_toolchain_file() {
  local target_id=$1
  case "$target_id" in
    x86_64-linux-gnu) printf '%s\n' "$repo_root/cmake/toolchains/linux-x86_64-gnu.cmake" ;;
    x86_64-linux-musl) printf '%s\n' "$repo_root/cmake/toolchains/linux-x86_64-musl.cmake" ;;
    aarch64-linux-gnu) printf '%s\n' "$repo_root/cmake/toolchains/linux-aarch64-gnu.cmake" ;;
    aarch64-linux-musl) printf '%s\n' "$repo_root/cmake/toolchains/linux-aarch64-musl.cmake" ;;
    armhf-linux-gnu) printf '%s\n' "$repo_root/cmake/toolchains/linux-armhf-gnu.cmake" ;;
    armhf-linux-musl) printf '%s\n' "$repo_root/cmake/toolchains/linux-armhf-musl.cmake" ;;
    *)
      printf 'unknown Linux target id: %s\n' "$target_id" >&2
      exit 1
      ;;
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
      printf '%s\n' "--ld-path=$LINKER"
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

target_cache_value() {
  local preset=$1
  local name=$2
  local cache_file="$build_root/$preset/CMakeCache.txt"

  if [[ -f "$cache_file" ]]; then
    sed -n "s/^${name}:[^=]*=//p" "$cache_file" | tail -n 1
  fi
}

target_dependency_root() {
  local preset=$1

  target_cache_value "$preset" LONEJSON_C_PKT_SYSTEMS_ROOT
}

target_pkg_config_path() {
  local package_root=$1
  local preset=$2
  local dependency_root

  dependency_root="$(target_dependency_root "$preset")"
  printf '%s' "$package_root/lib/pkgconfig"
  if [[ -n "$dependency_root" && -d "$dependency_root/lib/pkgconfig" ]]; then
    printf ':%s' "$dependency_root/lib/pkgconfig"
  fi
  if [[ -n "${PKG_CONFIG_PATH:-}" ]]; then
    printf ':%s' "$PKG_CONFIG_PATH"
  fi
  printf '\n'
}

target_cmake_prefix_path() {
  local package_root=$1
  local preset=$2
  local dependency_root

  dependency_root="$(target_dependency_root "$preset")"
  printf '%s' "$package_root"
  if [[ -n "$dependency_root" ]]; then
    printf ';%s' "$dependency_root"
  fi
  printf '\n'
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
  local forbidden_metadata_pattern

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
      "lib/pkgconfig/lonejson-curl.pc" | \
      "lib/pkgconfig/lonejson-jwt.pc" | \
      "lib/pkgconfig/lonejson-oidc.pc" | \
      "lib/pkgconfig/lonejson-openssl.pc" | \
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
  forbidden_metadata_pattern='(^|[^[:alnum:]_])(CURL::libcurl|OpenSSL::|openssl|OpenSSL|libcurl|libssl|libcrypto|-lcurl|-lssl|-lcrypto|crypto)([^[:alnum:]_]|$)'
  require_file "$package_root/lib/pkgconfig/lonejson-curl.pc"
  require_file "$package_root/lib/pkgconfig/lonejson-jwt.pc"
  require_file "$package_root/lib/pkgconfig/lonejson-oidc.pc"
  require_file "$package_root/lib/pkgconfig/lonejson-openssl.pc"
  if grep -RE 'c\.pkt\.systems|\.cache/|\.deps/|/home/|/build/' \
      "$package_root/lib/pkgconfig/lonejson.pc" \
      "$package_root/lib/cmake/lonejson" >/dev/null; then
    printf 'forbidden dependency or path leak in release metadata for %s\n' "$archive" >&2
    exit 1
  fi
  if grep -Eq "$forbidden_metadata_pattern" "$package_root/lib/pkgconfig/lonejson.pc"; then
    printf 'unexpected third-party pkg-config dependency in core lonejson SDK: %s\n' "$archive" >&2
    exit 1
  fi
  if awk '
      /add_library\(lonejson::lonejson SHARED IMPORTED\)/ { in_core = 1 }
      /add_library\(lonejson::lonejson_static STATIC IMPORTED\)/ { in_core = 1 }
      in_core { print }
      in_core && /^endif\(\)/ { in_core = 0 }
    ' "$package_root/lib/cmake/lonejson/lonejsonConfig.cmake" |
      grep -Eq "$forbidden_metadata_pattern"; then
    printf 'unexpected third-party CMake dependency in core lonejson SDK: %s\n' "$archive" >&2
    exit 1
  fi
  if grep -E '\.cache/|\.deps/|/home/|/build/|file://' "$dependency_manifest" >/dev/null; then
    printf 'forbidden path leak in dependency manifest for %s\n' "$archive" >&2
    exit 1
  fi
  for required_metadata in \
      '"schema": "pkt.systems.dependencies.v1"' \
      '"name": "c.pkt.systems"' \
      '"version": "0.8.0"' \
      "\"target_id\": \"$target_id\"" \
      '"source_url": "https://github.com/sa6mwa/c.pkt.systems/releases/download/v0.8.0/c.pkt.systems-0.8.0-' \
      '"sha256": "' \
      '"bundled": false' \
      '"external": false' \
      '"role": "release-sdk-build-input"' \
      '"curl"'; do
    if ! grep -F "$required_metadata" "$dependency_manifest" >/dev/null; then
      printf 'missing dependency manifest metadata in %s: %s\n' "$archive" "$required_metadata" >&2
      exit 1
    fi
  done
  if grep -F '"openssl"' "$dependency_manifest" >/dev/null; then
    printf 'unexpected OpenSSL build input in core lonejson SDK metadata: %s\n' "$archive" >&2
    exit 1
  fi

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
      *libcurl* | *libssl* | *libcrypto* | *OpenSSL* | *c.pkt.systems* | *".cache/"* | *".deps/"* | *"$repo_root"* | *"/home/"* | *"/build/"*)
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
      *libcurl* | *libssl* | *libcrypto* | *OpenSSL* | *c.pkt.systems* | *".cache/"* | *".deps/"* | *"$repo_root"* | *"/home/"* | *"/build/"*)
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
  local pkg_config_static_flags raw_link_flags cmake_source_dir cmake_build_dir
  local adapter_source adapter_cmake_source_dir adapter_cmake_build_dir
  local adapter_pkg_config_flags adapter_c89_flags cmake_prefix_path pkg_config_path
  local adapter_dependency_root darwin_deployment_target cmake_system_name

  load_target_tools "$preset" "$target_id"

  tmp_dir="$(mktemp -d)"
  package_root="$(extract_archive "$archive" "$tmp_dir")"
  pkg_config_path="$(target_pkg_config_path "$package_root" "$preset")"
  cmake_prefix_path="$(target_cmake_prefix_path "$package_root" "$preset")"
  adapter_dependency_root="$(target_dependency_root "$preset")"

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
  pkg_config_flags="$(PKG_CONFIG_PATH="$pkg_config_path" pkg-config --cflags --libs lonejson)"
  pkg_config_static_flags="$(PKG_CONFIG_PATH="$pkg_config_path" pkg-config --cflags --static --libs lonejson)"
  raw_compile_flags="$(target_raw_compile_flags "$target_id")"
  raw_link_flags="$(target_raw_link_flags "$target_id")"
  # shellcheck disable=SC2086
  run_with_target_path "$target_id" "$CC" "$consumer_source" $TARGET_CFLAGS $raw_compile_flags $pkg_config_flags $raw_link_flags -o "$tmp_dir/pkg-config-consumer"
  # shellcheck disable=SC2086
  run_with_target_path "$target_id" "$CC" "$consumer_source" $TARGET_CFLAGS $raw_compile_flags $pkg_config_static_flags $raw_link_flags -o "$tmp_dir/pkg-config-static-consumer"

  cmake_source_dir="$tmp_dir/cmake-consumer"
  cmake_build_dir="$tmp_dir/cmake-build"
  mkdir -p "$cmake_source_dir"
  cp "$consumer_source" "$cmake_source_dir/main.c"
  cat >"$cmake_source_dir/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.21)
project(lonejson_archive_consumer C)
find_package(lonejson CONFIG REQUIRED)
add_executable(lonejson_archive_consumer_shared main.c)
target_link_libraries(lonejson_archive_consumer_shared PRIVATE lonejson::lonejson)
add_executable(lonejson_archive_consumer_static main.c)
target_link_libraries(lonejson_archive_consumer_static PRIVATE lonejson::lonejson_static)
EOF

  if [[ "${target_id#*apple-darwin}" != "$target_id" ]]; then
    darwin_deployment_target="$(target_darwin_deployment_target)"
    cmake_args=(
      -S "$cmake_source_dir"
      -B "$cmake_build_dir"
      -G Ninja
      -D "CMAKE_PREFIX_PATH=$cmake_prefix_path"
      -D "lonejson_DIR=$package_root/lib/cmake/lonejson"
      -D "CMAKE_TOOLCHAIN_FILE=$repo_root/cmake/toolchains/arm64-apple-darwin.cmake"
      -D "LONEJSON_MACOS_DEPLOYMENT_TARGET=$darwin_deployment_target"
      -D "CMAKE_OSX_DEPLOYMENT_TARGET=$darwin_deployment_target"
      -D "LONEJSON_C_PKT_SYSTEMS_ROOT=$adapter_dependency_root"
    )
  else
    cmake_args=(
      -S "$cmake_source_dir"
      -B "$cmake_build_dir"
      -G Ninja
      -D "CMAKE_PREFIX_PATH=$cmake_prefix_path"
      -D "lonejson_DIR=$package_root/lib/cmake/lonejson"
      -D "CMAKE_TOOLCHAIN_FILE=$(target_toolchain_file "$target_id")"
    )
  fi
  run_with_target_path "$target_id" cmake "${cmake_args[@]}"
  run_with_target_path "$target_id" cmake --build "$cmake_build_dir"

  if [[ -z "$adapter_dependency_root" ]]; then
    rm -rf "$tmp_dir"
    return
  fi

  adapter_source="$tmp_dir/adapter-consumer.c"
  cat >"$adapter_source" <<'EOF'
#include <lonejson.h>
#include <stddef.h>

#ifndef LONEJSON_WITH_CURL
#error "lonejson curl adapter target did not define LONEJSON_WITH_CURL"
#endif
#ifndef LONEJSON_WITH_JWT
#error "lonejson jwt/openssl adapter target did not define LONEJSON_WITH_JWT"
#endif
#ifndef LONEJSON_WITH_OIDC
#error "lonejson oidc adapter target did not define LONEJSON_WITH_OIDC"
#endif
#ifndef LONEJSON_WITH_OPENSSL
#error "lonejson openssl adapter target did not define LONEJSON_WITH_OPENSSL"
#endif

int main(void) {
    lonejson_error error;
    lonejson_auth_provider provider;
    lonejson_curl_parse curl_parse;
    lonejson_oidc_pkce pkce;

    lonejson_error_init(&error);
    lonejson_oidc_pkce_init(&pkce);
    (void)sizeof(curl_parse);
    if (lonejson_auth_provider_init_openssl(&provider, NULL, &error) != LONEJSON_STATUS_OK) {
        lonejson_oidc_pkce_cleanup(&pkce);
        return 1;
    }
    lonejson_oidc_pkce_cleanup(&pkce);
    return 0;
}
EOF

  adapter_pkg_config_flags="$(PKG_CONFIG_PATH="$pkg_config_path" pkg-config --cflags --libs lonejson-curl lonejson-oidc lonejson-openssl)"
  adapter_c89_flags="-std=c89 -Wall -Wextra -Werror -Werror=implicit-function-declaration"
  # shellcheck disable=SC2086
  run_with_target_path "$target_id" "$CC" "$adapter_source" $TARGET_CFLAGS $raw_compile_flags $adapter_c89_flags $adapter_pkg_config_flags $raw_link_flags -o "$tmp_dir/pkg-config-adapter-consumer"

  adapter_cmake_source_dir="$tmp_dir/cmake-adapter-consumer"
  adapter_cmake_build_dir="$tmp_dir/cmake-adapter-build"
  mkdir -p "$adapter_cmake_source_dir"
  cp "$adapter_source" "$adapter_cmake_source_dir/main.c"
  cat >"$adapter_cmake_source_dir/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.21)
project(lonejson_archive_adapter_consumer C)
find_package(lonejson CONFIG REQUIRED COMPONENTS curl oidc openssl)
add_executable(lonejson_archive_adapter_consumer main.c)
target_compile_options(lonejson_archive_adapter_consumer PRIVATE
  -std=c89
  -Wall
  -Wextra
  -Werror
  -Werror=implicit-function-declaration)
target_link_libraries(lonejson_archive_adapter_consumer PRIVATE
  lonejson::lonejson
  lonejson::curl
  lonejson::oidc
  lonejson::openssl)
EOF

  if [[ "${target_id#*apple-darwin}" != "$target_id" ]]; then
    cmake_args=(
      -S "$adapter_cmake_source_dir"
      -B "$adapter_cmake_build_dir"
      -G Ninja
      -D "CMAKE_PREFIX_PATH=$cmake_prefix_path"
      -D "lonejson_DIR=$package_root/lib/cmake/lonejson"
      -D "CMAKE_TOOLCHAIN_FILE=$repo_root/cmake/toolchains/arm64-apple-darwin.cmake"
      -D "LONEJSON_MACOS_DEPLOYMENT_TARGET=$darwin_deployment_target"
      -D "CMAKE_OSX_DEPLOYMENT_TARGET=$darwin_deployment_target"
      -D "LONEJSON_C_PKT_SYSTEMS_ROOT=$adapter_dependency_root"
    )
  else
    cmake_args=(
      -S "$adapter_cmake_source_dir"
      -B "$adapter_cmake_build_dir"
      -G Ninja
      -D "CMAKE_PREFIX_PATH=$cmake_prefix_path"
      -D "lonejson_DIR=$package_root/lib/cmake/lonejson"
      -D "CMAKE_TOOLCHAIN_FILE=$(target_toolchain_file "$target_id")"
    )
  fi
  run_with_target_path "$target_id" cmake "${cmake_args[@]}"
  run_with_target_path "$target_id" cmake --build "$adapter_cmake_build_dir"

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

if [[ "$verified" -eq 0 ]]; then
  printf 'release archive verification failed: no binary SDK archives listed in %s\n' \
    "$checksums" >&2
  exit 1
fi

printf 'release archive verification passed: %s (%d binary SDK archive(s))\n' \
  "$checksums" "$verified"
