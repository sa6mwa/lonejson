#!/usr/bin/env bash
set -euo pipefail

# CMake, Make, and direct scripts must resolve the same Lua 5.5 executable,
# including hosts that expose it only as lua5.5.

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

fake_bin="$tmp_dir/bin"
mkdir -p "$fake_bin"
cat >"$fake_bin/lua" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' 'Lua 5.4.7' >&2
EOF
cat >"$fake_bin/lua5.5" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' 'Lua 5.5.0' >&2
EOF
chmod +x "$fake_bin/lua" "$fake_bin/lua5.5"

resolved=$(PATH="$fake_bin:$PATH" "$repo_root/scripts/resolve_lua55.sh")
[[ "$resolved" == "$fake_bin/lua5.5" ]]

cat >"$fake_bin/lua5.5" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' 'Lua 5.4.7' >&2
EOF
chmod +x "$fake_bin/lua5.5"
if PATH="$fake_bin:$PATH" "$repo_root/scripts/resolve_lua55.sh" \
    >"$tmp_dir/missing.out" 2>"$tmp_dir/missing.err"; then
  printf 'Lua resolver accepted a non-5.5 lua executable\n' >&2
  exit 1
fi
grep -F 'requires a Lua 5.5 executable' "$tmp_dir/missing.err" >/dev/null
