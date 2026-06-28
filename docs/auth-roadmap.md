# lonejson Auth Roadmap

This document captures the intended direction for optional authentication
helpers in lonejson. It is a planning spec, not a committed public API.

The goal is to make common JSON-centered authentication flows much easier for C
and Lua applications without turning lonejson into an HTTP framework, a Kore
module, or a general identity provider SDK. Core lonejson should provide
portable primitives and small flow helpers. Framework-specific wiring should
live in integration layers such as Vectis/Kore glue.

## Goals

- Parse JWT and JWK/JWKS data through explicit JSON-backed structures.
- Validate JWTs through a dedicated validation API; parsing a JWT must never
  imply trust.
- Support OpenSSL-backed signature validation for common asymmetric JWT
  algorithms.
- Support OAuth2/OIDC client flows that are useful for machine-to-machine and
  command-line or desktop authentication.
- Provide server-side helper primitives that make bearer-token validation easy
  to place before application logic.
- Preserve the existing dependency posture: optional facades are opt-in at
  compile time and must not surprise normal consumers with unrelated dependency
  requirements.
- Keep all APIs streaming- and bounded-memory-friendly where payload sizes can
  be large or attacker-controlled.

## Non-Goals

- Do not add Kore- or Vectis-specific public APIs to core lonejson.
- Do not implement a full HTTP server in lonejson.
- Do not treat decoded JWT claims as trusted until validation policy succeeds.
- Do not hide full-message materialization behind a streaming-looking API.
- Do not silently accept weak or unspecified validation policy.

## Compile-Time Feature Gates

The proposed feature gates are:

- `LONEJSON_WITH_OPENSSL`: enables OpenSSL-backed cryptographic helpers.
- `LONEJSON_WITH_JWT`: enables JWT/JWK/JWKS parsing and validation APIs. This
  requires `LONEJSON_WITH_OPENSSL`.
- `LONEJSON_WITH_OIDC`: enables combined OAuth2/OIDC helpers. This requires
  `LONEJSON_WITH_CURL`, `LONEJSON_WITH_OPENSSL`, and `LONEJSON_WITH_JWT`.

`LONEJSON_WITH_OIDC` should cover both OAuth2 and OpenID Connect flows rather
than splitting the public surface into two competing gates. OIDC is layered on
OAuth2 in practice, and the lonejson use case is an authentication facade, not
a standards taxonomy exercise. API names may still use `oauth2` where the
operation is specifically OAuth2, such as token endpoint exchange.

The dependency model should follow the curl precedent:

- Normal lonejson consumers should not need OpenSSL or curl headers.
- Consumers that opt into JWT validation need OpenSSL headers and libraries.
- Consumers that opt into OIDC helpers need both curl and OpenSSL.
- Release packaging must make dependency requirements explicit in metadata and
  must test that unwanted dependency leakage does not occur.

If the shared `liblonejson` cannot expose an OpenSSL-backed ABI without adding
runtime `libssl` or `libcrypto` requirements for normal consumers, we should
decide deliberately between separate crypto-enabled artifacts and accepting the
runtime dependency for those artifacts. That decision belongs before the first
public ABI for this work.

## JWT/JWK Primitives

JWT support should start as a small, explicit primitive layer:

- Parse JWT compact serialization into header, payload, and signature segments.
- Decode base64url segments.
- Expose header and claims as JSON values or mapped shorthand structs.
- Parse JWK and JWKS documents.
- Select JWKs by `kid`, `kty`, `alg`, and intended use.
- Validate JWT signatures against a selected JWK.
- Validate claims through an explicit policy object.

Parsing APIs should return decoded structure and syntax diagnostics only.
Validation APIs should perform trust decisions and should require a policy.

Validation policy should include:

- accepted issuer values,
- accepted audience values,
- accepted algorithms,
- required claims,
- clock source,
- allowed clock skew,
- `exp`, `nbf`, and `iat` handling,
- key-use constraints,
- maximum token size,
- maximum decoded header and claims size.

The API should reject ambiguous or unsafe defaults. In particular, callers must
not get a successful validation result merely because a signature parses or a
matching key is present.

## OAuth2/OIDC Helpers

OIDC helpers should build on JWT/JWK primitives plus curl transport helpers.
The first useful flows are:

- client credentials flow for machine-to-machine authentication,
- OIDC discovery from an issuer URL,
- JWKS retrieval and bounded cache,
- token endpoint exchange,
- refresh token exchange where the provider supports it,
- authorization-code with PKCE for CLI or desktop login.

The CLI/desktop browser flow should be expressed as reusable flow control, not
as a framework-specific server:

- create an auth session with random state, nonce, PKCE verifier, and callback
  path,
- provide the authorization URL to open in a browser,
- accept the callback request data from an embedding HTTP server,
- validate state and nonce,
- exchange code for tokens,
- validate ID token and issuer through JWKS,
- return token material and structured diagnostics.

Core lonejson can provide helpers for localhost callback request parsing and
response bodies, but the actual HTTP listener should be supplied by the
embedding program or integration layer.

## Server-Side Helpers

Server-side support should be framework-neutral. A Kore or Vectis integration
should be able to call lonejson before app logic, but core lonejson should only
provide small primitives:

- parse `Authorization: Bearer ...` input,
- validate the token against a configured issuer/JWKS cache,
- expose validated claims to the caller,
- produce structured auth failure information,
- classify failures as missing credentials, malformed token, invalid signature,
  expired token, issuer mismatch, audience mismatch, or transport/cache error.

The helper should make transparent machine-to-machine protection easy for an
application, while keeping request routing, response writing, and framework
lifetime rules outside core lonejson.

## Lua Facade

The Lua binding should expose the same conceptual surface without implementing
independent crypto or protocol logic in Lua. Lua should call into the C facade
and convert only normal Lua-facing values:

- JWT parse result,
- JWT validation result,
- JWK/JWKS object handles or decoded tables,
- OIDC session and token exchange helpers,
- structured errors.

Lua must preserve the same distinction as C: parse is not trust, validation is
the trust boundary.

## Security Invariants

The implementation must test and enforce at least these invariants:

- `alg: none` is rejected unless an explicit test-only policy allows it.
- A token signed with one algorithm cannot validate under another algorithm.
- Missing or mismatched `kid` does not silently fall back to an unsafe key.
- Unknown issuers and audiences fail closed.
- Expired and not-yet-valid tokens fail by default.
- Clock skew is bounded and caller-configured.
- Oversized JWT segments, decoded claims, JWKS documents, and cache entries are
  rejected by configured limits.
- Duplicate or malformed claims produce deterministic failure behavior.
- Network retrieval failures do not turn into successful validation.
- Cache refresh failure does not silently accept a token that cannot be
  validated under the configured policy.

## Suggested Implementation Slices

1. Add `LONEJSON_WITH_OPENSSL` detection, packaging policy, and dependency
   verification without public JWT APIs.
2. Add JWT compact parsing and base64url decoding with syntax tests and fuzzing.
3. Add JWK/JWKS parsing and key selection.
4. Add OpenSSL-backed JWT signature validation.
5. Add claim validation policy and failure diagnostics.
6. Add Lua facade for JWT/JWK parse and validation.
7. Add OIDC discovery and JWKS retrieval/cache over curl.
8. Add client credentials flow.
9. Add authorization-code with PKCE flow-control helpers.
10. Add framework-neutral server-side bearer validation helpers.

Each slice should include C tests, Lua tests when surfaced in Lua, negative
regression cases, and fuzzing for attacker-controlled inputs.
