#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cat >"$tmp_dir/clang" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
for arg in "$@"; do
  if [ "$arg" = "-c" ]; then
    while [ $# -gt 0 ]; do
      if [ "$1" = "-o" ]; then
        shift
        : >"$1"
        exit 0
      fi
      shift
    done
    exit 0
  fi
done
exit 1
EOF
chmod +x "$tmp_dir/clang"

result_thread=$(PATH="$tmp_dir:$PATH" \
  bash "$repo_root/scripts/check_clang_sanitizer_support.sh" thread)
[ "$result_thread" = "0" ]

result_memory=$(PATH="$tmp_dir:$PATH" \
  bash "$repo_root/scripts/check_clang_sanitizer_support.sh" memory)
[ "$result_memory" = "0" ]
