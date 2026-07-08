# Candidate Transform Implementation Contract

This document records the agreed implementation contract for completing
candidate transform support for liblql. It supersedes the exploratory notes in
`stash/` as the tracked design reference.

## Goal

The candidate transform API must let liblql remove its callback-source
candidate replay machinery while preserving public mutation/projection behavior.
LoneJSON owns JSON parsing, validation, candidate framing, logical candidate
flattening, writer structure, escaping, separators, output framing, explicit
spooling, replay, and source/sink error propagation.

The API is an extension of the existing candidate transform surface, not a new
`v2` function family. Public struct and enum changes may require an ABI bump.

## Implemented Surface

The implementation extends the existing transform API with:

- explicit `streaming`, `gated_spooled`, and `unsupported` transform modes;
- `LONEJSON_STATUS_UNSUPPORTED`;
- per-candidate transform metadata and aggregate stream/spool/spill/replay
  counters;
- per-call runtime spool class and maximum spooled candidate byte overrides;
- streaming keep behavior for large strings and numbers by default;
- opt-in complete old scalar materialization;
- same-executor gated replay through `lonejson_spooled`;
- object-member insertion hooks;
- structural projection paths with object-member and array-index segment kinds;
- grouped missing object-member synthesis and sparse array `null`
  placeholders;
- recursive root-array logical candidate framing.

Logical candidate offsets and byte sizes are source-relative for current
candidate stream framing, including recursive root-array leaf candidates. The
public value-event callback surface still reports paths, not per-value byte
offsets; if those hooks are added later, unknown values must remain explicit and
spool/replay-relative offsets must not be exposed as source offsets.

## Execution Modes

Candidate transform has explicit upfront execution modes:

- `streaming`: output can be safely committed as a logical candidate is parsed.
- `gated_spooled`: selector truth or output decisions may arrive too late, so
  the current logical candidate is retained until the decision is known.
- `unsupported`: the selected mode cannot produce the requested result for the
  encountered plan or JSON shape without violating its documented contract.

Mode selection happens before output can leak to the caller sink. A logical
candidate must not start in streaming mode and later upgrade to gated spooling
after output has already been emitted. liblql may conservatively select
`gated_spooled` for any plan where source field order can make selector truth
arrive after output-relevant content.

## Streaming Mode

Streaming mode is the fast path for stream-commit-safe plans:

- match-all mutation/projection;
- known-path large field keep/drop/replace;
- early-decidable selectors where truth is known before output-relevant earlier
  content must be committed;
- object member insertion that can be performed at object boundaries;
- projection synthesis whose required output can be emitted structurally as the
  source is parsed.

Streaming mode must not materialize the full source, candidate, match set, or
output. It may retain parser/writer stacks, current keys, array indices,
projection plan state, and opt-in current-scalar scratch.

Large strings and base64-like string fields are streaming by default. Kept
strings are emitted through `lonejson_writer_string_begin()`,
`lonejson_writer_string_chunk()`, and `lonejson_writer_string_end()`. Dropped or
replaced large fields are consumed and validated without retaining the old
payload. Complete old scalar views are opt-in and current-scalar scoped.

## Gated Spooled Mode

Gated spooled mode handles information-theoretically non-stream-safe plans, such
as `matches_only=true` where selector truth arrives after earlier semantic
content that may need to be emitted for matched candidates.

In gated spooled mode, lonejson:

- parses and validates the original source once;
- writes the logical candidate's compact semantic JSON into a
  `lonejson_spooled` handle;
- keeps bytes in memory up to the selected spool policy watermark;
- spills to disk after that watermark;
- enforces a configured maximum logical candidate byte size;
- after selector truth is known, discards unmatched candidates or replays matched
  spooled candidates through the same transform executor;
- reports the replay and spool behavior in result metadata and counters.

Replay is explicitly named replay: it is a second parse of a lonejson-owned
compact semantic candidate. It is acceptable because it is bounded,
policy-controlled, visible, and owned by lonejson rather than duplicated in
liblql. Public replay coordinates remain original source-relative, never
spool-relative.

Gated spooling is per logical candidate, not only per physical top-level
candidate. Recursive root-array flattening gives each emitted nested item its
own match/projection/mutation semantics, so each non-stream-safe logical
candidate must be spooled and replayed independently.

Spooling uses existing runtime spool classes with narrow per-call selection or
overrides. Do not create a second spool configuration system. The default model
is:

- `spool_default` for normal candidates;
- `spool_blob` for blob-heavy plans when selected by liblql;
- `spool_large_text` for large text-heavy plans when selected by liblql.

## Public Status

Add `LONEJSON_STATUS_UNSUPPORTED` with this narrow meaning:

> The requested operation is valid, but the selected transform mode cannot
> produce the requested result for the encountered plan or JSON shape without
> violating its documented streaming/spooling contract.

This is not `TYPE_MISMATCH`, not malformed JSON, and not invalid caller
arguments.

## Logical Candidate Metadata

Logical candidate metadata must be stable:

- stable physical and logical candidate index policy;
- source-relative offsets when available;
- no offsets relative to temporary replay, spool, or nested parser origins;
- byte size known by logical candidate end when lonejson can expose it;
- explicit unknown values when exact offsets or sizes are not yet available.

Design parser/value-event offset hooks now so recursive root-array flattening
can eventually expose exact source-relative logical begin/end offsets and byte
sizes. If exact nested logical offsets cannot be implemented immediately, the API
must represent unknown values rather than fabricating spool-relative metadata.

## Result Metadata And Counters

The transform API must expose concrete metadata sufficient for liblql to report
and test mode behavior:

- execution mode used for each logical candidate;
- physical candidate index;
- logical candidate index;
- logical candidate source-relative offset and byte size when known;
- whether gated spooling was used;
- bytes spooled for the logical candidate;
- whether the logical candidate spilled to disk;
- optional in-memory retained bytes and spill file bytes;
- aggregate counters for candidates streamed, candidates spooled, candidates
  spilled, total bytes spooled, and total spill bytes;
- replay count or replayed candidate count.

## Structural Transform Control

LoneJSON owns JSON structure and separators. Callers must not write raw commas,
colons, quotes, object keys, string escapes, or container punctuation.

The transform control surface must support:

- keep current semantic value;
- drop an object member, array element, or complete logical candidate root;
- replace current value through `lonejson_writer`;
- insert object members before first member, before/after source members, and at
  object end;
- synthesize missing object-member chains;
- emit projection parent objects/arrays and array `null` placeholders;
- stop before later logical candidates are parsed or emitted;
- fail with actionable diagnostics;
- report unsupported transform shapes before invalid partial output.

Missing object-member creation is required. Missing array-index creation should
be supported only through explicit projection placeholder semantics or reported
as unsupported before partial invalid output.

## Recursive Root-Array Flattening

Recursive root-array flattening is part of candidate-stream compatibility.
Nested root arrays become logical candidate streams:

```json
[{"id":"a"}, [{"id":"b"}], {"id":"c"}]
```

emits logical candidates:

```json
{"id":"a"}
{"id":"b"}
{"id":"c"}
```

Selector observation, mutation, projection, limits, gated spooling, replay, and
metadata apply to the logical candidates, not the physical wrapper array.

## Implementation Order

Implement in small, verifiable slices:

1. Extend public status/options/result metadata with modes, counters, spool
   policy selection, and `LONEJSON_STATUS_UNSUPPORTED`.
2. Add parser/value-event offset hooks needed for source-relative logical
   candidate metadata.
3. Refactor scalar handling so streaming mode keeps large strings chunked by
   default and uses opt-in complete old scalar materialization only when needed.
4. Add upfront mode selection and result/counter plumbing.
5. Add gated spooled logical candidate execution using `lonejson_spooled`.
6. Replay matched gated candidates through the same transform executor while
   preserving original candidate metadata.
7. Add object insertion hooks and missing object-member synthesis.
8. Add projection structural control with object/array synthesis and array null
   placeholders.
9. Add recursive root-array logical candidate frames.
10. Expand tests for streaming large fields, gated spool spill behavior,
    same-executor replay, metadata, projection, insertion, limits, partial
    results, and unsupported shapes.
11. Run the standard lifecycle gates and finish with:

```sh
codex review --base trunk
```
