# LoneJSON Candidate Transform V2 Streaming Implementation Draft

## Purpose

This is a draft implementation direction for the remaining candidate transform
V2 work, intended for liblql feedback.

The goal is still the CR outcome: liblql should be able to remove its
callback-source candidate replay paths for mutation/projection while preserving
public candidate behavior. The implementation should do that with two explicit
lonejson-owned execution modes:

- a real streaming rewrite mode for stream-commit-safe plans; and
- a gated spooled rewrite mode for selector-dependent plans whose output cannot
  be safely committed until late selector truth is known.

The streaming mode must provide:

- no full source materialization;
- no full candidate materialization;
- no all-candidate or all-match materialization;
- no hidden temporary files used as replay;
- no token replay queue;
- no full projected-output materialization;
- support replacing very large strings/base64 fields with small values without
  retaining the old large field.

The gated spooled mode is deliberately different: it may retain a current
candidate in memory up to a configured watermark and then spill to disk through
the existing `lonejson_spooled` policy. That mode is still valid for replacing
liblql replay paths because the retention is explicit, bounded by policy, owned
by lonejson, visible in API/docs/counters, and not hidden behind a streaming
name.

## Current Code Findings

The current `lonejson_transform_candidates_*` implementation in
`src/impl/39_candidate_transform_api.h` is a useful first slice, but it is not
the final shape for large scalar streaming:

- It uses `lonejson_visit_candidates_reader()` with
  `LONEJSON_CANDIDATE_CAPTURE_NONE`, so reader input is parsed once and is not
  privately captured by the candidate scanner.
- It owns one `lonejson_writer` per physical candidate and writes directly to
  the caller sink.
- It tracks object keys and container stack frames so object-member drops can
  suppress both key and value while preserving writer-owned separators.
- After the old-value slice, string and number scalars are buffered in
  `state->scalar` so the transform/replacement callback can receive a complete
  callback-scoped old scalar.
- That scalar buffering is correct for old-number dependent replacement, but it
  must not become the default path for large strings, base64 strings, or other
  large field replacement.

The existing `lonejson_value_rewrite_*` implementation in
`src/impl/39_value_rewrite_api.h` already demonstrates the better scalar model:

- Decide keep/drop/replace at value begin when path/type/key/index is enough.
- For kept strings, call `lonejson_writer_string_begin()`, stream every parsed
  string chunk through `lonejson_writer_string_chunk()`, then close with
  `lonejson_writer_string_end()`.
- For dropped/replaced values, consume and validate the original source chunks
  without emitting them.
- For old-value-dependent replacements, enter a replacing mode, stream old
  value events to the caller's old-value visitor, and only buffer the small
  scalar state needed by the chosen operation. Today value rewrite buffers
  numbers for `old_value.number`; strings are exposed to the old-value visitor
  as chunks.
- Object insertion for missing object members is already done at object end by
  `lonejson__value_rewrite_emit_missing_chain()`. This is streaming for object
  creation and deliberately rejects missing array-index synthesis.

The writer already has the primitives needed for large-field replacement:

- `lonejson_writer_string_begin/chunk/end()` streams kept JSON strings without
  materializing them.
- `lonejson_writer_string_reader()` streams a caller reader as one JSON string.
- `lonejson_writer_source_text()` streams a `lonejson_source` as JSON text.
- `lonejson_writer_source_base64()` streams raw outbound source bytes as a
  base64 JSON string.
- `lonejson_writer_spooled_base64()` and `lonejson_writer_string_spooled()`
  support explicitly named spooled inputs when the caller chooses them.
- Writer number chunks are still buffered until validation at number end. That
  is acceptable for normal JSON number limits and not the primary large-field
  concern.

The existing candidate scanner in `src/impl/30_api.h` provides source-relative
candidate metadata for physical candidates:

- `candidate_begin` receives `stream_offset` and unknown `byte_size`.
- `candidate_end` receives the final source-relative `byte_size`.
- `ARRAY_ITEMS` framing already treats top-level array items as candidates.
- Recursive nested root-array logical candidates are not represented today; that
  needs transform-layer logical candidate frames, not candidate capture/replay.

There is currently no dedicated public unsupported-shape status. Runtime shapes
that neither the streaming mode nor the gated spooled mode support should not be
reported as invalid caller arguments. A public `LONEJSON_STATUS_UNSUPPORTED`
would be the clearest status for those cases.

## Clarification For The CR

The CR outcome remains valid, but the wording should be tightened so it does
not imply complete scalar materialization as the normal transform path.

Recommended CR clarifications:

1. "Keep unchanged" means semantic JSON re-emission, not byte-for-byte source
   preservation. LoneJSON may output compact/canonical JSON with different
   whitespace and escaping while preserving the JSON value.
2. Streaming scalar access is the default. Strings are observed and, when kept,
   emitted chunk-by-chunk. Base64 strings that liblql treats as binary should be
   replaceable without storing the original encoded string.
3. Complete old scalar views are opt-in and current-scalar scoped. They are
   suitable for numbers and small string policies, but they must not be required
   for keep/drop/replace of large strings.
4. Scalar transforms should have a begin-time planning phase for streaming
   decisions. If the caller asks for a complete old scalar, lonejson may delay
   replacement until scalar end and document that selected scalar buffering is
   enabled.
5. Root-level late decisions require a mode distinction. Stream-commit-safe
   plans should use semantic re-emission as parsing proceeds. Plans that cannot
   safely commit output before late selector truth must use explicit
   lonejson-owned gated spooling, not hidden streaming.
6. Object insertion should be a first-class supported hook for object members.
   Unsupported reporting remains useful for genuinely impossible streaming
   shapes, but create-missing-member is a required liblql behavior and should
   not be an optional unsupported case.
7. Projection synthesis should be defined as writer-owned structural synthesis:
   lonejson owns object/array punctuation and separators; liblql supplies
   decisions and replacement/synthesized values through control callbacks.

## Proposed Public Model

The current V1-style callback pair should either be superseded by a V2 options
struct or extended in a source-compatible way only if the ABI policy permits it.
The conceptual model is a candidate transform executor with a streaming
structural rewrite path and a gated spooled path. It is not a scalar-end
callback-only transform.

### Plan Classification

Before transforming a candidate stream, liblql should classify the query plan,
or provide callbacks that let lonejson classify it, into one of these modes:

- `streaming`: output can be committed as the candidate is parsed;
- `gated_spooled`: selector truth or output decisions may arrive too late, so
  the current candidate must be retained until the decision is known;
- `unsupported`: the plan or encountered JSON shape is not supported by either
  mode.

Examples:

- match-all mutation/projection is normally stream-safe;
- known-path large field replacement is stream-safe;
- selector truth found before any output-relevant earlier content must be
  committed can be stream-safe;
- `matches_only=true` with late selector truth after earlier fields is
  gated-spooled;
- projection/mutation that needs earlier fields after a late match is
  gated-spooled.

Mode selection must happen before output can leak. Static liblql
classification may conservatively choose `gated_spooled` for any plan where
source field order can make selector truth arrive after output-relevant content.
If lonejson participates in dynamic classification, it must still choose the
candidate's execution mode before committing any bytes for that candidate to the
caller sink. A candidate must not begin in streaming mode and later upgrade to
gated spooling after output has already been emitted.

The mode must be visible. If spooling is selected or triggered, API names,
result metadata, counters, and diagnostics should say so.

### Value Planning

At each value begin, lonejson calls a planning callback with:

- candidate metadata;
- path;
- value type;
- parent relationship:
  - root;
  - object member with decoded key;
  - array element with index;
- depth;
- whether this value is a logical candidate root under recursive root-array
  flattening;
- a control handle for writer-owned actions.

The callback returns a plan:

- keep and stream semantic value;
- drop this value;
- replace this value now through a writer callback;
- skip original and synthesize through control hooks;
- observe old scalar chunks, then decide at scalar end;
- observe old value subtree, then callback at value end for operations that
  genuinely need end-of-subtree knowledge;
- stop successfully;
- fail;
- unsupported shape.

Default keep must be streaming for strings and containers. It must not buffer a
large string before writing it.

### Scalar Streaming

For strings:

- `string_begin` forwards observer begin.
- If the plan is keep, lonejson opens a writer string immediately.
- Each decoded string chunk is sent to observer and, for keep, directly to
  `lonejson_writer_string_chunk()`.
- If the plan is drop or replace, chunks are still parsed/validated and sent to
  observers as requested, but not retained or emitted.
- If the caller explicitly requested complete old scalar materialization,
  lonejson buffers only that selected scalar and invokes the end callback with a
  callback-scoped view.

For base64-like binary fields:

- JSON parsing still sees a string.
- liblql can identify the field by path/policy and either keep the original
  string chunks, drop it, or replace it.
- Replacement with small text, `null`, object, number, or a streamed
  `lonejson_source_base64()` value must not retain the original large base64
  string.
- If liblql needs decoded old bytes, that should be a separate opt-in decoding
  visitor/sink mode with explicit memory/spool policy. It must not be implicit
  in ordinary replacement.

For numbers:

- Current writer validation buffers chunked number text until number end.
- Complete old number text may remain available because JSON numbers are
  bounded by runtime number limits. This is enough for numeric increment.
- Large numeric arrays are handled by streaming array traversal and per-number
  bounded token handling, not by retaining the array.

For bool/null:

- The old value can be passed directly in the event.

### Structural Control

LoneJSON should own output structure and separators. The control handle should
allow these operations only when structurally legal:

- emit current source key;
- suppress current source member or array element;
- begin/end object or array;
- emit object key;
- emit scalar/string/source/base64/json value via `lonejson_writer`;
- insert object members before first source member, before/after a source
  member, or at object end;
- emit array `null` placeholders for projection semantics;
- stop after the current logical candidate is complete.

Callers must not write raw JSON punctuation, commas, colons, quotes, or string
escapes.

### Object Member Insertion

Use the value rewrite model as the baseline:

- Track object frames with decoded current key.
- At object begin, allow caller-specified "prefix members".
- Before/after each source member, allow caller-specified inserted members.
- At object end, allow caller-specified "suffix members" and create-missing
  member chains.
- For missing object chains, synthesize nested objects as value rewrite already
  does.
- Missing array-index synthesis should either:
  - be supported only through explicit projection placeholder policy, or
  - return `LONEJSON_STATUS_UNSUPPORTED` before invalid partial output.

For liblql create-missing-member, object-end insertion is enough for:

```json
{"id":"a"} -> {"id":"a","status":"done"}
```

and nested object creation:

```json
{"id":"a"} -> {"id":"a","state":{"status":"done"}}
```

provided no missing array index must be synthesized.

### Projection Synthesis

Projection cannot be implemented as simple value-level keep/drop because deep
selected fields require synthesized parent containers. The streaming design
should treat projection as a structural plan:

- liblql receives observer events for every source value.
- liblql tells lonejson which source branches are projected.
- lonejson opens projected parent objects/arrays through writer-owned control.
- Unselected siblings are consumed/validated and not emitted.
- Selected descendants are emitted under synthesized parents.
- Arrays can emit `null` placeholders when liblql projection semantics require
  positional preservation.

This can remain streaming if projection state is path/plan-sized rather than
candidate-sized. The transform keeps:

- parser stack;
- writer stack;
- current key;
- array indices;
- projection path trie / caller state;
- active synthesized container stack;
- current scalar scratch only for selected opt-in materialized scalars.

It must not retain the full projected output while waiting for candidate end.

### Candidate Root Decisions

The phrase "candidate-root decision after full observation" should be handled
carefully. If it means "do not emit anything until candidate end, then maybe
emit the original candidate unchanged", that is incompatible with pure
streaming for non-seekable input. It is compatible with an explicit gated
spooled candidate mode. If it means "liblql needs final match/project/mutation
accounting and stop control", that is compatible with streaming.

Recommended model:

- For semantic re-emission, lonejson may start writing as soon as the root plan
  says the candidate can produce output.
- For `matches_only=true`, liblql should decide whether a candidate can emit at
  the earliest selector-determined point. Before that point, output for source
  fields that might need to be suppressed cannot be committed unless the plan
  has already chosen to project/synthesize rather than pass through.
- For projection/mutate modes, liblql builds the output structurally as parsing
  proceeds. It should not need the original candidate bytes at candidate end.
- Candidate end still reports accounting and can request stop before later
  candidates are parsed.
- Shapes where the caller requires "late keep original candidate after selector
  truth known only at candidate end" must be either:
  - represented as a projection/synthesis plan that has been streaming all
    required semantic output;
  - routed through explicit gated spooled mode; or
  - rejected with `LONEJSON_STATUS_UNSUPPORTED` before emitting partial invalid
    output.

This preserves behavior without promising impossible pure-streaming late replay.

### Gated Spooled Mode

Gated spooled mode exists for information-theoretically non-stream-safe plans.
Example:

```json
{"big":"...large...", "id":"a", "status":"open"}
```

Selector:

```text
/status = "open"
```

Mutation:

```text
/id = "b"
```

For `matches_only=true`, the engine cannot emit `"big"` before seeing
`/status`, because an unmatched candidate must produce no output. It also cannot
reconstruct `"big"` after seeing `/status` unless it retained it. Therefore this
plan is not pure-streaming for non-seekable input.

In gated spooled mode, lonejson should:

- parse and validate the source once;
- write the candidate's compact semantic JSON into a `lonejson_spooled` handle;
- keep bytes in memory up to the selected spool policy's `memory_limit`;
- spill to disk after that watermark;
- enforce `max_bytes`;
- after selector truth is known, either discard the spool or replay the spooled
  candidate through the same transform machinery with an explicitly named second
  parse of the compact semantic candidate;
- report that the candidate used gated spooling, including whether it spilled.

This mode is explicit materialization of the current candidate, not streaming.
It is approved for replacing liblql replay paths because it is bounded,
policy-controlled, and owned by lonejson rather than duplicated in liblql.
Spool replay is acceptable because it is named replay of a lonejson-owned
semantic candidate, not hidden streaming and not liblql callback-source replay.

Spooling policy should reuse the existing runtime spool classes:

- `spool_default` for normal candidates;
- `spool_blob` for blob-heavy plans if liblql selects it;
- `spool_large_text` for large text-heavy plans if liblql selects it.

The transform options should allow the caller to select the spool class, memory
watermark, maximum logical candidate bytes, and temporary directory through the
existing `lonejson_config`/spool policy model rather than adding a second spool
configuration system.

Gated spooling must be scoped to the logical candidate that may be emitted, not
only to the physical top-level candidate. With recursive root-array flattening,
each emitted nested item has independent match/projection/mutation semantics.
If a logical candidate is not stream-commit-safe, lonejson must spool that
logical candidate's compact semantic JSON and replay that logical candidate
cleanly. Spooling only the physical wrapper array would either reintroduce
caller-side replay complexity or make source-relative logical metadata hard to
preserve.

Concrete result metadata should include enough information for liblql to report
and test mode behavior. A V2 result/candidate event should expose at least:

- execution mode used for the candidate: `streaming` or `gated_spooled`;
- logical candidate index and physical candidate index;
- source-relative logical candidate offset and byte size when known;
- bytes spooled for that logical candidate;
- whether it spilled to disk;
- optionally, in-memory bytes retained and spill file bytes written;
- aggregate counters for candidates streamed, candidates spooled, candidates
  spilled, total bytes spooled, and total spill bytes.

### Recursive Root-Array Flattening

Recursive root-array flattening should be implemented as logical candidate
frames inside the transform visitor, not by spooling nested arrays and reparsing
them.

For a physical candidate root value:

- If recursive flattening is disabled, transform as today.
- If enabled and the logical candidate root is an array, do not emit the wrapper
  array as a candidate.
- Treat each root-array item as a logical candidate.
- If an item is itself an array, recursively treat its items as logical
  candidates.
- Non-array items become logical candidate roots and are transformed/emitted as
  independent candidates.

Metadata:

- Logical candidates should carry physical candidate index plus logical
  sub-index/depth metadata, or reuse the existing `candidate_info` with a
  documented logical index policy.
- `stream_offset` remains source-relative. For logical candidates it should be
  the first byte offset of the item value when available from parser/cursor
  state.
- `byte_size` can be known at logical candidate end if the parser exposes item
  end offset. If not available initially, the V2 API should document unknown
  logical sizes or add internal offset capture at value begin/end.
- Metadata must never reset to a temporary parser origin.

Implementation note: the current path visitor does not expose byte offsets for
every nested value. Recursive flattening with exact logical item offsets likely
requires adding parser/value-event offset metadata or a transform-private parser
hook around value begin/end. That is still streaming; it is not replay.

### Error And Status Model

The public status model should distinguish at least:

- success;
- caller stop;
- observer callback failure;
- transform callback failure;
- replacement writer failure;
- source reader / file I/O failure;
- sink write failure;
- malformed JSON;
- unsupported transform shape.

Current statuses can express many failures, but not unsupported shape cleanly.
Add:

```c
LONEJSON_STATUS_UNSUPPORTED
```

with documentation such as: "The requested operation is valid API usage but the
encountered JSON shape cannot be transformed by the selected candidate transform
mode."

If adding a status is considered too disruptive, use `LONEJSON_STATUS_TYPE_MISMATCH`
only as a temporary bridge, but that is less precise for liblql diagnostics.

## Implementation Sequence

Recommended implementation order:

1. Refactor candidate transform scalar handling to match value rewrite:
   begin-time plan, streaming string keep, dropped/replaced scalar consume
   without buffering, opt-in materialized old scalar mode.
2. Add V2 event/control structs rather than continuing to overload the current
   scalar-end event. Keep V1 compatibility wrappers if needed.
3. Add plan classification and explicit mode reporting:
   `streaming`, `gated_spooled`, `unsupported`.
4. Add lonejson-owned gated spooled candidate execution using
   `lonejson_spooled`, memory watermarks, disk spill, max-byte limits, and
   visible counters.
5. Add object insertion hooks/control using value rewrite's object-end missing
   chain logic as the first supported model.
6. Add projection structural control with a path-plan/trie friendly API and
   explicit array placeholder support.
7. Add recursive root-array logical candidate frames.
8. Add precise unsupported-shape status and error messages.
9. Expand tests to cover large scalar replacement, gated spooling, and memory
   independence before liblql switches over.

## Required Tests

Tests should prove streaming behavior, not just output equality:

- Reader transform replaces a multi-megabyte string with `"x"` while peak
  lonejson-owned memory remains independent of the string size.
- Reader transform replaces a multi-megabyte base64 string with `null` or a
  small string without retaining the old encoded string.
- Kept large strings are emitted through chunked writer calls and do not use
  current-scalar buffering.
- Complete old scalar view is opt-in and, when enabled, buffers only the
  selected current scalar.
- Late selector `matches_only=true` routes through gated spooled mode, keeps RAM
  below the configured watermark, spills after the watermark, and discards
  unmatched candidates without output.
- Late selector matched mutation replays the spooled candidate and preserves
  earlier large fields semantically.
- Gated spooled mode enforces `max_bytes` with a clear overflow error.
- Result metadata/counters report candidates spooled and candidates spilled.
- Numeric increment works across fragmented reader chunks.
- Object member drop removes key and value.
- Array element drop preserves separators.
- Create-missing object member works at object end.
- Missing array-index creation reports unsupported before invalid partial
  output unless explicit placeholder projection mode is selected.
- Projection emits synthesized parent objects and array `null` placeholders.
- Recursive root-array flattening emits nested items as independent logical
  candidates with source-relative metadata.
- Stop after limit prevents parsing later candidates.
- Malformed JSON after prior complete emitted candidates preserves prior output
  and reports the parse error.
- Sink newline failure is distinguishable from replacement writer failure where
  possible.

## Compatibility Assessment

This direction is compatible with the CR outcome if the CR is interpreted as
semantic rewrite with explicit execution modes rather than pure streaming for
every possible selector shape. It improves the CR by making large scalar and
late selector behavior explicit:

- Default large-string/base64 handling is streaming.
- Complete old scalar materialization is opt-in.
- Candidate output is semantic JSON, not source formatting preservation.
- Projection/root behavior is achieved through structural control instead of
  hidden candidate buffering.
- Non-stream-commit-safe late selector behavior is handled by explicit
  lonejson-owned gated spooling, not by liblql callback-source replay paths.

The main API decision for liblql feedback is whether liblql can provide
begin-time plans for large field keep/drop/replace and classify each query plan
as streaming, gated-spooled, or unsupported. With the approved gated spooled
mode, the remaining CR can replace liblql replay paths while still admitting
that some candidates are explicitly materialized to a bounded memory/disk spool.
