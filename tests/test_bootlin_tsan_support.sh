#!/usr/bin/env bash
set -euo pipefail

# Rationale: the TSan gate must resolve the native target at gate time and
# probe the pinned compiler selected by normal debug and release routes.

repo_root=$1
grep -F 'detect_native_bootlin_target.sh' "$repo_root/scripts/check_bootlin_tsan_support.sh" >/dev/null
grep -F 'cpkt-toolchains.sh" ensure "$native_target"' \
  "$repo_root/scripts/check_bootlin_tsan_support.sh" >/dev/null
result="$(bash "$repo_root/scripts/check_bootlin_tsan_support.sh")"
[[ "$result" == "1" ]]
