#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
target=${1:?usage: require_build_workspace.sh ABSOLUTE_WORKSPACE}
while [[ "$target" == */ ]]; do target=${target%/}; done
fail() {
  printf 'refusing unsafe workspace outside repository build/: %s\n' "$target" >&2
  exit 1
}
[[ "$target" == "$root/build/"* ]] || fail
[[ "$target" != */../* && "$target" != */.. && "$target" != */./* ]] || fail
ancestor=$target
while [[ ! -e "$ancestor" ]]; do ancestor=$(dirname -- "$ancestor"); done
[[ -d "$ancestor" ]] || fail
suffix=${target#"$ancestor"}
ancestor=$(CDPATH= cd -- "$ancestor" && pwd -P)
# Reattach missing components before checking ownership: an existing alias may
# resolve to build/ itself, which must never be a disposable workspace.
case "$ancestor$suffix" in
  "$root/build/"*) ;;
  *) fail ;;
esac
