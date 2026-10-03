#!/usr/bin/env bash
set -euo pipefail
root=$1
nm=$2
system=$3
library=$4
cc=$5
mkdir -p "$root/build"
workspace=$(mktemp -d "$root/build/test-exports.XXXXXX")
trap 'rm -rf "$workspace"' EXIT
bash "$root/scripts/check_library_exports.sh" "$nm" "$system" "$library" "${7:?}"
bash "$root/scripts/check_private_link.sh" "$cc" "$system" "$library" "${6:-}"
while read -r symbol; do
  grep -w "$symbol" "$root/include/lonejson.h" >/dev/null
done <"$root/cmake/lonejson.exports"
cat "$root/cmake/lonejson_base.exports" "$root/cmake/lonejson_curl.exports" \
  "$root/cmake/lonejson_jwt.exports" "$root/cmake/lonejson_oidc.exports" \
  "$root/cmake/lonejson_curl_oidc.exports" | LC_ALL=C sort -u >"$workspace/all-public"
cmp "$workspace/all-public" "$root/cmake/lonejson.exports"
printf '%s\n' lonejson_allowed >"$workspace/allowlist"
cat >"$workspace/nm" <<'EOF'
#!/usr/bin/env bash
case "$*" in
  *--undefined-only*|*-gu*)
    [[ "${TEST_EXPORT_CASE:-}" != private ]] || printf 'lonejson__private U\n'
    ;;
  *)
    printf '%s T 0 1\n' "${TEST_EXPORT_SYMBOL:-lonejson_allowed}"
    [[ "${TEST_EXPORT_CASE:-}" != leak ]] || printf 'dependency_private T 1 1\n'
    ;;
esac
exit 0
EOF
chmod +x "$workspace/nm"
check=(bash "$root/scripts/check_library_exports.sh" "$workspace/nm" Linux "$library" "$workspace/allowlist")
"${check[@]}"
for failure in leak private missing; do
  symbol=lonejson_allowed
  [[ "$failure" != missing ]] || symbol=wrong_symbol
  if TEST_EXPORT_CASE="$failure" TEST_EXPORT_SYMBOL="$symbol" "${check[@]}" >"$workspace/$failure.log" 2>&1; then
    printf 'export checker accepted %s\n' "$failure" >&2; exit 1
  fi
done
TEST_EXPORT_SYMBOL=_lonejson_allowed bash "$root/scripts/check_library_exports.sh" \
  "$workspace/nm" Darwin "$library" "$workspace/allowlist"
printf '%s\n' 'Exact exports, private imports, Darwin names, and private linking passed.'
