# Third-Party Integrations

This document defines how `lonejson` integrates with third-party C libraries.
The rule is deliberately strict: core `lonejson` has no mandatory third-party
runtime or link dependencies. Third-party code is an integration boundary, not a
baseline package dependency.

The intended developer experience is not hostile to common libraries. `lonejson`
can provide optimized adapters for widely used stacks such as libcurl and
OpenSSL/libcrypto. Those adapters make the easy path easy for consumers that
choose those libraries. They do not make those libraries dependencies of the
plain core SDK.

## Core Contract

The plain `lonejson` SDK must be consumable as `lonejson` alone:

- `lib/pkgconfig/lonejson.pc` must not require curl, OpenSSL, libcrypto,
  libssl, TLS libraries, framework libraries, or transport/auth stacks.
- `lib/cmake/lonejson/lonejsonConfig.cmake` must not attach third-party link
  libraries to `lonejson::lonejson` or `lonejson::lonejson_static`.
- Optional adapter metadata may expose explicit opt-in targets or pkg-config
  files such as `lonejson::curl`, `lonejson::openssl`, `lonejson-curl.pc`, and
  `lonejson-openssl.pc`. Those adapter surfaces may define feature macros and
  link their third-party dependency because the consumer selected them.
- Binary SDK archives must not bundle third-party headers or libraries unless a
  separate bundled-SDK contract explicitly says so.
- Public core APIs must not require third-party headers.
- Optional integration APIs may expose third-party types only behind their
  feature guards.

Build tools may use third-party SDKs to compile optional integration surfaces.
That build provenance is not a downstream dependency contract. Release metadata
must not turn a build input into a consumer link requirement.

## Integration Pattern

Use the libcurl integration as the reference model.

`LONEJSON_WITH_CURL` enables curl-specific adapter declarations. Consumers that
enable it are responsible for compiling in an environment that provides curl
headers and for linking curl where their chosen integration requires it. With
packaged CMake, the normal path is `find_package(lonejson CONFIG REQUIRED
COMPONENTS curl)` plus `target_link_libraries(app PRIVATE lonejson::curl)`.
That adapter target defines `LONEJSON_WITH_CURL` and links `CURL::libcurl`.
With pkg-config, use `lonejson-curl`. The plain `lonejson` package remains
independent of curl.

The HTTP provider boundary is callback-backed. OAuth2/OIDC helpers call a
`lonejson_http_provider`; the provider implementation may use libcurl, a
platform HTTP client, a service framework, a test fixture, or another transport.
The curl adapter is an optimized convenience implementation of that pattern.

`LONEJSON_WITH_CURL=ON` is intentionally a header/callback integration, not a
linked libcurl implementation inside core lonejson. It may include
`<curl/curl.h>`, expose curl-compatible callback signatures, use curl callback
types such as `curl_off_t`, and return curl callback constants such as
`CURL_READFUNC_ABORT`. It must not call `curl_easy_*`, `curl_multi_*`, or any
other libcurl link symbol from `liblonejson.a` or `liblonejson.so`. The
downstream application owns the actual libcurl handle and links libcurl only
because it chose libcurl for its transport implementation.

The same pattern applies to crypto providers:

- `LONEJSON_WITH_JWT` enables JWT, JWK, JWKS, and claim-policy APIs.
- `LONEJSON_WITH_OIDC` enables OAuth2/OIDC orchestration and requires JWT.
- `LONEJSON_WITH_OPENSSL` enables the optional OpenSSL auth-provider adapter.
- JWT/OIDC without OpenSSL is valid. Runtime-backed helpers use
  `lonejson_auth_provider` for signature verification, SHA-256, and random
  bytes.

The provider boundary is the stable integration point. An application may back
`lonejson_auth_provider` with OpenSSL, another crypto stack, a platform crypto
API, or a test fixture. `lonejson` depends on the provider vtable, not on that
implementation.

The OpenSSL adapter is an optimized convenience implementation for consumers
that already want OpenSSL/libcrypto. It must remain optional. If a downstream
project can satisfy `verify_jws`, `sha256`, and `random_bytes` through another
implementation, that is equally valid.

`LONEJSON_WITH_OPENSSL=ON` must follow the same rule as `LONEJSON_WITH_CURL=ON`.
It may expose OpenSSL-shaped setup declarations, accept opaque OpenSSL-owned
objects such as an `X509_STORE *`, and make it easy for a consumer that already
uses OpenSSL to fill a `lonejson_auth_provider`. It must not compile `EVP_*`,
`RAND_*`, `X509_*`, `BN_*`, `OSSL_*`, `CRYPTO_*`, or other OpenSSL/libcrypto
link-symbol references into core `liblonejson.a` or `liblonejson.so`.

With packaged CMake, the normal OpenSSL path is `find_package(lonejson CONFIG
REQUIRED COMPONENTS openssl)` plus `target_link_libraries(app PRIVATE
lonejson::openssl)`. That adapter target defines `LONEJSON_WITH_OPENSSL`,
enables the JWT auth surface transitively, and links `OpenSSL::Crypto`. With
pkg-config, use `lonejson-openssl`.

The core release must therefore be able to enable the full OpenSSL integration
surface while still linking a plain static consumer with only `liblonejson.a`.
If an OpenSSL-backed helper needs actual OpenSSL calls, those calls must live at
the consumer's opt-in boundary, not in the core archive. Do not solve this by
disabling `LONEJSON_WITH_OPENSSL` in binary releases, and do not solve it by
making a second mandatory adapter library. The invariant is that OpenSSL is an
easy provider implementation for downstream users that choose it, not a core
dependency.

## Symbol-Level Invariant

Every binary SDK release must satisfy this invariant for both shared and static
artifacts:

- Enabling a `LONEJSON_WITH_*` integration surface may change public
  declarations and exported lonejson-owned symbols.
- Enabling that surface must not add mandatory third-party link symbols to
  `liblonejson.a` or dynamic dependencies to `liblonejson.so`.
- Plain static consumers must link using only the shipped core package metadata
  and `liblonejson.a`.
- Plain shared consumers must not load third-party integration libraries unless
  the downstream application loads or links them for its own provider.
- Optimized adapters may rely on third-party headers, types, constants, and
  callback ABI compatibility; they may not hide third-party runtime/link
  requirements inside the core archive.

This is the practical distinction between integration and dependency. A
dependency is present when a consumer that does not opt into the third-party
implementation still has to provide that third-party library to link or load
plain lonejson. That state is forbidden.

## Adapter Rule

Adapters are allowed when they improve correctness, performance, or DX for a
popular third-party stack. They must follow these constraints:

- The adapter is opt-in through a feature guard, build option, separate helper,
  or provider installation step.
- The adapter may depend on the third-party library; core `lonejson` may not.
- The provider/callback interface remains the product contract.
- A downstream implementation that satisfies the same interface is first-class,
  even when it does not use the optimized adapter.
- Release package metadata for plain `lonejson` must not mention the adapter's
  third-party link requirements.
- Core release archives must not contain unresolved third-party adapter symbols
  in `liblonejson.a` or dynamic third-party loader dependencies in
  `liblonejson.so`.

Examples:

- Good: `lonejson_http_provider` callback implemented with libcurl by the
  application.
- Good: optional curl helper that feeds a `CURLOPT_READFUNCTION`.
- Good: `lonejson_auth_provider` implemented with OpenSSL/libcrypto.
- Good: `lonejson_auth_provider` implemented with another crypto backend.
- Good: `LONEJSON_WITH_OPENSSL` exposing an OpenSSL provider setup surface that
  a consumer can satisfy at its own compile/link boundary.
- Bad: `lonejson::lonejson_static` forcing `OpenSSL::Crypto` or `crypto`.
- Bad: `lonejson.pc` forcing `-lcurl`, `-lssl`, or `-lcrypto`.
- Bad: `liblonejson.a` containing unresolved `EVP_*`, `RAND_*`, `X509_*`,
  `BN_*`, or `curl_easy_*` references.
- Bad: disabling `LONEJSON_WITH_OPENSSL` in binary releases to avoid the
  dependency instead of fixing the integration boundary.

## Header Rules

Core declarations in `include/lonejson.h` must remain self-contained for C
consumers. Optional integration declarations follow these rules:

- Use feature guards such as `LONEJSON_WITH_CURL` and
  `LONEJSON_WITH_OPENSSL`.
- Do not include third-party headers from the core surface unless that feature
  explicitly requires them.
- Prefer opaque `void *` or caller-owned provider callbacks when the third-party
  object does not need to be part of the public C type system.
- Document ownership and lifetime at the integration boundary.

For OpenSSL specifically, `lonejson_openssl_auth_provider_config` stores
OpenSSL-owned objects as opaque pointers. The OpenSSL adapter may use OpenSSL
internally, but the core auth provider contract stays independent of OpenSSL.

## Package Metadata Rules

Release package metadata is part of the public API. It must describe how to
consume core `lonejson`, not how the release build happened.

Forbidden in core `lonejson.pc` and core CMake targets:

- `Requires:` or `Requires.private:` entries for curl, OpenSSL, libcrypto,
  libssl, or equivalent third-party integration libraries.
- `Libs.private:` entries such as `-lcurl`, `-lssl`, or `-lcrypto`.
- CMake `INTERFACE_LINK_LIBRARIES` entries such as `CURL::libcurl`,
  `OpenSSL::Crypto`, `crypto`, `ssl`, or `curl` on `lonejson::lonejson` or
  `lonejson::lonejson_static`.

Allowed:

- Project-owned libraries and include directories.
- Relocatable package paths.
- Build-input provenance that is explicitly marked as build input, does not
  contain local paths, and does not imply a downstream dependency.
- Explicit adapter package metadata. For example, `lonejson-curl.pc` may
  require `libcurl`, `lonejson-openssl.pc` may require `openssl`,
  `lonejson::curl` may link `CURL::libcurl`, and `lonejson::openssl` may link
  `OpenSSL::Crypto`.

## Release Blockers

Prerelease and release verification must fail before publishing when a binary
SDK archive forces a third-party integration dependency into core package
metadata.

The archive verifier checks extracted release artifacts, not staging
directories. It rejects:

- curl/OpenSSL/crypto requirements in core pkg-config metadata,
- curl/OpenSSL/crypto link interfaces on core CMake targets,
- unresolved third-party link symbols in core static archives,
- third-party dynamic loader dependencies in core shared libraries,
- OpenSSL advertised as a core SDK build input,
- local dependency-cache paths or build paths in metadata,
- bundled third-party dependency payloads unless explicitly contracted.

Regression tests create release-shaped throwaway archives that intentionally
inject these bad metadata entries and assert that verification fails with an
actionable diagnostic. That makes a forced dependency a prerelease blocker, not
a reviewer preference.

## Adding A New Integration

Before adding a third-party integration:

1. Define the feature guard and CMake option.
2. Keep the core API buildable without the third-party headers.
3. Put the third-party behavior behind an adapter, provider, callback, or
   explicitly guarded helper.
4. Ensure release package metadata for plain `lonejson` remains dependency-free.
5. Add negative package-verification tests that fail if the integration becomes
   a forced core dependency.
6. Document the opt-in build and link expectations for consumers that enable the
   integration.

If an integration cannot satisfy this contract, stop and treat it as a product
decision. Do not silently make core `lonejson` depend on the third-party
library.
