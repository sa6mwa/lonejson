import json
from http.server import BaseHTTPRequestHandler, HTTPServer


class Handler(BaseHTTPRequestHandler):
    server_version = "lonejson-sink/0.1"

    def do_POST(self):
        self._handle_ingest()

    def do_PUT(self):
        self._handle_ingest()

    def _handle_ingest(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length) if length else b""
        status = 200
        try:
            parsed = json.loads(body.decode("utf-8") if body else "null")
            response = {
                "method": self.command,
                "ok": True,
                "received": parsed,
            }
        except json.JSONDecodeError as exc:
            status = 400
            response = {
                "method": self.command,
                "ok": False,
                "error": str(exc),
            }

        payload = json.dumps(response).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, fmt, *args):
        print("%s - - [%s] %s" % (self.address_string(), self.log_date_time_string(), fmt % args))


if __name__ == "__main__":
    HTTPServer(("0.0.0.0", 9000), Handler).serve_forever()
