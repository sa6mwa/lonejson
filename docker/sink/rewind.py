"""Upload-replay fixture shared by the HTTP and HTTPS nginx endpoints."""


class RewindFixture:
    def __init__(self):
        self.cases = {}

    def handle(self, handler):
        parts = handler.path.strip("/").split("/")
        if parts[0] == "rewind-result" and len(parts) == 2:
            state = self.cases.pop(parts[1], None)
            if handler.command != "GET" or state is None:
                self.reply(handler, 404, b"unknown rewind case\n")
            else:
                self.reply(handler, 200, f"{state[0]} {state[1]}".encode())
            return True
        if parts[0] != "rewind":
            return False
        handler.connection.settimeout(10)
        try:
            _, case, method, code, size, framing, hop = parts
            code, size, hop = int(code), int(size), int(hop)
            if code not in (307, 308) or size not in (3, 262144) or hop not in (0, 1, 2):
                raise ValueError("invalid rewind case parameters")
            state = self.cases.setdefault(case, [0, 0])
            state[0] += 1
            if handler.command != method or method not in ("POST", "PUT", "PATCH"):
                raise ValueError("upload method changed")
            if hop != state[1]:
                raise ValueError("unexpected replay order")
            expected = b'{"payload":"' + b"a" * size + b'"}'
            chunked = handler.headers.get("Transfer-Encoding", "").lower() == "chunked"
            # nginx can reframe a small preread request even with buffering off.
            # This header is overwritten by nginx from its client-facing headers.
            original = handler.headers.get("X-Lonejson-Upload-Framing")
            if original is None:
                original = (handler.headers.get("Transfer-Encoding", "") + "|" +
                            handler.headers.get("Content-Length", ""))
            transfer, content_length = original.split("|", 1)
            if framing == "chunked":
                if transfer.lower() != "chunked" or content_length:
                    raise ValueError("upload framing changed")
            elif framing == "known":
                if transfer or content_length != str(len(expected)):
                    raise ValueError("upload framing changed")
            else:
                raise ValueError("invalid framing")
            body = self.read_body(handler, len(expected), chunked)
            if body != expected:
                raise ValueError("upload body differs from expected bytes")
            state[1] += 1
            handler.send_response(code if hop < 2 else 200)
            handler.send_header("X-Lonejson-Rewind-Verified", str(hop))
            if hop < 2:
                handler.send_header("Location", "/" + "/".join(parts[:-1] + [str(hop + 1)]))
            handler.send_header("Content-Length", "0")
            handler.send_header("Connection", "close")
            handler.end_headers()
            handler.close_connection = True
        except (ValueError, OSError) as exc:
            self.reply(handler, 400, f"rewind fixture: {exc}\n".encode())
        return True

    @staticmethod
    def read_body(handler, limit, chunked):
        # Materialization is explicit and bounded in this verifying test server.
        body = bytearray()
        if not chunked:
            length = int(handler.headers.get("Content-Length", "-1"))
            if length != limit:
                raise ValueError("unexpected Content-Length")
            body.extend(handler.rfile.read(length))
        else:
            while True:
                line = handler.rfile.readline(128)
                length = int(line.strip(), 16)
                if length < 0 or length > limit - len(body):
                    raise ValueError("upload exceeds expected length")
                if length == 0:
                    if handler.rfile.readline(128) != b"\r\n":
                        raise ValueError("unexpected chunk trailers")
                    break
                chunk = handler.rfile.read(length)
                if len(chunk) != length or handler.rfile.read(2) != b"\r\n":
                    raise ValueError("truncated upload chunk")
                body.extend(chunk)
        return body

    @staticmethod
    def reply(handler, status, body):
        handler.send_response(status)
        handler.send_header("Content-Length", str(len(body)))
        handler.send_header("Connection", "close")
        handler.end_headers()
        handler.wfile.write(body)
        handler.close_connection = True
