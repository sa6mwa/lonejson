#!/usr/bin/env bash

set -eu

if [ "$#" -ne 6 ]; then
  printf 'usage: %s CC CFLAGS LIBFLAG OBJ_EXTENSION LIB_EXTENSION LUA_INCDIR\n' "$0" >&2
  exit 1
fi

cc="$1"
cflags="$2"
libflag="$3"
obj_ext="$4"
lib_ext="$5"
lua_incdir="$6"

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
build_root="${repo_root}/.luarocks-build"
module_dir="${build_root}/lonejson"
object_path="${build_root}/lonejson_lua.${obj_ext}"
module_path="${module_dir}/core.${lib_ext}"
probe_dir="${build_root}/probe"
probe_c="${probe_dir}/curl_probe.c"
probe_bin="${probe_dir}/curl_probe"

if [ -z "${cc}" ]; then
  printf 'compiler command is empty\n' >&2
  exit 1
fi

run_cc() {
  if [ -x "${cc}" ]; then
    "${cc}" "$@"
    return "$?"
  fi
  CC_LONEJSON="${cc}" sh -c '
    eval "set -- ${CC_LONEJSON} \"\$@\""
    exec "$@"
  ' sh "$@"
}

mkdir -p "${module_dir}" "${probe_dir}"
rm -f "${object_path}" "${module_path}" "${probe_bin}"

common_cflags="${cflags} -I${repo_root}/include -I${lua_incdir}"
linkflags="${LDFLAGS:-}"
if [ "$(uname -s)" = "Linux" ]; then
  linkflags="${linkflags} -Wl,--allow-shlib-undefined"
fi

curl_cflags=""
curl_libs=""
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libcurl; then
  curl_cflags="$(pkg-config --cflags libcurl)"
  curl_libs="$(pkg-config --libs libcurl)"
elif command -v curl-config >/dev/null 2>&1; then
  curl_cflags="$(curl-config --cflags)"
  curl_libs="$(curl-config --libs)"
else
  curl_libs="-lcurl"
fi

cat >"${probe_c}" <<'EOF'
#define LONEJSON_WITH_CURL 1
#include "lonejson.h"
int main(void) {
  curl_off_t size = 0;
  return (int)size;
}
EOF

use_curl=0
if [ -z "${LONEJSON_LUA_DISABLE_CURL:-}" ]; then
  if run_cc ${common_cflags} ${curl_cflags} "${probe_c}" -o "${probe_bin}" ${curl_libs} >/dev/null 2>&1; then
    use_curl=1
  elif [ -n "${LONEJSON_LUA_FORCE_CURL:-}" ]; then
    printf 'curl probe failed while LONEJSON_LUA_FORCE_CURL is set\n' >&2
    exit 1
  fi
fi

cppflags=""
libs=""
if [ "${use_curl}" -eq 1 ]; then
  cppflags="-DLONEJSON_WITH_CURL ${curl_cflags}"
  libs="${curl_libs}"
  printf 'Lua rock build: enabling curl support\n'
else
  printf 'Lua rock build: building without curl support\n'
fi

run_cc ${common_cflags} ${cppflags} -c "${repo_root}/src/lua/lonejson_lua.c" -o "${object_path}"
run_cc ${libflag} -o "${module_path}" "${object_path}" ${linkflags} ${libs}
