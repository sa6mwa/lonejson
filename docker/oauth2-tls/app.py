import http.client
import ssl
from http.server import BaseHTTPRequestHandler, HTTPServer


class Proxy(BaseHTTPRequestHandler):
    server_version = "lonejson-oauth2-tls/0.1"

    def do_GET(self):
        self._proxy()

    def do_POST(self):
        self._proxy()

    def _proxy(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length) if length else None
        path = self.path
        if path == "/.well-known/openid-configuration/default":
            path = "/default/.well-known/openid-configuration"
        headers = {
            key: value
            for key, value in self.headers.items()
            if key.lower() not in {"connection", "content-length"}
        }
        headers["Host"] = self.headers.get("Host", "localhost:18443")
        headers["X-Forwarded-Proto"] = "https"
        if body is not None:
            headers["Content-Length"] = str(len(body))

        conn = http.client.HTTPConnection("oauth2", 8080, timeout=10)
        conn.request(self.command, path, body=body, headers=headers)
        response = conn.getresponse()
        payload = response.read()

        self.send_response(response.status, response.reason)
        for key, value in response.getheaders():
            if key.lower() not in {"connection", "transfer-encoding"}:
                self.send_header(key, value)
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)
        conn.close()

    def log_message(self, fmt, *args):
        print(
            "%s - - [%s] %s"
            % (self.address_string(), self.log_date_time_string(), fmt % args)
        )


if __name__ == "__main__":
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain("/certs/server.crt", "/certs/server.key")
    server = HTTPServer(("0.0.0.0", 9443), Proxy)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    server.serve_forever()
