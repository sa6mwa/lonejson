#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
python3 - "$repo_root" <<'PY'
import hashlib
import http.server
import pathlib
import subprocess
import sys
import tempfile
import threading

root = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='dependency-cache-contract.', dir=root / 'build') as temporary:
    work = pathlib.Path(temporary)
    cache = work / 'cache'
    payload = b'verified immutable synthetic archive\n'
    digest = hashlib.sha256(payload).hexdigest()
    requests = []

    class Origin(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            requests.append(self.path)
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b'corrupt' if self.path.startswith('/bad') else payload)

        def do_HEAD(self):
            requests.append('HEAD ' + self.path)
            self.send_response(200)
            self.end_headers()

        def log_message(self, *args):
            pass

    origin = http.server.HTTPServer(('127.0.0.1', 0), Origin)
    worker = threading.Thread(target=origin.serve_forever, daemon=True)
    worker.start()
    url = f'http://127.0.0.1:{origin.server_port}'

    def acquire(name, route, expected=digest, success=True, source='source-one'):
        source_dir = work / source
        source_dir.mkdir(exist_ok=True)
        script = source_dir / 'acquire.cmake'
        script.write_text(f'''cmake_minimum_required(VERSION 3.21)
include("{root}/cmake/CpktDependencyCache.cmake")
cpkt_acquire_verified_archive(COMPONENT fixture URL "{url}/{route}"
 SHA256 "{expected}" ARCHIVE_NAME "{name}" RETRIES 2 OUTPUT_VARIABLE archive)
file(SHA256 "${{archive}}" actual)
if(NOT actual STREQUAL "{expected}")
 message(FATAL_ERROR "unverified archive returned")
endif()
''')
        result = subprocess.run(['cmake', f'-DCPKT_DEPENDENCY_CACHE={cache}', '-P', str(script)],
                                capture_output=True, text=True)
        assert (result.returncode == 0) == success, result.stdout + result.stderr
        return result

    try:
        acquire('first.tar.gz', 'first')
        assert requests == ['/first'], requests
        published = cache / 'archives' / 'sha256' / digest
        assert (published / 'first.tar.gz').read_bytes() == payload
        # A renamed asset at a different URL from a fresh source root is a
        # digest hit and cannot probe or contact the origin.
        requests.clear()
        acquire('renamed.zip', 'different-url', source='source-two')
        assert requests == [], requests
        (published / 'renamed.zip').write_bytes(b'corrupt cache entry')
        acquire('renamed.zip', 'different-url', source='source-three')
        assert requests == [], requests
        assert (published / 'renamed.zip').read_bytes() == payload
        # Unpublished partial files with valid bytes must not be treated as hits.
        for file in published.iterdir():
            file.unlink()
        for name in ['.hidden.tmp', 'transfer.tmp.123', 'transfer.part-123', 'transfer.tmp']:
            (published / name).write_bytes(payload)
        acquire('complete.tar.gz', 'fresh')
        assert requests == ['/fresh'], requests
        assert (published / 'complete.tar.gz').read_bytes() == payload
        # Hash failure is caught by the helper: both retries run and each
        # owned temporary file is removed without publishing corrupt bytes.
        requests.clear()
        missing_digest = hashlib.sha256(b'not served by origin').hexdigest()
        result = acquire('bad.tar.gz', 'bad', expected=missing_digest, success=False)
        assert requests == ['/bad', '/bad'], requests
        assert 'SHA256 mismatch' in result.stderr
        assert not list((cache / 'archives' / 'sha256' / missing_digest).iterdir())
    finally:
        origin.shutdown()
        origin.server_close()
        worker.join()
print('PASS dependency archive digest reuse, zero requests, partial rejection, retry cleanup')
PY
