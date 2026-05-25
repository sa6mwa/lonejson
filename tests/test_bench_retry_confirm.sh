#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
lua_exec=${2:-lua}
luarocks_exec=${3:-luarocks}
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

make --no-print-directory -C "$repo_root" lua-rock \
  LUA="$lua_exec" LUAROCKS="$luarocks_exec" >/dev/null
eval "$("$luarocks_exec" path --tree "$repo_root/build/luarocks")"

write_run() {
  local path=$1
  shift
  printf '{"schema_version":1,"iterations":1,"parser_buffer_size":4096,"push_parser_buffer_size":4096,"reader_buffer_size":4096,"stream_buffer_size":4096,"results":[' >"$path"
  local first=1
  while [ "$#" -gt 0 ]; do
    local name=$1
    local mib=$2
    local mismatch=$3
    shift 3
    if [ "$first" -eq 0 ]; then
      printf ',' >>"$path"
    fi
    first=0
    printf '{"name":"%s","mib_per_sec":%s,"mismatch_count":%s}' \
      "$name" "$mib" "$mismatch" >>"$path"
  done
  printf ']}\n' >>"$path"
}

fake_bench="$tmp_dir/fake_bench.sh"
fake_log="$tmp_dir/fake_bench.log"
cat >"$fake_bench" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
log_path=${FAKE_BENCH_LOG:?}
mode=${FAKE_BENCH_MODE:?}
printf '%s\n' "$*" >>"$log_path"
if [ "$1" != "case" ]; then
  exit 2
fi
case_name=$2
if [ "$mode" = "pass-one" ]; then
  printf '%s stream 101.000 MiB/s 1.0 docs/s mismatch=0 c_sib=- ratio=-\n' "$case_name"
  exit 0
fi
if [ "$mode" = "fail-first" ]; then
  if [ "$case_name" = "case/a" ]; then
    printf '%s stream 90.000 MiB/s 1.0 docs/s mismatch=0 c_sib=- ratio=-\n' "$case_name"
  else
    printf '%s stream 150.000 MiB/s 1.0 docs/s mismatch=0 c_sib=- ratio=-\n' "$case_name"
  fi
  exit 0
fi
exit 3
EOF
chmod +x "$fake_bench"

baseline="$tmp_dir/baseline.json"
latest="$tmp_dir/latest.json"

write_run "$baseline" "case/a" 100 0 "case/b" 100 0
write_run "$latest" "case/a" 90 0 "case/b" 100 0
rm -f "$fake_log"
FAKE_BENCH_LOG="$fake_log" FAKE_BENCH_MODE="pass-one" \
  bash -lc "cd $(printf '%q' "$repo_root") && exec $(printf '%q' "$lua_exec") bench/lonejson_lua_bench.lua confirm-c $(printf '%q' "$fake_bench") $(printf '%q' "$baseline") $(printf '%q' "$latest") 1" >/dev/null
grep -qx 'case case/a 1' "$fake_log"
if grep -q 'case case/b 1' "$fake_log"; then
  printf 'confirm-c reran a non-failing case\n' >&2
  exit 1
fi

write_run "$baseline" "case/a" 100 0 "case/b" 100 0 "case/c" 100 0
write_run "$latest" "case/a" 90 0 "case/b" 80 0 "case/c" 100 0
rm -f "$fake_log"
if FAKE_BENCH_LOG="$fake_log" FAKE_BENCH_MODE="fail-first" \
  bash -lc "cd $(printf '%q' "$repo_root") && exec $(printf '%q' "$lua_exec") bench/lonejson_lua_bench.lua confirm-c $(printf '%q' "$fake_bench") $(printf '%q' "$baseline") $(printf '%q' "$latest") 1" >/dev/null 2>&1; then
  printf 'confirm-c succeeded despite a confirmed failure\n' >&2
  exit 1
fi
grep -qx 'case case/a 1' "$fake_log"
if grep -q 'case case/b 1' "$fake_log"; then
  printf 'confirm-c did not stop on the first confirmed failure\n' >&2
  exit 1
fi

write_run "$baseline" "case/a" 100 0
printf '{"schema_version":2,"iterations":1,"parser_buffer_size":4096,"push_parser_buffer_size":4096,"reader_buffer_size":4096,"stream_buffer_size":4096,"results":[{"name":"case/a","mib_per_sec":100,"mismatch_count":0}]}\n' >"$latest"
rm -f "$fake_log"
if FAKE_BENCH_LOG="$fake_log" FAKE_BENCH_MODE="pass-one" \
  bash -lc "cd $(printf '%q' "$repo_root") && exec $(printf '%q' "$lua_exec") bench/lonejson_lua_bench.lua confirm-c $(printf '%q' "$fake_bench") $(printf '%q' "$baseline") $(printf '%q' "$latest") 1" >/dev/null 2>&1; then
  printf 'confirm-c succeeded despite a schema mismatch\n' >&2
  exit 1
fi
if [ -f "$fake_log" ] && [ -s "$fake_log" ]; then
  printf 'confirm-c retried cases despite a schema mismatch\n' >&2
  exit 1
fi
