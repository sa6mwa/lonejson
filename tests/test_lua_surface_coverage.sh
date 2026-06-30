#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
module_file="$repo_root/src/lua/lonejson_lua_module.inc.h"
test_files=(
  "$repo_root/tests/test_lua.lua"
  "$repo_root/tests/test_lua_fuzz.lua"
)

python3 - "$module_file" "${test_files[@]}" <<'PY'
import pathlib
import re
import sys

module_path = pathlib.Path(sys.argv[1])
test_paths = [pathlib.Path(path) for path in sys.argv[2:]]
module = module_path.read_text(encoding="utf-8")
tests = "\n".join(path.read_text(encoding="utf-8") for path in test_paths)
exports = []
missing = []
internal_core_only = {"monotonic_ns"}

for match in re.finditer(r'\{"([A-Za-z0-9_]+)",\s*ljlua_', module):
    name = match.group(1)
    if name not in exports:
        exports.append(name)

for name in exports:
    if name.startswith("_test_") or name in internal_core_only:
        continue
    if re.search(r'(?<![A-Za-z0-9_])' + re.escape(name) + r'(?![A-Za-z0-9_])',
                 tests) is None:
        missing.append(name)

if missing:
    print("Lua public C binding exports missing test/fuzz coverage:")
    for name in missing:
        print(f"  {name}")
    raise SystemExit(1)
PY
