#!/usr/bin/env bash
set -euo pipefail

# Rationale: the TSan gate must probe the pinned compiler selected by normal
# debug and release routes, rather than infer support from host Clang.

repo_root=$1
result="$(bash "$repo_root/scripts/check_bootlin_tsan_support.sh")"
[[ "$result" == "1" ]]
