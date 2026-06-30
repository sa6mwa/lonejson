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
- HTTP transfer, proxy, redirect, TLS policy, event-loop integration, and
  telemetry remain caller-owned.
- Helpers that perform provider-backed token or key fetching should apply
  bounded retry and refresh behavior by default where the next step is
  protocol-obvious. Callers must be able to opt out or override the policy.
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
  and targeted e2e fixtures for OIDC bearer and M2M Basic/Bearer/signup
  verification.

For exact API behavior and known implementation gaps, use
`docs/auth-implementation.md`.

## Remaining Work

The remaining auth target for this branch is near-complete OAuth2/OIDC client
and resource-server support plus JWT/JWK/JWKS compliance. JWE is intentionally
not part of this target.

### Token Flow State Helpers

Add narrowly scoped helpers for token-bearing client flows:

- represent the current access token, optional refresh token, expiry time, and
  flow state without taking over durable storage,
- transparently refresh an expired access token when a refresh token and token
  endpoint inputs are available,
- use bounded retry defaults for transient provider/network failures,
- expose opt-out/override controls for refresh and retry behavior,
- return explicit flow-state/failure values when interaction is required or the
  flow cannot continue.

The server-side bearer validation helper should continue to fail closed for an
expired presented token; a resource server cannot refresh a caller's bearer
token unless the application has separately chosen to own that client flow.

### Server-Side OAuth2/OIDC Helpers

Add provider-backed helpers for common resource-server and administration
flows:

- token introspection request/response helpers,
- token revocation request helpers,
- userinfo request/response helpers,
- bounded response parsing and explicit failure classification,
- the same HTTP provider, user-agent, retry, and size-limit model used by the
  existing discovery/JWKS/token helpers,
- C, Lua, fuzz, and e2e coverage for success paths and multiple failure modes.

These helpers should not own routing, response writing, durable credential
storage, or authorization decisions. They should return authenticated or
provider-reported facts for the embedding application to authorize.

### JOSE/JWK Compliance Completion

Complete the JWT/JWK/JWKS compliance surface needed for OIDC use:

- `x5c` certificate-chain parsing and validation policy,
- `x5t` and `x5t#S256` certificate thumbprint matching,
- `crit` handling: reject tokens with unrecognized critical headers, and only
  accept recognized critical headers when the caller explicitly enables the
  relevant behavior,
- JWK `key_ops` parsing and enforcement for verification use,
- `azp`, `scope`, and `scp` claim parsing/validation helpers where useful for
  OIDC resource-server decisions,
- stronger multiple-audience policy controls where OIDC requires them.

`crit` is a JOSE extension-safety mechanism. Silently ignoring a critical
header is not compliant; the safe default is fail closed.

`key_ops` limits what a JWK may be used for. Signature validation should reject
keys whose declared operations do not allow verification.

### Examples And Integration DX

Add examples-level helpers or documentation for common embedding patterns:

- curl-backed HTTP provider callback using caller-owned curl policy,
- Kore/Vectis-style request-handler composition for bearer validation,
- server-local M2M credential store update, rotation, revocation, and
  consumed-signup removal patterns with caller-owned locking,
- admin CLI pattern for signup seed generation.

These should start as examples or integration-layer code, not dependencies in
`liblonejson.so`.

### Operational Auth Gaps

The following remain deliberate non-core areas unless a future requirement
justifies a narrowly scoped helper:

- browser launcher,
- localhost callback listener,
- background refresh scheduler,
- proxy/redirect/TLS policy abstraction,
- credential-store persistence, encryption, and audit helpers,

OAuth2 device authorization is not the same as a generic "helper knows where in
the flow you are" state machine. A generic flow-state helper belongs in the
token-flow work above; the device authorization grant itself should be added
only when we intentionally support that grant type.

### Out-Of-Scope JOSE/Crypto Work

The following remain outside this OIDC/JWT/JWK completion target:

- HS256 only if a clear key-management boundary is specified,
- Ed448 if there is real demand and provider support is clean,
- encrypted JWT/JWE support.

`alg: none` should remain rejected outside explicit test-only paths.
JWE is a separate encryption feature with its own key-management and content
encryption matrix; it should not be bundled into the current OIDC/JWT/JWK
completion slice.

## Future Slice Rules

Each new auth slice should include:

- C regression tests for success paths and multiple failure modes,
- Lua tests when the surface is exposed in Lua,
- fuzz coverage for attacker-controlled inputs,
- ABI/export/package checks when public symbols change,
- dependency-leakage checks when optional providers or adapters are involved,
- documentation updates in `docs/auth-implementation.md` and README when public
  behavior or DX changes.
