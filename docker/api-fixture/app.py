import json
from http.server import BaseHTTPRequestHandler, HTTPServer


class Handler(BaseHTTPRequestHandler):
    server_version = "lonejson-api-fixture/0.1"

    def do_GET(self):
        if self.path == "/health":
            self._send_json(200, {"ok": True, "service": "api-fixture"})
            return
        if self.path == "/protected":
            authorization = self.headers.get("Authorization")
            if not authorization:
                self._send_json(401, {"ok": False, "error": "missing bearer"})
                return
            self._send_json(
                200,
                {
                    "ok": True,
                    "handler": "protected",
                    "authorization_prefix": authorization[:16],
                },
            )
            return
        self._send_json(404, {"ok": False, "error": "not found"})

    def _send_json(self, status, value):
        payload = json.dumps(value, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, fmt, *args):
        print(
            "%s - - [%s] %s"
            % (self.address_string(), self.log_date_time_string(), fmt % args)
        )


if __name__ == "__main__":
    HTTPServer(("0.0.0.0", 9001), Handler).serve_forever()
