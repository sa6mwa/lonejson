# lonejson Auth Implementation

This document describes the current JWT, JWK, JWKS, OAuth2, and OIDC
implementation in lonejson. It is a status writeup for the implemented feature
surface, including boundaries, invariants, verification, and known gaps.

The implementation is intentionally not a full identity-provider SDK, HTTP
client, HTTP server, Kore integration, or Vectis integration. It provides
bounded JSON-centered authentication primitives and small flow helpers. Network
fetching, browser launching, request routing, response writing, retry policy,
proxy policy, TLS policy, credential storage, logging policy, and framework
lifetime rules remain caller-owned.

## Feature Gates

The auth surface is compile-time optional.

- `LONEJSON_WITH_OPENSSL` enables OpenSSL-backed cryptographic internals.
- `LONEJSON_WITH_JWT` enables JWT, JWK, JWKS, and claim/signature validation
  APIs. It requires `LONEJSON_WITH_OPENSSL`.
- `LONEJSON_WITH_CURL` enables existing curl adapter APIs.
- `LONEJSON_WITH_OIDC` enables OAuth2/OIDC helpers. It requires
  `LONEJSON_WITH_CURL`, `LONEJSON_WITH_OPENSSL`, and `LONEJSON_WITH_JWT`.

The corresponding CMake options are:

- `LONEJSON_BUILD_WITH_OPENSSL`
- `LONEJSON_BUILD_WITH_JWT`
- `LONEJSON_BUILD_WITH_CURL`
- `LONEJSON_BUILD_WITH_OIDC`

CMake enforces the dependency graph. `LONEJSON_BUILD_WITH_JWT=ON` without
OpenSSL is a configure error. `LONEJSON_BUILD_WITH_OIDC=ON` without curl,
OpenSSL, or JWT is also a configure error.

When `LONEJSON_C_PKT_SYSTEMS_ROOT` is set, curl and OpenSSL are resolved through
the c.pkt.systems bundle. OIDC-capable release/package builds require the
c.pkt.systems route to provide curl and OpenSSL package metadata. Normal
non-auth consumers do not need curl or OpenSSL headers.

## Dependency And Packaging Posture

Auth-enabled builds link OpenSSL Crypto for cryptographic operations. The
OpenSSL dependency is private to the implementation from the public ABI
perspective: release verification checks that exported shared-library symbols do
not leak OpenSSL symbol names.

Release archive verification also checks that public package metadata does not
unconditionally expose curl/OpenSSL requirements to consumers of the plain
`lonejson` package. Binary release archives must still include and verify the
auth/curl ABI symbols when built as release artifacts.

The important release checks are:

- build-tree curl ABI symbol checks,
- build-tree JWT/OIDC ABI symbol checks,
- release archive metadata checks,
- release archive dependency-leak checks,
- release archive ABI symbol checks,
- Darwin Mach-O metadata checks for install names and dependency paths.

## Ownership And Memory Model

The C API keeps parse and trust decisions explicit.

- JWT compact parsing returns slices into the caller-provided token.
- Parsed JWK, JWKS, JWT header, JWT claims, OIDC discovery, OAuth2 token
  response, PKCE, callback, JWKS cache, and bearer validation objects own their
  retained strings where documented.
- Every owning object has an init/cleanup pair.
- JWKS cache selection returns pointers into the caller-owned cache. The caller
  must keep the cache alive while selected JWK pointers or successful bearer
  validation results are used.
- OAuth2 and OIDC URL/body builders write into `lonejson_owned_buffer`.
- Size limits are explicit where attacker-controlled JSON or callback data can
  grow.

JWT and JWK parsing necessarily materializes decoded JWT header/claims JSON and
JWK/JWKS key documents into bounded in-memory structures. This is not a
streaming API for arbitrarily large auth documents. The values involved in
normal JWT/JWK/OIDC flows are expected to be small, and the implementation
rejects configured over-size inputs.

## JWT Compact And Base64url

Implemented public APIs:

- `lonejson_base64url_decoded_len`
- `lonejson_base64url_decode`
- `lonejson_jwt_parse_compact`
- short aliases `lj_base64url_decoded_len`, `lj_base64url_decode`,
  `lj_jwt_parse_compact`

`lonejson_jwt_parse_compact` validates compact serialization shape only:

- exactly three compact JWT/JWS segments,
- non-empty header segment,
- non-empty payload segment,
- signature segment may be empty at parse time,
- every segment uses unpadded base64url syntax,
- no `=` padding,
- no `+` or `/`,
- no extra fourth segment.

The result contains caller-owned slices:

- `header`,
- `payload`,
- `signature`,
- `signing_input`.

Parsing a compact token does not decode JSON, validate a signature, validate
claims, validate issuer/audience, or establish trust.

## JWK And JWKS

Implemented public APIs:

- `lonejson_jwk_init`
- `lonejson_jwk_cleanup`
- `lonejson_jwks_init`
- `lonejson_jwks_cleanup`
- `lonejson_jwk_parse_json`
- `lonejson_jwks_parse_json`
- `lonejson_jwks_select`
- short aliases `lj_jwk_*`, `lj_jwks_*`

Implemented JWK fields:

- `kty`
- `kid`
- `alg`
- `use`
- `crv`
- `n`
- `e`
- `x`
- `y`
- `k`

Implemented JWK shape checks:

- `kty` is required.
- RSA keys require valid base64url `n` and `e`.
- EC keys require valid base64url `crv`, `x`, and `y`.
- symmetric keys require valid base64url `k`.
- JWKS documents require a `keys` array.
- JWKS documents with no keys are rejected where used for cache installation.

`lonejson_jwks_select` selects the first key matching all non-NULL filters in
`lonejson_jwk_select_options`:

- `kid`,
- `kty`,
- `alg`,
- `use`.

No trust is established by parsing or selecting a JWK. Selection only chooses a
candidate key.

## JWT Header And Claims Decode

Implemented public APIs:

- `lonejson_jwt_header_init`
- `lonejson_jwt_header_cleanup`
- `lonejson_jwt_claims_init`
- `lonejson_jwt_claims_cleanup`
- `lonejson_jwt_decode_compact`
- short aliases `lj_jwt_header_*`, `lj_jwt_claims_*`,
  `lj_jwt_decode_compact`

Decoded header fields:

- `alg`,
- `kid`,
- `typ`.

Decoded claim fields:

- `iss`,
- `sub`,
- `aud` as string,
- `aud` as array of strings,
- `exp`,
- `nbf`,
- `iat`.

`lonejson_jwt_decode_compact` performs:

- compact serialization validation,
- base64url decoding,
- decoded header JSON size checks,
- decoded claims JSON size checks,
- JSON syntax checks,
- duplicate registered header/claim detection,
- registered claim type checks,
- integer time claim checks.

It does not validate signatures, trust, issuer, audience, clock policy, or
required claims.

Relevant policy limits:

- `max_token_bytes`,
- `max_decoded_header_bytes`,
- `max_decoded_claims_bytes`.

Zero decoded JSON limits use internal defaults.

## JWT Claim Validation

Implemented public APIs:

- `lonejson_jwt_validate_claims`
- short alias `lj_jwt_validate_claims`

Claim validation uses `lonejson_jwt_claim_policy`.

Required trust-policy members:

- accepted algorithms,
- accepted issuers,
- accepted audiences,
- current time,
- non-negative allowed clock skew.

Optional policy members:

- required claims,
- maximum token bytes,
- maximum decoded header bytes,
- maximum decoded claims bytes.

Claim validation enforces:

- `alg` must be present and accepted,
- `alg: none` is rejected,
- issuer must match an accepted issuer,
- string or array audience must include an accepted audience,
- required claims must be present,
- expired tokens fail,
- not-yet-valid tokens fail,
- issued-at values in the future fail beyond configured skew,
- negative clock skew is invalid policy.

This API is a claim trust decision only. It does not validate the JWS
signature. Callers must compose it with signature validation or use the bearer
validation helper.

## JWT Signature Validation

Implemented public APIs:

- `lonejson_jwt_validate_signature`
- short alias `lj_jwt_validate_signature`

Implemented algorithm support:

- `RS256`.

The implementation validates RS256 signatures with OpenSSL using RSA public key
material from a selected JWK.

Signature validation enforces:

- header `alg` is present,
- `alg: none` is rejected,
- JWK `alg`, when present, must match the JWT header algorithm,
- JWK `kid`, when present alongside header `kid`, must match,
- JWK `use`, when present, must be `sig`,
- `RS256` requires `kty: RSA`,
- RSA modulus and exponent must decode and be bounded,
- signature must be present and non-empty,
- cryptographic verification must succeed.

This API is a signature trust decision only. It does not validate issuer,
audience, time claims, or required claims.

## OIDC Discovery

Implemented public APIs:

- `lonejson_oidc_discovery_init`
- `lonejson_oidc_discovery_cleanup`
- `lonejson_oidc_discovery_url`
- `lonejson_oidc_discovery_parse_json`
- `lonejson_oidc_discovery_validate_issuer`
- short aliases `lj_oidc_discovery_*`

Implemented discovery fields:

- `issuer`,
- `authorization_endpoint`,
- `token_endpoint`,
- `jwks_uri`.

`lonejson_oidc_discovery_url` builds the discovery URL for HTTPS issuers.
Path-based issuers follow OpenID Connect discovery placement:

```text
https://host/path -> https://host/.well-known/openid-configuration/path
```

Discovery parsing validates required metadata shape and HTTPS URL fields. It
does not fetch discovery, fetch JWKS, validate tokens, or establish issuer
trust. The caller must pass parsed discovery metadata to
`lonejson_oidc_discovery_validate_issuer` before use.

## OIDC JWKS Cache

Implemented public APIs:

- `lonejson_oidc_jwks_cache_init`
- `lonejson_oidc_jwks_cache_cleanup`
- `lonejson_oidc_jwks_cache_update_json`
- `lonejson_oidc_jwks_cache_is_fresh`
- `lonejson_oidc_jwks_cache_select`
- `lonejson_oidc_jwks_cache_parse_init`
- `lonejson_oidc_jwks_cache_write_callback`
- `lonejson_oidc_jwks_cache_parse_finish`
- `lonejson_oidc_jwks_cache_parse_cleanup`
- short aliases `lj_oidc_jwks_cache_*`

`lonejson_oidc_jwks_cache_policy` supplies:

- expected issuer,
- JWKS URI,
- maximum JWKS bytes,
- current time,
- TTL seconds.

Cache update:

- requires HTTPS issuer and JWKS URI,
- requires positive TTL,
- checks `now + ttl_seconds` overflow,
- enforces bounded JWKS JSON length,
- parses and validates the JWKS document,
- rejects empty JWKS,
- records issuer, URI, fetch time, expiry time, and size limit.

Cache selection:

- requires the cache issuer and URI to match policy,
- requires the cache to be unexpired at `policy->now`,
- selects a JWK using the normal JWK selection filters.

The cache never performs network I/O. Callers may fetch bytes through their own
HTTP code and call `lonejson_oidc_jwks_cache_update_json`, or use
`lonejson_oidc_jwks_cache_parse_*` as a curl write-callback adapter. Retry,
timeouts, TLS policy, proxy policy, and event-loop integration remain
caller-owned.

## OAuth2 Client Credentials

Implemented public APIs:

- `lonejson_oauth2_client_credentials_body`
- `lonejson_oauth2_token_response_init`
- `lonejson_oauth2_token_response_cleanup`
- `lonejson_oauth2_token_response_parse_json`
- short aliases `lj_oauth2_*`

`lonejson_oauth2_client_credentials_body` builds an
`application/x-www-form-urlencoded` body for `client_secret_post` token
endpoint authentication.

Implemented request fields:

- `client_id`,
- `client_secret`,
- `scope`,
- `audience`,
- `resource`,
- `max_body_bytes`.

The helper always sets:

```text
grant_type=client_credentials
```

Then it appends configured fields with form encoding and enforces
`max_body_bytes` or an internal default.

`lonejson_oauth2_token_response_parse_json` parses a bounded successful token
endpoint response.

Implemented response fields:

- `access_token`,
- `token_type`,
- `refresh_token`,
- `scope`,
- `id_token`,
- `error`,
- `error_description`,
- `error_uri`,
- `expires_in`.

Validation rules:

- successful responses require an access token,
- token type must be `Bearer`, case-insensitive,
- provider error responses are rejected as `LONEJSON_STATUS_TYPE_MISMATCH`,
- response JSON is bounded by `max_response_bytes` or an internal default.

HTTP execution is not implemented here. The caller posts the generated body to
the token endpoint and passes the response bytes back to lonejson.

## Authorization Code With PKCE

Implemented public APIs:

- `lonejson_oidc_pkce_init`
- `lonejson_oidc_pkce_cleanup`
- `lonejson_oidc_pkce_challenge`
- `lonejson_oidc_pkce_generate`
- `lonejson_oidc_authorization_url`
- `lonejson_oidc_authorization_callback_init`
- `lonejson_oidc_authorization_callback_cleanup`
- `lonejson_oidc_authorization_callback_parse_query`
- short aliases `lj_oidc_pkce_*`, `lj_oidc_authorization_*`

PKCE support:

- computes S256 challenge from a caller-provided verifier,
- validates verifier length and allowed characters,
- generates random verifiers with OpenSSL `RAND_bytes`,
- uses 32 random bytes by default,
- allows verifier byte counts that produce RFC 7636 verifier lengths of
  43..128 characters,
- base64url encodes without padding.

Authorization URL construction requires:

- HTTPS authorization endpoint,
- client id,
- redirect URI,
- state,
- nonce,
- code challenge.

It emits:

- `response_type=code`,
- `client_id`,
- `redirect_uri`,
- optional `scope`,
- `state`,
- `nonce`,
- `code_challenge`,
- `code_challenge_method=S256`,
- optional `audience`,
- optional `resource`.

The redirect URI is caller-provided and may be a local HTTP callback URI for
desktop/CLI flows. The authorization endpoint itself must be HTTPS.

Callback query parsing:

- accepts a query with or without leading `?`,
- percent-decodes form-style query values,
- validates required expected state,
- rejects provider error callbacks,
- rejects duplicate known fields,
- rejects malformed percent encoding,
- requires a code on success,
- applies `max_query_bytes` or an internal default.

Browser launching, local HTTP listener creation, random callback path
management, token exchange HTTP execution, and ID-token nonce validation are
not implemented in core lonejson.

## Server-Side Bearer Validation

Implemented public APIs:

- `lonejson_auth_failure_string`
- `lonejson_oidc_bearer_validation_init`
- `lonejson_oidc_bearer_validation_cleanup`
- `lonejson_oidc_authorization_bearer_token`
- `lonejson_oidc_validate_bearer_token`
- short aliases `lj_auth_failure_string`,
  `lj_oidc_bearer_validation_*`,
  `lj_oidc_authorization_bearer_token`,
  `lj_oidc_validate_bearer_token`

`lonejson_oidc_authorization_bearer_token` extracts the compact JWT token from
an HTTP `Authorization` header value. It accepts optional leading/trailing HTTP
OWS and case-insensitive `Bearer`, requires a token, and rejects trailing
non-whitespace data.

`lonejson_oidc_validate_bearer_token` composes:

- Authorization Bearer extraction,
- JWT compact parse,
- JWT header and claims decode,
- required `kid`,
- fresh JWKS cache selection,
- signature validation,
- claim policy validation.

Inputs are supplied by `lonejson_oidc_bearer_validation_request`:

- Authorization header value,
- caller-owned JWKS cache,
- JWKS cache policy,
- JWT claim policy.

On success, `lonejson_oidc_bearer_validation` contains:

- `failure == LONEJSON_AUTH_FAILURE_NONE`,
- decoded validated header,
- decoded validated claims,
- selected JWK pointer into the caller-owned JWKS cache.

On failure, the result is classified as one of:

- `missing_credentials`,
- `malformed_token`,
- `cache_unavailable`,
- `key_not_found`,
- `invalid_signature`,
- `expired_token`,
- `not_yet_valid`,
- `issuer_mismatch`,
- `audience_mismatch`,
- `claims_invalid`.

This helper is framework-neutral. It does not read request objects, route
requests, write HTTP responses, refresh the JWKS cache, or own framework
lifetime.

## Lua Facade

The Lua binding exposes the auth surface by calling into the C implementation.
It does not implement independent Lua crypto or protocol logic.

JWT/JWK/JWKS Lua facade:

- `base64url_decode`
- `jwt_parse_compact`
- `jwt_decode_compact`
- `jwt_validate_compact_claims`
- `jwt_validate_compact_signature`
- `jwk_parse_json`
- `jwks_parse_json`
- `jwks_select_json`

OIDC/OAuth2 Lua facade:

- `oidc_discovery_url`
- `oidc_discovery_parse_json`
- `oidc_jwks_cache_select_json`
- `oauth2_client_credentials_body`
- `oauth2_token_response_parse_json`
- `oidc_pkce_challenge`
- `oidc_pkce_generate`
- `oidc_authorization_url`
- `oidc_authorization_callback_parse_query`
- `oidc_validate_bearer_token`

The functions are registered both on the module table and runtime userdata
where applicable.

Lua policy tables map directly to the C policy concepts:

- `accepted_algs`,
- `accepted_issuers`,
- `accepted_audiences`,
- `required_claims`,
- `now`,
- `allowed_clock_skew`,
- `max_token_bytes`,
- `max_decoded_header_bytes`,
- `max_decoded_claims_bytes`.

Lua OIDC cache policy tables map to:

- `issuer`,
- `jwks_uri`,
- `max_jwks_bytes`,
- `now`,
- `ttl_seconds`.

Lua failure returns follow existing binding conventions. Most auth helpers
return the normal result on success and `nil, error` on failure. Bearer
validation returns `nil, error, failure` on classified auth denial.

## Security Invariants Covered

Implemented and tested invariants include:

- JWT parse does not imply trust.
- JWT signature validation does not imply claim trust.
- JWT claim validation does not imply signature trust.
- `alg: none` is rejected by validation.
- Unsupported algorithms are rejected.
- RS256 requires RSA JWK material.
- JWK `alg`, when present, must match the JWT header algorithm.
- JWK `use`, when present, must be `sig` for signature validation.
- JWK `kid`, when present with header `kid`, must match.
- Server-side bearer validation requires a header `kid` and does not silently
  fall back to an arbitrary key.
- Unknown issuer fails closed.
- Unknown audience fails closed.
- Expired token fails closed.
- Not-yet-valid token fails closed.
- Issued-at in the future fails beyond configured skew.
- Duplicate registered JWT header/claim fields are rejected.
- Malformed registered claims are rejected deterministically.
- Oversized decoded JWT header/claims are rejected.
- Oversized JWKS JSON is rejected.
- Empty JWKS cache installation is rejected.
- Expired or issuer/URI-mismatched JWKS cache selection fails.
- Token endpoint provider error JSON is rejected.
- OAuth2 token response token type must be Bearer.
- Authorization callback provider error is rejected.
- Authorization callback state mismatch is rejected.
- Authorization callback duplicate fields are rejected.
- Authorization callback query size is bounded.

## Verification

The implementation is covered by:

- C unit and regression tests in `tests/test_jwt.inc.h`,
- Lua facade tests in `tests/test_lua.lua`,
- ABI export checks in `scripts/check_jwt_abi_symbols.sh`,
- release archive verifier checks in `tests/test_release_archive_verify.sh`,
- fuzzer coverage in `fuzz/fuzz_jwt.c`,
- makefile fuzz target registration checks,
- c.pkt.systems dependency route tests,
- curl/OpenSSL dependency leakage checks,
- release metadata checks.

The standard auth verification path used during implementation was:

```sh
cmake --build --preset host-curl
ctest --preset host-curl
cmake --build --preset debug
ctest --preset debug
make fuzz-smoke
```

At the time this document was written, those gates passed on the feature branch
containing the auth implementation.

## Known Gaps And Non-Implemented Work

The following are deliberate gaps or future work, not hidden behavior.

- No HTTP fetch helper for OIDC discovery.
- No HTTP fetch helper for JWKS retrieval.
- No automatic JWKS cache refresh.
- No retry, timeout, proxy, redirect, or TLS policy helper.
- No credential storage helper.
- No token endpoint HTTP POST helper.
- No refresh-token exchange body builder.
- No authorization-code token exchange body builder.
- No ID-token nonce validation helper.
- No complete browser-launch helper.
- No localhost HTTP listener.
- No random callback-path session object.
- No Kore-specific or Vectis-specific middleware API.
- No HTTP response writer for auth failures.
- No request-router integration.
- No ES256, PS256, EdDSA, HS256, or `none` validation support.
- No x5c/x5t certificate-chain validation.
- No `crit` JOSE header handling.
- No `azp` validation.
- No `scope` or `scp` authorization helper.
- No multiple-audience policy mode beyond accepted-audience membership.
- No JWK `key_ops` enforcement.
- No JWKS cache eviction policy beyond one explicit cache object.
- No concurrency synchronization inside the JWKS cache object.
- No full OAuth2/OIDC provider abstraction.
- No token introspection endpoint support.
- No revocation endpoint support.
- No device authorization flow.
- No userinfo endpoint helper.
- No encrypted JWT/JWE support.

Most of these omissions are intentional because lonejson is not meant to own
application flow control. The core value is bounded parsing, explicit
validation, structured failure classification, and facades that are easy for
framework-specific code to compose.

## Recommended Composition Patterns

Machine-to-machine client credentials:

1. Fetch and validate OIDC discovery metadata.
2. Build a client-credentials body with
   `lonejson_oauth2_client_credentials_body`.
3. POST the body using caller-owned HTTP code.
4. Parse the token response with
   `lonejson_oauth2_token_response_parse_json`.
5. Treat returned token material as secret credential material.

CLI or desktop authorization-code with PKCE:

1. Fetch and validate OIDC discovery metadata.
2. Generate PKCE with `lonejson_oidc_pkce_generate`.
3. Build the authorization URL with `lonejson_oidc_authorization_url`.
4. Open the browser using caller-owned platform code.
5. Receive callback query through caller-owned local HTTP code.
6. Parse callback with `lonejson_oidc_authorization_callback_parse_query`.
7. Exchange the code with caller-owned HTTP code.
8. Parse token response with `lonejson_oauth2_token_response_parse_json`.
9. Validate any ID token with the JWT/JWKS validation primitives.

Server-side bearer validation:

1. Fetch and validate OIDC discovery metadata.
2. Fetch JWKS with caller-owned HTTP code.
3. Install JWKS into `lonejson_oidc_jwks_cache`.
4. For each request, call `lonejson_oidc_validate_bearer_token` before
   application logic.
5. Map `lonejson_auth_failure` to framework-specific HTTP responses.
6. Refresh JWKS cache using caller-owned policy when validation fails due to
   cache availability or key miss.

## Release Notes For Consumers

Consumers that do not define the auth feature gates see no auth API and should
not need OpenSSL or curl headers.

Consumers that define `LONEJSON_WITH_JWT` must build against a library compiled
with JWT/OpenSSL support.

Consumers that define `LONEJSON_WITH_OIDC` must build against a library
compiled with OIDC, JWT, OpenSSL, and curl support.

Parsing JWTs is never trust. Trust is established only by explicit validation:

- signature validation with a selected key,
- claim validation with an explicit policy,
- or composed bearer validation against a fresh JWKS cache and claim policy.
