# Fixture Layout

`tests/fixtures/spec/` contains the checked-in, small JSON corpus.
Those files are intentionally distilled and target RFC 8259 syntax edges, parser invariants, and failure modes.

`tests/fixtures/vendor/json_test_suite/` vendors the upstream `test_parsing/` subset from `nst/JSONTestSuite`.
That corpus is kept intact for parser-conformance work and carries the upstream MIT license text.

`tests/fixtures/languages/` contains small checked-in UTF-8 benchmark fixtures
for Japanese, Hebrew, and Arabic text. See that directory's `README.md` for
source URLs and license/provenance notes.

Large payloads are not checked in.
They are generated deterministically by `scripts/generate_large_fixtures.lua` into the shared `build/generated/fixtures/` cache and the local nginx test tree.
The generated set is approximately `100 MiB` in total and includes large objects, large arrays of objects, large base64-heavy documents, and a large JSONL stream.

Generation entry points:

- `cmake --build --preset debug`
- any normal build or test path that depends on the generated fixtures

Reference corpus used for shaping the checked-in cases:

- `nst/JSONTestSuite`: <https://github.com/nst/JSONTestSuite>
