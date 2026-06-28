#!/usr/bin/env bash
set -euo pipefail

build_dir=$1

if ! command -v python3 >/dev/null 2>&1; then
  printf 'SKIP: python3 unavailable for CTest metadata validation\n'
  exit 0
fi

json_path="$(mktemp)"
trap 'rm -f "$json_path"' EXIT

ctest --test-dir "$build_dir" --show-only=json-v1 >"$json_path"
python3 - "$json_path" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    data = json.load(handle)

missing = []
for test in data.get("tests", []):
    name = test.get("name", "<unnamed>")
    properties = {prop.get("name"): prop.get("value") for prop in test.get("properties", [])}
    if "TIMEOUT" not in properties:
        missing.append(f"{name}: missing TIMEOUT")
    labels = properties.get("LABELS")
    if not labels:
        missing.append(f"{name}: missing LABELS")

if missing:
    for line in missing:
        print(line, file=sys.stderr)
    sys.exit(1)
PY
