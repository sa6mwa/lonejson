"""Exercise the verifying fixture over HTTP, including rejection paths."""
import http.client
from pathlib import Path
import sys
import threading
import unittest
from http.server import HTTPServer

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "docker" / "sink"))
from app import Handler  # noqa: E402
from rewind import RewindFixture  # noqa: E402


class QuietHandler(Handler):
    def log_message(self, *_args):
        pass


class RewindFixtureTests(unittest.TestCase):
    def setUp(self):
        QuietHandler.rewind = RewindFixture()
        self.server = HTTPServer(("127.0.0.1", 0), QuietHandler)
        self.thread = threading.Thread(target=self.server.serve_forever)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.thread.join()
        self.server.server_close()

    def request(self, method, path, body=None, chunked=False):
        connection = http.client.HTTPConnection(*self.server.server_address, timeout=3)
        if chunked:
            connection.request(method, path, [body], encode_chunked=True)
        else:
            connection.request(method, path, body)
        response = connection.getresponse()
        result = response.status, dict(response.getheaders()), response.read()
        connection.close()
        return result

    def test_repeated_replay(self):
        for framing in ("known", "chunked"):
            for hop in range(3):
                status, headers, _ = self.request(
                    "PATCH", f"/rewind/{framing}/PATCH/308/3/{framing}/{hop}",
                    b'{"payload":"aaa"}', framing == "chunked")
                self.assertEqual(status, 308 if hop < 2 else 200)
                self.assertEqual(headers["X-Lonejson-Rewind-Verified"], str(hop))
            status, _, body = self.request("GET", f"/rewind-result/{framing}")
            self.assertEqual((status, body), (200, b"3 3"))

    def test_rejects_corruption(self):
        status, headers, _ = self.request(
            "PUT", "/rewind/corrupt/PUT/307/3/known/0", b'{"payload":"aab"}')
        self.assertEqual(status, 400)
        self.assertNotIn("X-Lonejson-Rewind-Verified", headers)
        self.assertEqual(self.request("GET", "/rewind-result/corrupt")[2], b"1 0")

    def test_rejects_method_change(self):
        self.assertEqual(self.request(
            "POST", "/rewind/method/PUT/307/3/known/0", b'{"payload":"aaa"}')[0], 400)

    def test_rejects_wrong_framing(self):
        self.assertEqual(self.request(
            "PUT", "/rewind/framing/PUT/307/3/chunked/0", b'{"payload":"aaa"}')[0], 400)

    def test_rejects_replay_order(self):
        self.assertEqual(self.request(
            "PUT", "/rewind/order/PUT/307/3/known/1", b'{"payload":"aaa"}')[0], 400)

    def test_result_missing_or_consumed(self):
        self.assertEqual(self.request("GET", "/rewind-result/missing")[0], 404)
        self.request("PUT", "/rewind/result/PUT/307/3/known/0", b'{"payload":"aaa"}')
        self.assertEqual(self.request("GET", "/rewind-result/result")[2], b"1 1")
        self.assertEqual(self.request("GET", "/rewind-result/result")[0], 404)


if __name__ == "__main__":
    unittest.main()
