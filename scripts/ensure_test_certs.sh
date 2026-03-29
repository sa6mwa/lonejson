#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cert_dir="${repo_root}/docker/nginx/certs"
crt="${cert_dir}/server.crt"
key="${cert_dir}/server.key"
cfg="${cert_dir}/openssl.cnf"

mkdir -p "${cert_dir}"

if [[ -s "${crt}" && -s "${key}" ]]; then
  exit 0
fi

cat > "${cfg}" <<'EOF'
[req]
distinguished_name = dn
x509_extensions = v3_req
prompt = no

[dn]
CN = localhost

[v3_req]
subjectAltName = @alt_names
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth

[alt_names]
DNS.1 = localhost
IP.1 = 127.0.0.1
EOF

openssl req \
  -x509 \
  -nodes \
  -newkey rsa:2048 \
  -days 3650 \
  -keyout "${key}" \
  -out "${crt}" \
  -config "${cfg}"
