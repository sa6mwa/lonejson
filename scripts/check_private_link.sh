#!/usr/bin/env bash
set -euo pipefail
cc=${1:?usage: check_private_link.sh CC SYSTEM LIBRARY [TARGET_FLAGS]}
system=${2:?}
library=${3:?}
read -r -a flags <<<"${4:-}"
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
mkdir -p "$root/build"
workspace=$(mktemp -d "$root/build/private-link.XXXXXX")
trap 'rm -rf "$workspace"' EXIT
cat >"$workspace/consumer.c" <<'EOF'
/* This implementation helper has no public declaration or linkage. */
extern void lonejson__default_parse_options(void);
int main(void) { lonejson__default_parse_options(); return 0; }
EOF
link_flags=(-Wl,-fatal_warnings)
[[ "$system" == Darwin ]] || link_flags=(-Wl,--fatal-warnings -Wl,--allow-shlib-undefined)
if "$cc" "${flags[@]}" -Wall -Wextra -Werror "$workspace/consumer.c" \
    "$library" "${link_flags[@]}" -o "$workspace/consumer" >"$workspace/link.log" 2>&1; then
  printf 'private implementation helper linked through %s\n' "$library" >&2
  exit 1
fi
grep -F lonejson__default_parse_options "$workspace/link.log" >/dev/null || {
  cat "$workspace/link.log" >&2
  exit 1
}
