#!/usr/bin/env bash
set -euo pipefail
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
cmake "-DBUILD_DIR=${1:?build directory required}" -P "$repo_root/tests/check_ctest_metadata.cmake"
