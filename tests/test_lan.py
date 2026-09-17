"""Exercise the real LAN implementation against a local fake projector."""
import os
import pathlib
import socket
import subprocess
import threading
import unittest

PROBE = pathlib.Path(os.environ.get(
    "CEC_LAN_PROBE", pathlib.Path(__file__).resolve().parents[1] / "build" / "lan-probe"
))
ACK = b"\x06\x89\x01PW\x0a"
STATUS = b"\x40\x89\x01PW1\x0a"


def receive_exact(peer, length):
    data = b""
    while len(data) < length:
        chunk = peer.recv(length - len(data))
        if not chunk:
            raise AssertionError("Client closed before sending its complete request")
        data += chunk
    return data


class ProjectorTests(unittest.TestCase):
    def run_projector(self, stall_at=None, split=None, bytewise=False,
                      response=ACK + STATUS, close_response=False,
                      trickle=False, mode="query", greeting=b"PJ_OK", handshake=b"PJACK"):
        with socket.socket() as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind(("127.0.0.1", 20554))
            server.listen()
            server.settimeout(3)
            stop = threading.Event()
            errors = []

            def transmit(peer, data):
                chunks = [bytes([value]) for value in data] if bytewise else [data]
                for chunk in chunks:
                    peer.sendall(chunk)
                    if bytewise and stop.wait(0.02):
                        break

            def serve():
                try:
                    with server.accept()[0] as peer:
                        peer.settimeout(3)
                        if stall_at == "greeting":
                            stop.wait(9)
                            return
                        transmit(peer, greeting)
                        if greeting != b"PJ_OK":
                            return
                        self.assertEqual(receive_exact(peer, 5), b"PJREQ")
                        if stall_at == "handshake":
                            stop.wait(9)
                            return
                        transmit(peer, handshake)
                        if handshake != b"PJACK":
                            return
                        commands = {
                            "on": b"\x21\x89\x01PW1\x0a",
                            "off": b"\x21\x89\x01PW0\x0a",
                            "null": b"\x21\x89\x01\x00\x00\x0a",
                        }
                        command = commands.get(mode, b"\x3f\x89\x01PW\x0a")
                        self.assertEqual(receive_exact(peer, len(command)), command)
                        if stall_at == "response":
                            stop.wait(9)
                            return
                        if trickle:
                            for value in response:
                                peer.sendall(bytes([value]))
                                if stop.wait(0.6):
                                    return
                        elif split is not None:
                            peer.sendall(response[:split])
                            stop.wait(0.05)
                            peer.sendall(response[split:])
                        else:
                            transmit(peer, response)
                        if not close_response:
                            stop.wait(9)
                except (BrokenPipeError, ConnectionResetError):
                    if not trickle:
                        errors.append(AssertionError("Client closed during fragmented reply"))
                except Exception as error:
                    errors.append(error)

            worker = threading.Thread(target=serve)
            worker.start()
            try:
                result = subprocess.run([str(PROBE), mode], capture_output=True, text=True, timeout=8)
            finally:
                stop.set()
                worker.join()
            if errors:
                raise errors[0]
            return result

    def assert_result(self, result, code):
        self.assertEqual(result.returncode, 1 if code < 0 else 0, result.stdout + result.stderr)
        self.assertIn(f"result={code}\n", result.stdout)

    def test_stalled_responses_time_out(self):
        for stage in ("greeting", "handshake", "response"):
            with self.subTest(stage=stage):
                result = self.run_projector(stall_at=stage)
                self.assert_result(result, -4)
                self.assertIn("timed out", result.stdout)

    def test_successful_exchange(self):
        self.assert_result(self.run_projector(), 13)

    def test_response_split_at_every_boundary(self):
        for split in range(1, 13):
            with self.subTest(split=split):
                self.assert_result(self.run_projector(split=split), 13)

    def test_bytewise_handshake_and_status(self):
        self.assert_result(self.run_projector(bytewise=True, mode="status"), 1)

    def test_all_power_states(self):
        for state in range(5):
            with self.subTest(state=state):
                response = ACK + b"\x40\x89\x01PW" + bytes([ord('0') + state]) + b"\x0a"
                self.assert_result(self.run_projector(response=response, mode="status"), state)

    def test_operating_commands_need_only_ack(self):
        for mode, ack in (("on", ACK), ("off", ACK), ("null", b"\x06\x89\x01\x00\x00\x0a")):
            with self.subTest(mode=mode):
                self.assert_result(self.run_projector(mode=mode, response=ack, bytewise=True), 6)

    def test_incomplete_reply_timeout_and_eof_are_distinct(self):
        result = self.run_projector(response=ACK + STATUS[:2])
        self.assert_result(result, -4)
        self.assertIn("timed out", result.stdout)
        result = self.run_projector(response=ACK + STATUS[:2], close_response=True)
        self.assert_result(result, -4)
        self.assertIn("Truncated", result.stdout)

    def test_trickle_does_not_extend_response_deadline(self):
        result = self.run_projector(trickle=True)
        self.assert_result(result, -4)
        self.assertIn("timed out", result.stdout)

    def test_malformed_frames(self):
        for response in (b"\x06\x89\x01XX\x0a" + STATUS, ACK + b"\x40\x89\x01XX1\x0a",
                         ACK + STATUS[:-1] + b"X"):
            with self.subTest(response=response):
                result = self.run_projector(response=response)
                self.assert_result(result, -5)
                self.assertIn("Malformed", result.stdout)

    def test_unknown_status_is_not_a_framing_error(self):
        result = self.run_projector(response=ACK + b"\x40\x89\x01PW9\x0a", mode="status")
        self.assert_result(result, -1)
        self.assertIn("Unknown power status", result.stdout)
        self.assertNotIn("Malformed", result.stdout)

    def test_truncated_handshake(self):
        for args in ({"greeting": b"PJ_"}, {"handshake": b"PJA"}):
            with self.subTest(args=args):
                result = self.run_projector(**args)
                self.assert_result(result, -4)
                self.assertIn("Truncated", result.stdout)

    def test_rejected_handshake(self):
        for args in ({"greeting": b"PJ_NG"}, {"handshake": b"PJNAK"}):
            with self.subTest(args=args):
                self.assert_result(self.run_projector(**args), -5)


if __name__ == "__main__":
    unittest.main()
