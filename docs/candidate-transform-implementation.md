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
- per-candidate transform metadata and aggregate
  stream/spool/drop/stop/spill/replay counters;
- per-call runtime spool class and maximum spooled candidate byte overrides;
- streaming keep behavior for large strings and numbers by default;
- opt-in complete old scalar materialization;
- same-executor gated replay through `lonejson_spooled`;
- a gated-spooled candidate decision callback that runs after first-pass
  observation and before replay/output;
- caller-owned candidate policy pointers visible to replay transform, replace,
  and insert callbacks from the root callback onward;
- an explicit projection/transform composition policy;
- opt-in projected-candidate composition in gated-spooled mode, where lonejson
  builds a bounded projected spool and then replays that projected candidate
  through normal transform callbacks;
- event metadata for root/object-member/array-element relationship,
  source/replay/projected-source/projected-synthetic origin, and
  source/replay/projected-replay phase;
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
payload. Complete old scalar views are opt-in and current-scalar scoped. Callers
can use the call-wide `old_scalar_mode` for simple cases or `old_scalar` for a
per-event policy that materializes only the string/number values whose
transform or replacement callback needs complete old bytes.

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
- calls `candidate_decision` after the full logical candidate has been observed
  and before any transformed output for that candidate is emitted;
- lets the caller return a candidate-level action and an optional caller-owned
  `candidate_policy` pointer;
- drops candidates before replay/output, stops successfully, fails with a
  callback error, or replays the spooled candidate through the same transform
  executor according to that decision;
- passes the returned `candidate_policy` to replay transform, replace, and
  insert callbacks from the first root callback onward;
- reports the replay and spool behavior in result metadata and counters.

Replay is explicitly named replay: it is a second parse of a lonejson-owned
compact semantic candidate. It is acceptable because it is bounded,
policy-controlled, visible, and owned by lonejson rather than duplicated in
liblql. Public replay coordinates remain original source-relative, never
spool-relative.

`observer` callbacks in gated-spooled mode are first-pass observation only.
Replay does not call `observer`; replay callbacks consume the finalized
candidate policy. This prevents selector evaluators from double-counting first
pass and replay events.

The candidate policy pointer is caller-owned. LoneJSON never owns, copies,
retains, or frees it, and never uses it after the current logical candidate
completes. Candidate transform callbacks are serialized by logical candidate in
one transform call; callbacks for two logical candidates are not interleaved.
This lets liblql store a small policy in its per-call transform state and return
a pointer to that storage without allocating per candidate.

Decision action semantics are:

- `EMIT`: replay the candidate and allow transform callbacks to emit, mutate,
  project, insert, replace, or stop according to normal transform rules;
- `DROP`: skip replay entirely and emit no bytes for the candidate;
- `STOP`: stop successfully before replaying this candidate or parsing later
  candidates;
- `ERROR`: fail the transform and preserve the callback's `lonejson_error`
  diagnostic when supplied.

When liblql needs "emit the candidate that reaches a limit, then stop", it
should return `EMIT` for that candidate and have a replay transform callback
return `STOP` at the appropriate point. The decision-level `STOP` means
"do not replay this candidate; stop before later candidates."

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
- bytes in the projected-candidate spool when projected composition is used;
- aggregate counters for candidates streamed, candidates spooled, candidates
  dropped before replay/output, candidates stopped, candidates spilled,
  candidates projected, total bytes spooled, total projected bytes, and total
  spill bytes;
- replay count or replayed candidate count.

`candidates_dropped` is a transform-result diagnostic counter for candidates
that produced no output. In gated-spooled selector paths it proves sparse
`matches_only=true` candidates were discarded before replay. `candidates_stopped`
counts logical candidates whose transform or gated decision stopped the call
successfully.

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

Projection paths and transform callbacks have two explicit composition policies:

- `SOURCE_EVENTS`, the default, composes projection and callbacks in one
  executor over the source-value event stream while the writer emits projected
  output;
- `PROJECT_THEN_TRANSFORM`, currently supported only in `gated_spooled` mode,
  first builds the projected logical candidate into a bounded `lonejson_spooled`
  handle and then replays that projected candidate through the normal transform,
  replace, and insert callbacks.

`SOURCE_EVENTS` preserves the original streaming-preferred behavior. Transform
callbacks decide source values that are included by projection or are
ancestors/descendants needed for projection synthesis. Projection parent
objects/arrays and sparse `null` placeholders are owned by lonejson writer
logic.

Current callback ordering with projection is:

- source container begin is observed before transform decides that source
  container;
- source scalar callbacks are observed through scalar completion before
  transform decides that source scalar;
- source object-member insert callbacks fire only while lonejson is positioned
  in an emitted object that corresponds to a source object frame;
- projection parent containers and sparse array `null` placeholders may be
  emitted by lonejson without transform callbacks for synthetic values;
- replacement callbacks write the projected output value for the source value
  being replaced, not a separate projected intermediate node.

This means `SOURCE_EVENTS` is source-event driven. It supports
projection-filtered transforms of source values and source-object insertions
that occur while a corresponding source object is being emitted. It does not
define mutation over a fully materialized projected intermediate tree.

When a caller needs mutation over the projected shape, it must select
`PROJECT_THEN_TRANSFORM`. In that mode, full-candidate materialization is still
not a hidden fast-path cost: it is restricted to explicit gated-spooled
candidate storage and the additional bounded projected spool. Both obey the
runtime/per-call spool byte policy and may spill to disk. Selector-only and
streaming-safe paths must continue to use streaming/source-event behavior and
must not pay this projected-spool cost.

In `SOURCE_EVENTS`, the following shapes must be reported as
`LONEJSON_STATUS_UNSUPPORTED` before partial output for the affected candidate
is written unless the caller switches to `PROJECT_THEN_TRANSFORM`:

- mutation or insertion into an object that exists only because projection
  synthesized it;
- insertion callbacks that would need to fire inside projection-synthesized
  parent objects rather than source object frames;
- mutation semantics that require projection-before-mutation over a projected
  intermediate tree rather than source paths;
- sparse array placeholder synthesis combined with mutation or insertion of
  placeholder-derived values;
- any projection/mutation composition whose separators, object keys, array
  indexes, or replacement location cannot be represented by the current
  source-event callback order.

Transform events expose path, value type, candidate metadata, old scalar data,
insertion phase, object key for insertion callbacks, candidate policy,
relationship, origin, and phase. Relationship is `ROOT`, `OBJECT_MEMBER`, or
`ARRAY_ELEMENT`. Origin is `SOURCE` for direct traversal, `REPLAY` for replayed
source-candidate storage, `PROJECTED_SOURCE` for projected replay values copied
from source, and `PROJECTED_SYNTHETIC` for projected replay values synthesized
by projection. Phase distinguishes direct source traversal, source-candidate
replay, and projected-candidate replay.

## Projected Composition Contract

Projection-before-mutation is now a first-class opt-in lonejson-owned transform
behavior for gated-spooled execution. This section is the contract to iterate
against until the feature set is resolved; it describes the implemented
ownership boundary and the remaining refinements that must not be pushed into
downstream code.

The cutover requirement is that projection, mutation, gated policy, replay,
writer state, and unsupported diagnostics remain owned by lonejson. Downstream
consumers must not need a parallel replay path, temporary projected staging
path, second JSON parser, hidden full-candidate cache, or hidden full-output
materialization layer to express projected mutation.

Projection-before-mutation means:

- lonejson evaluates the projection shape for one logical candidate;
- mutation callbacks operate on the projected candidate shape, not accidentally
  on the original unprojected source shape;
- matched candidates are projected and then mutated;
- unmatched candidates are projected and preserved when caller policy says to
  emit without mutation;
- unmatched candidates are dropped before replay/output when caller policy says
  to drop;
- match-limit behavior can emit the limiting projected/mutated candidate and
  then stop before later logical candidates;
- recursive root-array logical candidates each receive independent projection,
  mutation, decision, replay, metadata, and stop behavior.

Concrete behavior that must be representable includes:

- mutation of fields that survive projection;
- mutation of nested fields inside projected source-backed objects;
- creation/insertion into projected source-backed objects;
- creation/insertion into objects that exist only because projection
  synthesized them;
- mutation over sparse projected arrays and projection placeholders where those
  shapes are valid under the caller's projection/mutation semantics;
- pass-through projection of unmatched candidates with mutation and insertion
  disabled;
- fragmented non-seekable callback sources.

Examples of required projected mutation shapes:

```text
project /id, /state
mutate  /state/count++
```

must operate on the projected candidate's `/state/count`, not on an unrelated
source-only branch. Likewise:

```text
project /uri
mutate  /hello = "world"
```

must be able to create `/hello` in the projected candidate root when the caller
policy enables mutation.

Acceptable implementation shapes are:

- a single transform executor with explicit projected-shape event semantics,
  including synthetic containers and placeholders;
- the implemented explicit projection-before-mutation composition mode that
  projects structurally to a bounded spool and then applies mutation through the
  same transform machinery over the projected shape;
- a two-phase lonejson-owned transform where both phases are bounded, explicit,
  visible in result counters/status, and do not move replay or staging back into
  downstream code.

The implementation shape is subordinate to the behavior and ownership boundary.
The implemented shape uses a bounded projection spool per emitted logical
candidate. The API and counters name that behavior precisely as projected
spooling, not streaming. The projected spool obeys the same max-candidate byte
policy as other gated transform storage.

### Projected-Shape Events

The projection-before-mutation slice must define deterministic callback events
over the projected shape.

Events must distinguish enough state for downstream code to avoid reconstructing
phase from output. The current public event surface distinguishes:

- direct source traversal from replayed lonejson-owned storage;
- source-candidate replay from projected-candidate replay;
- projected source-backed replay values from projected synthetic replay values;
- root, object-member, and array-element relationships;
- candidate policy during replay/emit callbacks.

Projection-synthetic containers and scalar placeholders use
`PROJECTED_SYNTHETIC`; source-backed projected values use `PROJECTED_SOURCE`.
This metadata is path-based sidecar metadata collected while lonejson builds the
projected spool. It is not derived from spool offsets and does not expose
spool-relative coordinates as source coordinates.

Paths in mutation callbacks must be paths in the projected logical candidate for
the projection-before-mutation phase. Source-relative candidate metadata remains
available through candidate metadata; event paths must not mix projected paths
with original source-only paths in the same phase without an explicit phase or
origin marker.

Synthetic object containers must have defined insertion positions:

- object-begin insertion before existing projected members;
- before-member and after-member insertion around projected source-backed or
  synthetic members when such ordering is deterministic;
- object-end insertion after all projected members.

If a synthetic array or sparse placeholder can be replaced or mutated, the
event must identify it as an array element with its projected index. If a shape
cannot define deterministic replacement or insertion ordering, it must be
reported as unsupported before output for that candidate is written.

### Event Metadata

The public event and candidate metadata must expose enough state to avoid
ambiguous downstream reconstruction:

- logical candidate index;
- physical candidate index where applicable;
- source-relative logical candidate offset and byte size when known;
- explicit unknown values when exact source-relative values are unavailable;
- projected event path for projected-shape mutation phases;
- event relationship: root, object member, or array element;
- event origin: source traversal, source replay, projected source-backed replay,
  or projected synthetic replay;
- candidate policy pointer during replay/emit callbacks;
- phase when needed to distinguish first-pass observation, replay, projection,
  and projected-shape mutation.

Spool-relative offsets must never be exposed as source offsets. If projected
synthetic events have no source offset, their offset must be unknown or reported
through a synthetic-origin-specific field.

### Unsupported-Before-Output

Unsupported-before-output remains a required safety property, but it is not the
default answer for projected mutation. It is acceptable only for shapes that are
genuinely outside the transform contract or cannot define deterministic JSON
writer behavior.

For any unsupported projected mutation shape:

- return `LONEJSON_STATUS_UNSUPPORTED`;
- set an actionable error message identifying the composition shape;
- write no output bytes for the affected candidate;
- preserve already-completed candidate output from earlier logical candidates;
- stop before later candidates unless the API explicitly defines recoverable
  per-candidate unsupported behavior.

Streaming mode must detect unsupported projected mutation shapes before the
first output byte for the affected candidate is committed. Gated-spooled or
projection-spooled modes should perform this validation before replay/projected
mutation output begins.

### Old Scalar Materialization

Projection-before-mutation must preserve large-value streaming behavior by
default. Numeric increment and similar operations require complete old scalar
text only for the values being mutated. The `old_scalar` callback is the
path/need-scoped policy surface for this: it receives the same event metadata
without an `old_value` and returns whether that one string or number should be
materialized before the transform callback runs.

Required behavior:

- kept large strings and numbers stream by default;
- dropped or replaced large values are consumed and validated without retaining
  unrelated complete payloads;
- complete old number text is available for numeric mutation paths that require
  it;
- numeric token size limits remain explicit and enforced.

### Required Regression Tests

The projection-before-mutation suite must retain tests proving:

- projecting `/id` and `/state` followed by mutating `/state/count++`;
- projecting `/uri` followed by creating `/hello = "world"` in the projected
  candidate root;
- matched candidates are projected then mutated;
- unmatched candidates are projected and preserved unchanged when policy says
  emit without mutation;
- unmatched candidates are dropped before replay/output when policy says drop;
- match limits can emit the limiting candidate and stop before later logical
  candidates;
- recursive root-array logical candidates behave independently;
- fragmented callback-source readers preserve output and counters;
- source read failures before candidate output leave no partial output for that
  candidate, with the status propagated through the applicable reader/callback
  boundary;
- unsupported projected mutation compositions fail with
  `LONEJSON_STATUS_UNSUPPORTED` before candidate output;
- synthetic object insertion and synthetic placeholder replacement behavior are
  deterministic where supported;
- source, replay, and synthetic event metadata are correct and do not expose
  spool-relative offsets as source offsets.

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

Under recursive root-array framing:

- `candidate_decision` runs once per logical non-array candidate;
- replay root transform callbacks use `path->segment_count == 0` for the
  logical candidate root, not the enclosing array indexes;
- candidate metadata remains source-relative to the logical candidate when
  known, and uses explicit unknown values rather than spool-relative ranges when
  exact source ranges are unavailable.

## liblql Integration Contract

liblql owns selector, projection, mutation, match-limit, and public result
semantics. LoneJSON owns JSON parsing, validation, candidate framing, bounded
spooling, replay, writer structure, and transform callback boundaries.

For callback-source candidate mutation and projected mutation, liblql should
route through `lonejson_transform_candidates_reader()` instead of maintaining a
parallel callback-source replay path, temp-file projection-then-mutation staging
path, second JSON parser, hidden full-candidate cache, or hidden full-output
materialization layer.

The liblql transform state remains one caller-owned object per transform call,
containing:

- receiver and allocator state;
- selector evaluator state;
- projection and mutation plan references;
- query options and counters;
- mutation path-frame state and applied-mutation bookkeeping;
- the current finalized candidate policy.

At `candidate_decision`, liblql computes a compact policy from first-pass
observer state:

```text
matched = eval_doc_matches(selector, doc)
emit_candidate = matched || !matches_only
mutate_candidate = matched && mutation_plan != NULL
project_candidate = projection != NULL
drop_candidate = !emit_candidate
```

The policy should be small and candidate-scoped. It must not contain full
candidate bytes, full output bytes, or a replay cache. During replay:

- unmatched candidates with `matches_only=true` return decision `DROP` and
  receive no transform, replace, insert, or output callbacks;
- unmatched candidates with `matches_only=false` return decision `EMIT` with a
  policy that disables mutation and insertion, preserving pass-through output;
- matched candidates return decision `EMIT` with a policy that enables the
  required projection and mutation callbacks;
- transform callbacks use `event->candidate_policy` from the root callback
  onward and must not recompute selector truth from replay-local early state.

liblql may use `LONEJSON_CANDIDATE_TRANSFORM_MODE_STREAMING` for plans where no
late candidate decision can affect already-emitted output. Conservative early
cutover behavior should use `GATED_SPOOLED` for selector-backed callback-source
transforms until the plan is proven stream-commit-safe.

`max_candidates`, `max_matches`, and `max_bytes_read` remain liblql policy:

- `max_candidates` can be checked after logical candidate completion;
- `max_matches` can be checked after the finalized policy says matched;
- `max_bytes_read` should use source-reader byte counts or source-relative
  candidate metadata, not replay/spool bytes;
- stopping must prevent later candidates from parsing or emitting;
- already-emitted complete candidate output remains valid.

Callback failures should preserve the `lonejson_error.message` set by the
callback when possible so liblql can map mutation diagnostics and unsupported
create paths to its public error surface.

## Implementation Order

Historical implementation order, retained to explain the staged surface:

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
10. Add the gated-spooled candidate decision/policy callback and dropped/stopped
    result counters.
11. Add projection-before-mutation composition over projected-shape events, or
    an equivalent lonejson-owned two-phase transform, with synthetic container
    and placeholder semantics.
12. Add event metadata for relationship, origin, projected path, phase when
    needed, and source-relative-versus-synthetic coordinate handling.
13. Add path/need-scoped old-scalar materialization through the `old_scalar`
    callback while preserving the call-wide mode for simple callers.
14. Expand tests for streaming large fields, gated spool spill behavior,
    same-executor replay, metadata, projection, insertion, limits, partial
    results, unsupported shapes, late selector policy, decision drop, decision
    stop/error, projection-before-mutation, synthetic projected-shape events,
    fragmented readers, and recursive array logical candidates.
15. Run the standard lifecycle gates and finish with:

```sh
codex review --base trunk
```
