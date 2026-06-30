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
- Provide server-local machine-to-machine credential helpers for services that
  want caller-owned JSON credential stores instead of a full external identity
  provider.
- Provide admin-seeded signup helper primitives that let an application issue a
  one-time signup URL or query string, collect required user metadata, and turn
  that seed into a generated API key or client credential.
- Preserve the existing dependency posture: optional facades are opt-in at
  compile time and must not surprise normal consumers with unrelated dependency
  requirements.
- Keep all APIs streaming- and bounded-memory-friendly where payload sizes can
  be large or attacker-controlled.

## Non-Goals

- Do not add Kore- or Vectis-specific public APIs to core lonejson.
- Do not implement a full HTTP server in lonejson.
- Do not implement durable credential storage, store locking, or credential
  rotation policy in lonejson.
- Do not decide endpoint, method, tool, tenant, or operation authorization from
  authenticated M2M claims. lonejson returns authenticated facts; application
  policy remains caller-owned.
- Do not treat decoded JWT claims as trusted until validation policy succeeds.
- Do not hide full-message materialization behind a streaming-looking API.
- Do not silently accept weak or unspecified validation policy.

## Compile-Time Feature Gates

The proposed feature gates are:

- `LONEJSON_WITH_OPENSSL`: enables OpenSSL-backed cryptographic helpers.
- `LONEJSON_WITH_JWT`: enables JWT/JWK/JWKS parsing and validation APIs. This
  does not require OpenSSL at the core API boundary; signature trust uses an
  installed auth provider.
- `LONEJSON_WITH_OIDC`: enables combined OAuth2/OIDC helpers. This requires
  `LONEJSON_WITH_JWT`, but does not require curl or OpenSSL at the core API
  boundary.

`LONEJSON_WITH_OIDC` is the single feature gate for the combined OAuth2/OIDC
facade. We should not split this into competing `LONEJSON_WITH_OAUTH2` and
`LONEJSON_WITH_OIDC` build gates unless a later requirement needs a pure OAuth2
surface that intentionally excludes OIDC discovery, JWKS, and JWT validation.
OIDC is layered on OAuth2 in practice, and the lonejson use case is an
authentication facade, not a standards taxonomy exercise.

Public API names should still use precise terminology:

- use `oauth2` for OAuth2-generic operations such as token endpoint exchange,
  client credentials, refresh token exchange, and authorization-code exchange,
- use `oidc` for issuer discovery, ID-token validation helpers, OIDC session
  setup, nonce handling, and JWKS-backed issuer validation,
- use `jwt` and `jwk` for standalone token and key primitives.

The dependency model should follow the curl precedent:

- Normal lonejson consumers should not need OpenSSL or curl headers.
- Consumers that opt into JWT/OIDC parsing and policy helpers should not need
  OpenSSL or curl headers.
- Consumers that want local cryptographic trust can install the OpenSSL auth
  provider or provide an equivalent crypto provider.
- Consumers that want local HTTP transfer can use curl adapters or provide
  their own HTTP/request code.
- Release packaging must make dependency requirements explicit in metadata and
  must test that unwanted dependency leakage does not occur.

The shared `liblonejson` must not require libcurl, libssl, or libcrypto merely
because JWT/OIDC parsing and orchestration are enabled. Optional provider
adapters own those dependencies.

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

OIDC helpers should build on JWT/JWK primitives plus explicit provider or
caller-owned transport helpers.
The first useful flows are:

- client credentials flow for machine-to-machine authentication,
- OIDC discovery from an issuer URL,
- JWKS retrieval and bounded cache,
- token endpoint exchange,
- refresh token exchange where the provider supports it,
- authorization-code with PKCE for CLI or desktop login.

The OIDC boundary starts with transport-neutral parsing helpers plus an
optional runtime HTTP provider for common fetch flows:

- build the `.well-known/openid-configuration` URL from an HTTPS issuer,
- parse discovery metadata from caller-provided JSON bytes,
- validate the parsed issuer against the caller's configured issuer,
- install a caller-provided `lonejson_http_provider` on a runtime,
- fetch discovery, JWKS, and client-credentials token responses through that
  provider,
- expose the same helpers through the Lua facade.

Network policy remains caller-owned at this layer. Applications provide an HTTP
provider for event-loop, proxy, retry, telemetry, TLS policy, and actual socket
I/O. The generic provider initializer accepts caller-owned callback config
including a default user-agent, and the simple direct-argument initializer also
accepts the user-agent explicitly. Core lonejson does not embed a curl easy
transfer path into `liblonejson`.

The CLI/desktop browser flow should be expressed as reusable flow control, not
as a framework-specific server:

- create an auth session with random state, nonce, PKCE verifier, and callback
  path,
- provide the authorization URL to open in a browser,
- accept the callback request data from an embedding HTTP server,
- validate callback state and later ID-token nonce through JWT claim policy,
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

Server-local M2M support is a separate path from OIDC bearer validation. It is
for services that want a small JSON credential store, with no external token
issuer:

- generate store-ready credential records containing hashes, salts, auth-mode
  metadata, and caller-supplied opaque claim JSON,
- return cleartext client secrets and API keys only once at generation time,
- verify `Authorization: Basic <base64(client_id:client_secret)>`,
- verify `Authorization: Bearer <api_key>`,
- let callers opt out of either Basic or Bearer mode per verification request,
- return authenticated `client_id`, selected auth mode, and opaque claim JSON,
- keep persistence, locking, encryption, rotation, audit, and authorization
  decisions outside lonejson.

Signup support should compose with the same store shape:

- generate one-time signup seeds with a signup ID, signup secret, optional
  public URL/query string, and store-ready signup record,
- keep the seed record under caller-owned `signups[]`,
- let a web handler collect required user metadata, initially email address,
- complete the seed into a generated credential record and returned one-time
  credential material,
- return the consumed signup ID so caller-owned storage can remove the seed,
- never retain the signup secret or generated cleartext credential in generated
  store records.

## Lua Facade

The Lua binding should expose the same conceptual surface without implementing
independent crypto or protocol logic in Lua. Lua should call into the C facade
and convert only normal Lua-facing values:

- JWT parse result,
- JWT validation result,
- JWK/JWKS object handles or decoded tables,
- OIDC session and token exchange helpers,
- M2M credential verification and signup helpers,
- structured errors.

Lua must preserve the same distinction as C: parse is not trust, validation is
the trust boundary.

## Security Invariants

The implementation must test and enforce at least these invariants:

- `alg: none` is rejected unless an explicit test-only policy allows it.
- A token signed with one algorithm cannot validate under another algorithm.
- Missing or mismatched `kid` does not silently fall back to an unsafe key.
- Unknown issuers and audiences fail closed.
- Configured OIDC nonce expectations fail closed on mismatch.
- Expired and not-yet-valid tokens fail by default.
- Clock skew is bounded and caller-configured.
- Oversized JWT segments, decoded claims, JWKS documents, and cache entries are
  rejected by configured limits.
- Duplicate or malformed claims produce deterministic failure behavior.
- Network retrieval failures do not turn into successful validation.
- Cache refresh failure does not silently accept a token that cannot be
  validated under the configured policy.
- Generated M2M credential records do not contain cleartext client secrets or
  API keys.
- M2M verification rejects wrong secrets, wrong API keys, missing credentials,
  malformed Authorization headers, and disallowed auth modes.
- Signup records do not contain cleartext signup secrets.
- Signup completion requires the configured signup secret and required email
  metadata.
- Signup completion returns the consumed signup ID so callers can invalidate the
  seed in persistent storage.

## Suggested Implementation Slices

1. Add `LONEJSON_WITH_OPENSSL` detection, packaging policy, and dependency
   verification without public JWT APIs.
2. Add JWT compact parsing and base64url decoding with syntax tests and fuzzing.
3. Add JWK/JWKS parsing and key selection.
4. Add provider-backed JWT signature validation with an OpenSSL adapter.
5. Add claim validation policy and failure diagnostics.
6. Add Lua facade for JWT/JWK parse and validation.
7. Add OIDC discovery and JWKS retrieval/cache helpers. Discovery URL
   construction, discovery JSON parsing, issuer validation, Lua facade, bounded
   JWKS cache installation, cache selection, provider-backed discovery fetch,
   provider-backed JWKS refresh, and a curl write/finish adapter are
   implemented. The HTTP transport remains caller-owned through the provider
   boundary so lonejson does not hide retry, timeout, credential, proxy, TLS, or
   event-loop policy.
8. Add token endpoint helpers. Transport-neutral client-credentials,
   refresh-token, and authorization-code form body builders, provider-backed
   token request helpers, bounded token response parsing, Lua facade,
   regression tests, ABI/package checks, and fuzz coverage are implemented. HTTP
   transport, retry, TLS, proxy, and credential storage policy remain
   caller/provider-owned.
9. Add authorization-code with PKCE flow-control helpers. PKCE S256 challenge
   derivation, random verifier/challenge generation, authorization URL
   construction, callback query parsing/state validation, Lua facade,
   regression tests, ABI/package checks, and fuzz coverage are implemented.
   Browser launching and localhost listener lifetime remain caller-owned.
10. Add framework-neutral server-side bearer validation helpers. Bearer
    Authorization header extraction, fresh JWKS cache selection, compact JWT
    decode, signature validation, claim-policy validation, structured failure
    classification, Lua facade, regression tests, ABI/package checks, and fuzz
    coverage are implemented. Request routing, response writing, cache refresh,
    and framework lifetime remain caller-owned.
11. Add server-local M2M credential helpers. Store-ready credential generation,
    Basic and Bearer Authorization verification, opaque claim return, auth-mode
    opt-out, runtime method wiring, ABI/package checks, C regression tests, and
    curl-driven fixture e2e coverage for Basic/Bearer request handling are
    implemented. Persistent storage, locking, encryption, rotation, and
    application authorization remain caller-owned.
12. Add server-local signup seed helpers. Signup seed generation, optional
    signup URL/query construction, signup completion with email metadata,
    generated credential return, consumed-signup ID return, runtime method
    wiring, ABI/package checks, and C regression tests are implemented.
    Caller-owned web UX and persistent store mutation remain outside lonejson.
    A Lua facade and e2e signup handler fixture remain future work.

Each slice should include C tests, Lua tests when surfaced in Lua, negative
regression cases, and fuzzing for attacker-controlled inputs.
