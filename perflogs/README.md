# Performance Logs

Generated benchmark artifacts live here.

The standalone Lua benchmark harness writes its artifacts under
`perflogs/lua/`. That harness is intentionally separate because it depends on
Lua/LuaRocks and compares Lua lanes only against lonejson's C sibling lanes,
not an external C comparator.

Files:

* `latest.json` stores the most recent benchmark run as one JSON object and is committed so `bench-compare` works from the repo checkout.
* `history.jsonl` appends every benchmark run as compact JSONL.
* `baseline.json` stores a frozen run copied from the last history entry and is committed as the comparison baseline.
* `runs/<timestamp_epoch_ns>.json` stores archived benchmark reports.

The benchmark tool reads and writes these files using `lonejson` itself.

The C benchmark harness measures lonejson only. Earlier releases carried YAJL
comparison lanes, but those external comparator lanes are gone; the supported
workflow now is "measure lonejson, compare to the frozen lonejson baseline".

Methodology:

* Each benchmark lane records the median of `5` samples.
* Each sample repeats the lane until at least `100 ms` of wall time has elapsed.
* The C benchmark target compiles lonejson inline from the packaged single-header artifact (`LONEJSON_IMPLEMENTATION`) so the frozen C baseline stays comparable across single-header-focused releases.
* Fixed parse and stream lanes are reported in two modes:
  * default lanes use lonejson's normal `clear_destination=1` behavior
  * `*_prepared/*` lanes use `clear_destination=0` with caller-prepared fixed destinations
* Many lonejson lanes are schema-guided benchmarks:
  * lonejson parses directly into a known mapped layout instead of building a generic DOM
  * that removes generic key/value allocation and lookup work when the document shape is known up front
  * comparisons against generic decoders should therefore be read as "schema-guided mapped decode" versus "generic decode", not as proof that all parsing modes are equivalent
* The wide multilingual document lanes exist to make that schema-guided advantage visible on a less toy-like shape than the original single-field UTF-8 fixtures.
* `bench-compare` labels deltas under `3%` as `noise`, deltas under `5%` as `small`, and larger deltas as `material`.
* `bench-gate` is the hard check:
  * it fails if the baseline schema version differs from the latest run
  * it fails if the benchmark configuration differs from the baseline
  * it fails if the baseline is missing any current benchmark lanes
  * it fails on any material negative throughput regression
* Freeze a new `baseline.json` whenever the benchmark schema, benchmark case set, or measurement method changes.
