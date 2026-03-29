# Vendored JSONTestSuite Subset

This directory vendors only the `test_parsing/` subset from:

- <https://github.com/nst/JSONTestSuite>

Upstream project:

- `nst/JSONTestSuite`
- Author: Nicolas Seriot
- License: MIT

Only the parser conformance corpus is included here.
The original upstream `LICENSE` text is preserved in this directory as `LICENSE`.

Filename semantics from upstream:

- `y_*.json`: parsers must accept the input
- `n_*.json`: parsers must reject the input
- `i_*.json`: parsers may accept or reject the input

This subset is vendored to keep lonejson's parser conformance checks reproducible and offline.
