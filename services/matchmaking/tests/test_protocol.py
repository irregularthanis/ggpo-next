"""Wire-format contract: exact bytes, shared vectors, and rejection rules."""

import json
import unittest
from pathlib import Path

from matchmaking import protocol as p

VECTORS = Path(__file__).resolve().parents[1] / "protocol_vectors.json"

CONSTRUCTORS = {
    "JOIN": p.Join, "WAITING": p.Waiting, "MATCH": p.Match, "BIND": p.Bind,
    "BOUND": p.Bound, "HELLO": p.Hello, "ERROR": p.Error,
}


def load_vectors():
    return json.loads(VECTORS.read_text(encoding="utf-8"))


def build(vector):
    fields = dict(vector["fields"])
    for name in ("relay_ports", "platforms"):
        if name in fields:
            fields[name] = tuple(fields[name])
    return CONSTRUCTORS[vector["type"]](**fields)


class SharedVectors(unittest.TestCase):
    """The same file is decoded by the C++ client tests."""

    def test_vectors_are_for_this_version(self):
        self.assertEqual(load_vectors()["version"], p.VERSION)

    def test_every_vector_encodes_and_decodes_exactly(self):
        vectors = load_vectors()["vectors"]
        self.assertEqual({v["type"] for v in vectors}, set(CONSTRUCTORS))
        for vector in vectors:
            with self.subTest(vector["name"]):
                message = build(vector)
                wire = bytes.fromhex(vector["hex"])
                self.assertEqual(p.encode(message), wire)
                self.assertEqual(p.decode(wire), message)


class Sizes(unittest.TestCase):
    def test_replies_are_never_larger_than_their_requests(self):
        join = p.encode(p.Join(1, p.PLATFORM_LINUX, 4, "FRIENDS"))
        self.assertEqual(len(join), p.JOIN_SIZE)
        four = p.Match(1, 2, 3, (7101, 7102, 7103, 7104), (1, 2, 3, 0))
        for reply in (p.Waiting(1, 3, 4), four, p.Error(1, p.ERROR_SERVER_FULL)):
            self.assertLessEqual(len(p.encode(reply)), len(join))
        self.assertEqual(len(p.encode(p.Bound(9, 2))), len(p.encode(p.Bind(9, 2))))

    def test_match_size_is_fixed_for_any_player_count(self):
        two = p.encode(p.Match(1, 2, 1, (7101, 7102), (1, 2)))
        four = p.encode(p.Match(1, 2, 1, (7101, 7102, 7103, 7104), (1, 2, 3, 0)))
        self.assertEqual(len(two), p.MATCH_SIZE)
        self.assertEqual(len(four), p.MATCH_SIZE)


class LobbyCodes(unittest.TestCase):
    def test_codes_are_case_insensitive_and_padded(self):
        self.assertEqual(p.normalize_lobby("k7q2"), b"K7Q2\x00\x00\x00\x00")
        self.assertEqual(p.normalize_lobby(""), bytes(8))
        joined = p.decode(p.encode(p.Join(1, 1, 2, "ab12")))
        self.assertEqual(joined.lobby, "AB12")

    def test_invalid_codes_are_rejected(self):
        for code in ("TOOLONGCODE", "no space", "dash-1", "é"):
            with self.subTest(code=code), self.assertRaises(p.ProtocolError):
                p.normalize_lobby(code)


class Rejection(unittest.TestCase):
    def setUp(self):
        self.join = p.encode(p.Join(7, p.PLATFORM_WINDOWS, 2))

    def assertRejected(self, data):
        with self.assertRaises(p.ProtocolError):
            p.decode(bytes(data))

    def test_short_bad_magic_other_version_reserved(self):
        self.assertRejected(b"")
        self.assertRejected(self.join[:7])
        self.assertRejected(b"GGPO" + self.join[4:])
        self.assertRejected(self.join[:4] + bytes([1]) + self.join[5:])
        self.assertRejected(self.join[:6] + b"\x01\x00" + self.join[8:])

    def test_truncated_trailing_and_padding(self):
        self.assertRejected(self.join[:-1])
        self.assertRejected(self.join + b"\x00")
        self.assertRejected(self.join[:-1] + b"\x01")
        bind = p.encode(p.Bind(1, 1))
        self.assertRejected(bind[:-1])
        self.assertRejected(bind + b"\x00")

    def test_group_size_must_be_two_to_four(self):
        for size in (0, 1, 5):
            data = bytearray(self.join)
            data[17] = size
            self.assertRejected(data)

    def test_match_player_must_be_in_range_and_unused_slots_zero(self):
        match = bytearray(p.encode(p.Match(1, 2, 1, (7101, 7102), (1, 2))))
        bad_player = bytearray(match)
        bad_player[24] = 3
        self.assertRejected(bad_player)
        bad_slot = bytearray(match)
        bad_slot[-1] = 1
        self.assertRejected(bad_slot)

    def test_unknown_types(self):
        for message_type in (0, 8, 200):
            data = bytearray(self.join)
            data[5] = message_type
            self.assertRejected(data)


if __name__ == "__main__":
    unittest.main()
