#!/usr/bin/env python3
"""Render only lifecycle-owned paths, names, and validated host ports."""
import hashlib
import json
import os
from pathlib import Path
import re
import sys


def render(check_only=False):
    root = Path(__file__).resolve().parent.parent
    generated = root / "build" / "devenv"
    pod = "lonejson-e2e-" + hashlib.sha256(os.fsencode(root)).hexdigest()[:12]
    values = {"REPO_ROOT": str(root), "DEVENV_ROOT": str(generated), "POD_NAME": pod}
    ports = {}
    for token, variable, default in (
        ("OAUTH2_PORT", "LONEJSON_OAUTH2_E2E_PORT", 8090),
        ("OIDC_PORT", "LONEJSON_OIDC_E2E_PORT", 18443),
        ("API_PORT", "LONEJSON_API_FIXTURE_E2E_PORT", 18080),
        ("NGINX_HTTP_PORT", "LONEJSON_NGINX_HTTP_E2E_PORT", 8080),
        ("NGINX_HTTPS_PORT", "LONEJSON_NGINX_HTTPS_E2E_PORT", 8443),
    ):
        raw = os.environ.get(variable, str(default))
        if not raw.isascii() or not raw.isdecimal() or not 1024 <= int(raw) <= 65535:
            raise ValueError(f"{variable} must be a port between 1024 and 65535")
        ports[token] = int(raw)
        values[token] = str(int(raw))
    if len(set(ports.values())) != len(ports):
        raise ValueError("published LONEJSON_*_E2E_PORT values must be distinct")
    template = (root / "devenv.yaml.in").read_text()

    def substitute(match):
        # YAML accepts Unicode scalar escapes, but rejects JSON's UTF-16
        # surrogate pairs. Keep quoting/control escapes for every other byte.
        return "".join(
            f"\\U{ord(char):08x}" if ord(char) > 0xFFFF
            else json.dumps(char, ensure_ascii=True)[1:-1]
            for char in values[match[1]]
        )

    output = re.sub(r"@@([A-Z0-9_]+)@@", substitute, template)
    if "@@" in output:
        raise ValueError("unresolved devenv template placeholder")
    if check_only:
        return
    generated.mkdir(parents=True, exist_ok=True)
    manifest = generated / "devenv.yaml"
    # Fail before replacing the manifest if validation failed; down uses these
    # exact bytes even when callers change their port environment afterward.
    temporary = generated / "devenv.yaml.tmp"
    with temporary.open("w") as handle:
        os.chmod(temporary, 0o600)
        handle.write(output)
    temporary.replace(manifest)
    print(pod)


if __name__ == "__main__":
    if sys.argv[1:] not in ([], ["--check"]):
        raise SystemExit("usage: render_devenv.py [--check]")
    try:
        render(check_only=sys.argv[1:] == ["--check"])
    except (ValueError, KeyError) as error:
        raise SystemExit(str(error)) from error
