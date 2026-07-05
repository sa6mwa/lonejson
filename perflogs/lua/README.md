# Lua Performance Logs

Generated Lua benchmark artifacts used to live here. Current benchmark output
is host-scoped under `perflogs/hosts/<host-id>/lua/`, where `<host-id>` is the
first field of `uname -n | md5sum`.

Files:

* `perflogs/hosts/<host-id>/lua/latest.json` stores the most recent Lua benchmark run and is ignored because it is local run output.
* `perflogs/hosts/<host-id>/lua/history.jsonl` appends every Lua benchmark run.
* `perflogs/hosts/<host-id>/lua/baseline.json` stores a frozen Lua benchmark baseline and is intended to be committed.
* `perflogs/hosts/<host-id>/lua/runs/<timestamp_epoch_ns>.json` stores archived Lua benchmark reports.

The Lua benchmark harness is separate from the C benchmark harness. It compares Lua lanes only to their lonejson C siblings from the same host archive's C `latest.json`; it does not benchmark YAJL.

`lonejson`'s fast lanes are schema-guided: the binding compiles a schema into a C-backed mapped layout and parses directly into that layout or a reusable record, instead of decoding into a generic DOM first. That is why some lonejson Lua lanes can beat generic Lua decoders on known shapes. The bounded-memory third-party comparison lanes should therefore be read as "schema-guided mapped decode" versus "generic table decode".

If `lua-cjson` is available in the active Lua environment, the harness also adds bounded-memory decode lanes for it. Those third-party cases are intentionally limited to one-shot decode scenarios and line-by-line JSONL decode from `workloadbench.jsonl`; they are not used for lonejson-style streaming or large generated fixture runs.

The multilingual `*_wide/*` lanes are included specifically to show that this advantage is not limited to single-field objects.

`lua-bench-gate` is the hard check for the Lua harness:

* it fails if the Lua benchmark schema version differs from the baseline
* it fails if the baseline is missing any current lonejson Lua benchmark lanes
* it fails on any negative throughput regression of `10%` or more
* it does not gate on `cjson` sibling/reference lanes
