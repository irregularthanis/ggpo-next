"""Wire-format contract: exact bytes, shared vectors, and rejection rules."""

import json
import unittest
from pathlib import Path

from matchmaking import protocol as p

VECTORS = Path(__file__).resolve().parents[1] / "protocol_vectors.json"

CONSTRUCTORS = {
    "REGISTER": p.Register, "WAITING": p.Waiting, "MATCH": p.Match,
    "PUNCH": p.Punch, "HELLO": p.Hello, "ERROR": p.Error,
}


def load_vectors():
    return json.loads(VECTORS.read_text(encoding="utf-8"))["vectors"]


class SharedVectors(unittest.TestCase):
    """The same file is decoded by the C++ client tests."""

    def test_every_vector_encodes_and_decodes_exactly(self):
        vectors = load_vectors()
        self.assertEqual({v["type"] for v in vectors}, set(CONSTRUCTORS))
        for vector in vectors:
            with self.subTest(vector["name"]):
                message = CONSTRUCTORS[vector["type"]](**vector["fields"])
                wire = bytes.fromhex(vector["hex"])
                self.assertEqual(p.encode(message), wire)
                self.assertEqual(p.decode(wire), message)


class Sizes(unittest.TestCase):
    def test_server_replies_are_never_larger_than_register(self):
        replies = [
            p.Waiting(1, 1),
            p.Match(1, 2, 1, p.PLATFORM_ANDROID, "203.0.113.9", 65535),
            p.Error(1, p.ERROR_SERVER_FULL),
        ]
        register = p.encode(p.Register(1, p.PLATFORM_LINUX, 1))
        self.assertEqual(len(register), p.REGISTER_SIZE)
        for reply in replies:
            self.assertLessEqual(len(p.encode(reply)), len(register))


class Rejection(unittest.TestCase):
    def setUp(self):
        self.register = p.encode(p.Register(7, p.PLATFORM_WINDOWS, 1))

    def assertRejected(self, data):
        with self.assertRaises(p.ProtocolError):
            p.decode(data)

    def test_short_and_empty(self):
        self.assertRejected(b"")
        self.assertRejected(self.register[:7])

    def test_bad_magic(self):
        self.assertRejected(b"GGPO" + self.register[4:])

    def test_other_version(self):
        self.assertRejected(self.register[:4] + bytes([2]) + self.register[5:])

    def test_nonzero_reserved_header(self):
        self.assertRejected(self.register[:6] + b"\x01\x00" + self.register[8:])

    def test_truncated_and_trailing(self):
        self.assertRejected(self.register[:-1])
        self.assertRejected(self.register + b"\x00")
        hello = p.encode(p.Hello(1, p.PLATFORM_LINUX))
        self.assertRejected(hello[:-1])
        self.assertRejected(hello + b"\x00")

    def test_nonzero_register_padding(self):
        self.assertRejected(self.register[:-1] + b"\x01")

    def test_unknown_and_reserved_types(self):
        for message_type in (0, p.RELAY, 200):
            data = bytearray(self.register)
            data[5] = message_type
            self.assertRejected(bytes(data))

    def test_invalid_player_number(self):
        match = bytearray(p.encode(p.Match(1, 2, 1, 0, "198.51.100.1", 7000)))
        match[24] = 3
        self.assertRejected(bytes(match))


if __name__ == "__main__":
    unittest.main()
