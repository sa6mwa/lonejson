# lonejson Auth Roadmap

This document tracks remaining direction for optional authentication helpers in
lonejson. Implemented API behavior, invariants, verification, and composition
patterns live in `docs/auth-implementation.md`.

The goal remains narrow: make common JSON-centered authentication flows easier
for C and Lua applications without turning lonejson into an HTTP framework, a
Kore module, a credential database, or a general identity provider SDK. Core
lonejson should provide portable primitives and small flow helpers.
Framework-specific wiring belongs in integration layers such as Vectis/Kore
glue.

## Stable Direction

- Parsing a JWT is never a trust decision.
- Trust requires explicit signature validation and claim-policy validation.
- Normal lonejson consumers must not need OpenSSL, libcrypto, libssl, or curl
  merely because auth support exists in the source tree.
- The shared `liblonejson` must not require libcurl, libssl, or libcrypto for
  auth parsing/orchestration alone. Optional providers and adapters own those
  dependencies.
- HTTP transfer, retry, proxy, redirect, TLS policy, event-loop integration,
  and telemetry remain caller-owned.
- Durable credential storage, locking, encryption, rotation, auditing, and
  application authorization remain caller-owned.
- Core lonejson must not add Kore- or Vectis-specific public APIs.
- Large or attacker-controlled inputs must stay bounded; APIs must not hide
  full-message materialization behind streaming-looking names.
- Lua should facade C behavior, not reimplement independent crypto, storage, or
  protocol logic in Lua.

## Implemented Baseline

The current branch already implements the planned C baseline for:

- `LONEJSON_WITH_OPENSSL`, `LONEJSON_WITH_JWT`, and `LONEJSON_WITH_OIDC`
  feature gates,
- provider-backed JWT/JWK/JWKS parsing, selection, signature validation, and
  claim validation,
- OpenSSL-backed `RS256`, `PS256`, `ES256`, and Ed25519 `EdDSA` validation,
- OAuth2/OIDC discovery, JWKS cache, token endpoint helpers, refresh-token body
  construction, authorization-code with PKCE helpers, callback parsing, and
  bearer-token validation,
- generic base64 helpers in C and Lua, including unpadded base64url for JWT/JWS
  segments,
- runtime HTTP provider callbacks with explicit user-agent configuration,
- server-local M2M credential generation and Basic/Bearer verification against
  caller-owned JSON store bytes,
- server-local signup seed generation and signup completion into generated M2M
  credentials,
- Lua facades for JWT/JWK/JWKS, OIDC/OAuth2, M2M credential verification, and
  signup helpers,
- C runtime/free-function/short-alias surfaces, ABI/package checks, fuzz gates,
  and targeted e2e fixtures for OIDC bearer and M2M Basic/Bearer verification.

For exact API behavior and known implementation gaps, use
`docs/auth-implementation.md`.

## Remaining Work

### Signup E2E Fixture

Add a small signup web-handler fixture that exercises the seed flow end to end:

- admin-generated signup seed and public query/URL,
- recipient-provided email metadata,
- signup completion into generated credential material,
- caller-owned removal of the consumed signup seed,
- successful use of the generated API key or client credentials from a
  non-lonejson client.

This should remain a fixture, not a general HTTP server abstraction.

### Examples And Integration DX

Add examples-level helpers or documentation for common embedding patterns:

- curl-backed HTTP provider callback using caller-owned curl policy,
- Kore/Vectis-style request-handler composition for bearer validation,
- server-local M2M credential store update pattern with caller-owned locking,
- admin CLI pattern for signup seed generation.

These should start as examples or integration-layer code, not dependencies in
`liblonejson.so`.

### Operational Auth Gaps

The following remain deliberate non-core areas unless a future requirement
justifies a narrowly scoped helper:

- browser launcher,
- localhost callback listener,
- refresh scheduler,
- retry/proxy/redirect/TLS policy abstraction,
- credential-store persistence, encryption, rotation, and audit helpers,
- token introspection endpoint,
- revocation endpoint,
- device authorization flow,
- userinfo endpoint helper.

### Crypto And JOSE Gaps

Current support is intentionally conservative. Future work may add:

- HS256 only if a clear key-management boundary is specified,
- Ed448 if there is real demand and provider support is clean,
- `x5c`/`x5t` certificate-chain validation,
- `crit` JOSE header handling,
- JWK `key_ops` enforcement,
- encrypted JWT/JWE support.

`alg: none` should remain rejected outside explicit test-only paths.

## Future Slice Rules

Each new auth slice should include:

- C regression tests for success paths and multiple failure modes,
- Lua tests when the surface is exposed in Lua,
- fuzz coverage for attacker-controlled inputs,
- ABI/export/package checks when public symbols change,
- dependency-leakage checks when optional providers or adapters are involved,
- documentation updates in `docs/auth-implementation.md` and README when public
  behavior or DX changes.
