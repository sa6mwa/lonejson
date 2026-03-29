## Language Bench Fixtures

This directory contains small checked-in UTF-8 benchmark fixtures used by the
C and Lua benchmark harnesses.

Sources and provenance:

- `japanese_hojoki.json`
- `japanese_hojoki_wide.json`
  - Source: 鴨長明『方丈記』, opening passage.
  - Source URL: https://www.aozora.gr.jp/cards/000196/files/975_15935.html
  - Provenance: Aozora Bunko public-domain text.

- `hebrew_fox_and_stork.json`
- `hebrew_fox_and_stork_wide.json`
  - Source: Hebrew Wikisource page `השועל והחסידה`, excerpted summary text.
  - Source URL: https://he.wikisource.org/wiki/השועל_והחסידה
  - Provenance: Wikisource text, licensed under CC BY-SA 4.0.

- `arabic_kalila_intro.json`
- `arabic_kalila_intro_wide.json`
  - Source: Arabic Wikisource page `كليلة ودمنة/باب مقدمة الكتاب`,
    excerpted introductory passage.
  - Source URL: https://ar.wikisource.org/wiki/كليلة_ودمنة/باب_مقدمة_الكتاب
  - Provenance: Wikisource text, licensed under CC BY-SA 4.0.

These fixtures are intentionally short and generic. They are benchmark inputs,
not conformance tests or product examples.
