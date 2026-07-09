# Runtime Candidate Read Buffer Performance

This is the current performance record for the runtime candidate read-buffer
CR on host `242867651d905848bc891b3cac236c45`.

The benchmark gate used schema `22`, GCC `15.2.0`, host preset, and
`PERF_ITERATIONS=40`. The latest verified release-candidate gate was:

```sh
make prerelease
```

Its `bench-check` phase completed successfully with no schema mismatch, no
config mismatch, no missing rows, no result mismatches, and no material
regressions. Candidate rows had no mismatches.

## Tuning Summary

The smallest useful transport buffer is `64 KiB`.

It reduces reader calls on the flat reader-backed candidate fixture from `168`
to `11` per operation, about `15.3x`. `128 KiB` reduces calls to `6`, and
`256 KiB` reduces calls to `3`, but the added throughput is mostly noise because
parser and transform work dominate after the reader boundary is no longer hot.

The default follows `LONEJSON_PARSER_BUFFER_SIZE`, but the runtime still enforces
the `1 KiB` candidate minimum. Builds that lower the parser buffer below that
minimum must set `LONEJSON_CANDIDATE_READ_BUFFER_SIZE` explicitly. Consumers with
large reader/file candidate streams should configure `candidate_read_buffer_size`
to `64 KiB` first, then test `128 KiB` only when their transport callback or
syscall cost remains visible.

## Latest Candidate Snapshot

The following values are from the latest full prerelease `bench-check` run after
the candidate scan hot-path cleanup.

| Case | Buffer | Read calls | MiB/s | RSS |
| --- | ---: | ---: | ---: | ---: |
| decision reader | 4 KiB | 168 | 471.283 | 1733148672 |
| decision reader | 64 KiB | 11 | 485.991 | 1733279744 |
| decision reader | 128 KiB | 6 | 456.544 | 1733410816 |
| decision reader | 256 KiB | 3 | 451.752 | 1733410816 |
| decision reader | 1 MiB | 1 | 475.422 | 1734066176 |
| streaming transform reader | 4 KiB | 168 | 87.986 | 1734066176 |
| streaming transform reader | 64 KiB | 11 | 88.923 | 1734066176 |
| streaming transform reader | 128 KiB | 6 | 89.543 | 1734066176 |
| streaming transform reader | 256 KiB | 3 | 92.217 | 1734066176 |
| streaming transform reader | 1 MiB | 1 | 89.727 | 1734066176 |
| gated transform reader | 4 KiB | 168 | 25.367 | 1734066176 |
| gated transform reader | 64 KiB | 11 | 24.988 | 1734066176 |
| gated transform reader | 128 KiB | 6 | 24.506 | 1734066176 |
| gated transform reader | 256 KiB | 3 | 24.070 | 1734066176 |
| gated transform reader | 1 MiB | 1 | 24.464 | 1734066176 |

## Interpretation

Reader-call reduction is strong and proportional through the configured sizes.
Throughput improvement is workload-dependent and quickly reaches diminishing
returns because JSON validation, callback dispatch, writer work, and gated spool
replay become the visible costs.

The branch also improves transform hot paths that replay memory spools and drops
unmatched gated candidates before replay. The gated-drop benchmark rows report
`candidates_spooled`, `candidates_replayed`, and `candidates_dropped`, proving
that replay work is avoided for dropped candidates.

No candidate cache was added.
