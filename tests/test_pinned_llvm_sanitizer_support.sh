#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
[[ "$(bash "$repo_root/scripts/check_pinned_llvm_sanitizer_support.sh" memory)" == 1 ]]
