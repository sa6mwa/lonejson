#!/usr/bin/env bash

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
exec "${repo_root}/scripts/run_release_matrix.sh" "$@"
