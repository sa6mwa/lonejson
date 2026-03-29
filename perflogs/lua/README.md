# Lua Performance Logs

Generated Lua benchmark artifacts live here.

Files:

* `latest.json` stores the most recent Lua benchmark run and is intended to be committed so `make lua-bench-compare` works from the repo checkout.
* `history.jsonl` appends every Lua benchmark run.
* `baseline.json` stores a frozen Lua benchmark baseline and is intended to be committed.
* `runs/<timestamp_epoch_ns>.json` stores archived Lua benchmark reports.

The Lua benchmark harness is separate from the C benchmark harness. It compares Lua lanes only to their lonejson C siblings from `perflogs/latest.json`; it does not benchmark YAJL.

`lonejson`'s fast lanes are schema-guided: the binding compiles a schema into a C-backed mapped layout and parses directly into that layout or a reusable record, instead of decoding into a generic DOM first. That is why some lonejson Lua lanes can beat generic Lua decoders on known shapes. The bounded-memory third-party comparison lanes should therefore be read as "schema-guided mapped decode" versus "generic table decode".

If `lua-cjson` is available in the active Lua environment, the harness also adds bounded-memory decode lanes for it. Those third-party cases are intentionally limited to one-shot decode scenarios and line-by-line JSONL decode from `lockdbench.jsonl`; they are not used for lonejson-style streaming or large generated fixture runs.

The multilingual `*_wide/*` lanes are included specifically to show that this advantage is not limited to single-field objects.
