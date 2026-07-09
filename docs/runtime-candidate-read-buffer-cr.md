# Runtime Candidate Read Buffer CR

## Goal

Add runtime-configurable bounded read buffers for candidate stream and candidate
transform APIs, then use the new control to improve the two hot candidate
paths:

- single-pass decision-only candidate scanning;
- candidate transform, including streaming and gated-spooled execution.

The change must reduce transport read overhead without changing the candidate
streaming contract. It is not a license to materialize complete inputs,
implicitly cache candidates, spool whole streams, or hide buffered behavior
behind a streaming API.

## Current Behavior

The public configuration surface exposes runtime policy for allocation limits,
JSON value limits, serializer limits, spool classes, fixed-string scratch, and
providers. It does not expose runtime read-buffer sizing for candidate reader or
file APIs.

Read sizes are currently controlled by compile-time constants:

- `LONEJSON_PARSER_BUFFER_SIZE`, default `4096`;
- `LONEJSON_READER_BUFFER_SIZE`, default `1024`;
- `LONEJSON_STREAM_BUFFER_SIZE`, default `LONEJSON_READER_BUFFER_SIZE`;
- `LONEJSON_PUSH_PARSER_BUFFER_SIZE`, default
  `LONEJSON_PARSER_BUFFER_SIZE`.

The candidate visitor and candidate transform APIs both ultimately parse through
the candidate cursor path. Reader/file entry points are therefore constrained by
the library's internal read-buffer policy rather than by a caller's runtime
configuration.

## Problem

Large candidate streams spend too much time crossing the reader boundary when
the internal read buffer is small. Downstream benchmarks show that candidate
decision-only and transform workloads can become syscall/callback dominated
before parser and selector work become the limiting factors.

Compile-time buffer macros are not sufficient for binary SDK consumers. They
require rebuilding the library, make performance behavior a packaging decision,
and prevent per-runtime tuning for different workloads.

Adding a downstream buffering adapter is an incomplete fix because lonejson owns
the parser, candidate framing, transform executor, file entry points, and
payload/spool policy. The transport buffer should be configured where those
reader calls are made.

## Public API Contract

Add a `lonejson_config` field for candidate reader/file/path/fd input buffer
sizing. The implemented field name is:

```c
size_t candidate_read_buffer_size;
```

The field controls bounded transport read buffering for:

- `lonejson_visit_candidates_reader`;
- `lonejson_visit_candidates_filep`;
- `lonejson_visit_candidates_path`;
- `lonejson_visit_candidates_fd`;
- `lonejson_transform_candidates_reader`;
- `lonejson_transform_candidates_filep`;
- `lonejson_transform_candidates_path`;
- `lonejson_transform_candidates_fd`;
- internal transform calls that replay through the candidate reader path when
  the source is a reader-like handle.

Buffer-backed APIs may ignore this field because the complete caller-provided
buffer is already available to the cursor:

- `lonejson_visit_candidates_buffer`;
- `lonejson_transform_candidates_buffer`.

Defaults must preserve source compatibility. A zero value must not mean
"disable buffering"; it resolves to `LONEJSON_CANDIDATE_READ_BUFFER_SIZE` so
callers can use `lonejson_default_config()` and override only fields they care
about.

Short-name aliases must expose the same field through `lj_config`.

Public clangd/Doxygen comments must describe the default, accepted bounds,
affected reader/file/path/fd APIs, and the buffer-backed exception. The comments
are part of the public SDK distribution because they are visible from
`include/lonejson.h` and the generated single-header artifact.

## Runtime Semantics

The configured buffer is:

- owned by lonejson for the duration of the operation or stream state that needs
  it;
- bounded by the configured byte size;
- used only as a transport chunk between producer callbacks/files/fds and the
  parser cursor;
- released at operation cleanup;
- accounted through the runtime allocator when heap allocation is required;
- subject to existing allocation ceilings where practical and documented where
  stack-backed legacy paths remain compile-time sized.

The configured buffer must not:

- retain a complete input;
- retain complete candidates unless the caller selected an explicit candidate
  capture/spool mode;
- change candidate offsets, byte sizes, payload sizes, framing, callback order,
  transform output, stop behavior, or error diagnostics;
- silently convert streaming transform mode into gated, spooled, or buffered
  behavior.

## Size Policy

Implement a clear validation policy in `lonejson_new()`:

- `0`: use the default candidate read buffer size;
- below the documented minimum: reject with `LONEJSON_STATUS_INVALID_ARGUMENT`;
- above the documented maximum: reject with `LONEJSON_STATUS_INVALID_ARGUMENT`;
- non-power-of-two values: allowed unless implementation evidence shows a real
  need to reject them.

Implemented bounds and default:

- default: `LONEJSON_CANDIDATE_READ_BUFFER_SIZE`, which defaults to
  `LONEJSON_PARSER_BUFFER_SIZE`;
- minimum: `LONEJSON_CANDIDATE_READ_BUFFER_MIN_SIZE`, `1 KiB`;
- maximum: `LONEJSON_CANDIDATE_READ_BUFFER_MAX_SIZE`, `1 MiB`.

The default intentionally follows `LONEJSON_PARSER_BUFFER_SIZE`. Builds that
lower the parser buffer below the candidate minimum must also set
`LONEJSON_CANDIDATE_READ_BUFFER_SIZE` to a supported value. Benchmark evidence
supports `64 KiB` as the first tuned value for large reader/file candidate
workloads, but that remains a per-runtime opt-in rather than a distribution-wide
default.

The implementation should document expected RSS impact as approximately one
candidate read buffer per active candidate reader/transform operation, plus
existing parser workspace, writer state, and selected spool/capture storage.

## Implementation Shape

Prefer one runtime field over per-call options unless implementation evidence
shows that per-call control is required. The candidate hot path is runtime-owned
today, and one runtime-level control keeps the public surface small.

Expected changes:

- add the public field to `lonejson_config`;
- initialize it in `lonejson_default_config()`;
- validate and copy it into runtime state in `lonejson_new()`;
- route candidate reader/file cursor refill through the configured capacity;
- ensure transform streaming and gated-spooled modes inherit the same configured
  capacity when they call the candidate visitor core;
- keep buffer-backed APIs behaviorally unchanged;
- update single-header generation and short aliases;
- update ABI/header checks as required by the public struct change.
- update public API comments in `include/lonejson.h`, including the generated
  single-header source comments consumed by clangd.

If existing cursor storage is stack-backed with fixed-size arrays, convert only
the candidate reader/file cursor buffer to a runtime-sized allocation. Avoid
large stack allocations for tuned values such as `64 KiB` and above.

## Performance Baseline

Before implementation work beyond this spec, record a baseline from the current
branch. Capture:

- `make bench-check`;
- candidate-specific microbenchmarks for decision-only scanning;
- candidate-specific microbenchmarks for transform streaming;
- candidate-specific microbenchmarks for transform gated-spooled;
- syscall or reader-call counts for at least one large reader/file candidate
  input;
- peak RSS for each candidate benchmark row.

The baseline records should include:

- git commit and dirty state;
- compiler and preset;
- host id;
- buffer constants reported by the benchmark run;
- input byte size;
- candidates seen;
- matches or emitted candidates where applicable;
- elapsed ns, MiB/s, and ns/byte;
- reader callback count or `read(2)` count;
- peak RSS.

Do not freeze new baseline files until the benchmark schema and gate are stable.
Use temporary run artifacts for iteration unless the benchmark contract itself
is intentionally updated.

## Candidate Benchmark Work

Add benchmark coverage focused on the hot paths rather than only broad JSON
corpus parsing.

Required benchmark cases:

- decision-only candidate scan over large NDJSON-like candidate streams;
- decision-only recursive array item scan;
- streaming transform with keep/drop/replace decisions that can commit
  incrementally;
- gated-spooled transform where unmatched candidates are dropped before replay;
- file/reader-backed variants that exercise the configured read buffer;
- at least one buffer-backed control case proving the runtime read-buffer field
  does not affect already-buffered inputs.

Each case should report:

- configured candidate read buffer size;
- effective reader calls;
- bytes read;
- candidates seen;
- candidates emitted/dropped/spooled/replayed where applicable;
- elapsed time and throughput;
- peak RSS when the benchmark harness supports it.

The first implementation target is proportional reader-call reduction. For a
large reader-backed input, moving from roughly `4 KiB` effective reads to
`64 KiB` should reduce reader/syscall count by about `16x`; `128 KiB` should
reduce it by about `32x`. Runtime speedup is expected to be lower than call-count
reduction because parser and transform work remain.

## Optimization Loop

Use this loop after the spec lands:

1. Record baseline with current constants and no runtime buffer field.
2. Add the public runtime config and tests that prove configured reader-call
   reduction.
3. Run candidate microbenchmarks at `4 KiB`, `16 KiB`, `64 KiB`, `128 KiB`,
   `256 KiB`, and `1 MiB`.
4. Pick the smallest size that captures most of the throughput improvement
   without materially increasing RSS.
5. Profile remaining hot rows after transport read overhead stops dominating.
6. Optimize transform and decision-only internals only where the profile shows
   parser, callback, writer, or spool work is now the bottleneck.
7. Stop when larger buffers or local hot-path changes produce only noise-level
   improvement or move cost into RSS/allocator pressure.

## Acceptance Tests

Tests must prove observable behavior:

- default config preserves existing candidate reader and transform behavior;
- configured larger read buffer reduces underlying reader calls for
  `lonejson_visit_candidates_reader`;
- configured larger read buffer reduces underlying reader calls for
  `lonejson_transform_candidates_reader` in streaming mode;
- configured larger read buffer reduces underlying reader calls for
  `lonejson_transform_candidates_reader` in gated-spooled mode;
- `filep`, `path`, and `fd` entry points use the same policy through their
  reader adapters;
- buffer-backed candidate APIs ignore the field and preserve output;
- invalid sizes fail at runtime construction with actionable diagnostics;
- candidate offsets, byte sizes, logical indices, framing, stop behavior,
  malformed-input errors, capture decisions, transform output, and result
  counters are unchanged across default and tuned buffer sizes.

## Verification Gates

Minimum completion gate for the CR:

- unit tests for config validation and reader-call-count behavior;
- transform tests for streaming and gated-spooled reader-backed paths;
- single-header/config alias tests;
- ABI/header version checks updated intentionally;
- `make test-all`;
- `make bench-check`;
- candidate benchmark report comparing baseline, selected tuned buffer size, and
  at least one larger size that demonstrates diminishing returns;
- RSS comparison showing memory growth is bounded by the configured transport
  buffer and does not violate existing ceilings.

## Non-Goals

Do not add:

- full-input buffering;
- hidden temp-file staging;
- candidate caches;
- implicit candidate materialization;
- selector-specific behavior;
- transform-specific payload shortcuts that bypass lonejson writer/parser
  ownership;
- dependency-specific APIs or compile-time-only tuning requirements.

## Resolved Decisions

- The default remains compatibility-sized through
  `LONEJSON_CANDIDATE_READ_BUFFER_SIZE`, currently
  `LONEJSON_PARSER_BUFFER_SIZE`.
- `64 KiB` is the recommended first explicit tuning value for large
  reader/file candidate streams.

## Open Decisions

- Whether `candidate_read_buffer_size` should also influence non-candidate
  reader APIs in a later, separate CR.
- Whether benchmark gate thresholds should become absolute candidate hot-path
  requirements or remain comparative against frozen host baselines.
