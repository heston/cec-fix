"""Exercise the real LAN implementation against a local fake projector."""
import pathlib
import os
import socket
import subprocess
import threading
import unittest

PROBE = pathlib.Path(os.environ.get(
    "CEC_LAN_PROBE", pathlib.Path(__file__).resolve().parents[1] / "build" / "lan-probe"
))


class ProjectorTests(unittest.TestCase):
    def run_projector(self, stall_at):
        with socket.socket() as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind(("127.0.0.1", 20554))
            server.listen()
            server.settimeout(3)
            stop = threading.Event()
            errors = []

            def serve():
                try:
                    with server.accept()[0] as peer:
                        peer.settimeout(3)
                        if stall_at == "greeting":
                            stop.wait(9)
                            return
                        peer.sendall(b"PJ_OK")
                        self.assertEqual(peer.recv(5), b"PJREQ")
                        if stall_at == "handshake":
                            stop.wait(9)
                            return
                        peer.sendall(b"PJACK")
                        self.assertEqual(peer.recv(6), b"\x3f\x89\x01PW\x0a")
                        if stall_at == "response":
                            stop.wait(9)
                            return
                        peer.sendall(b"\x06\x89\x01PW\x0a")
                except Exception as error:
                    errors.append(error)

            worker = threading.Thread(target=serve)
            worker.start()
            try:
                result = subprocess.run([str(PROBE)], capture_output=True, text=True, timeout=8)
            finally:
                stop.set()
                worker.join()
            if errors:
                raise errors[0]
            return result

    def test_stalled_responses_time_out(self):
        for stage in ("greeting", "handshake", "response"):
            with self.subTest(stage=stage):
                result = self.run_projector(stage)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("result=-4", result.stdout)

    def test_successful_exchange(self):
        result = self.run_projector(None)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("result=6", result.stdout)


if __name__ == "__main__":
    unittest.main()
