"""Pairing rules, expiry, limits and abuse resistance, without sockets."""

import unittest

from matchmaking import matchmaker as mm
from matchmaking import protocol as p

A = ("198.51.100.10", 40001)
B = ("203.0.113.20", 50002)
C = ("192.0.2.30", 60003)


def register(nonce, platform=p.PLATFORM_LINUX):
    return p.encode(p.Register(nonce, platform, 1))


class Pairing(unittest.TestCase):
    def setUp(self):
        self.ids = iter(range(100, 200))
        self.m = mm.Matchmaker(session_ids=lambda: next(self.ids))

    def send(self, data, address, now=0.0):
        return [(p.decode(reply), to) for reply, to in self.m.handle(data, address, now)]

    def test_first_client_waits(self):
        self.assertEqual(self.send(register(1), A), [(p.Waiting(1, 1), A)])
        self.assertEqual(self.m.waiting_count, 1)

    def test_second_client_pairs_both_with_public_endpoints(self):
        self.send(register(1, p.PLATFORM_WINDOWS), A)
        replies = dict((to, msg) for msg, to in self.send(register(2, p.PLATFORM_ANDROID), B))
        self.assertEqual(replies[A], p.Match(1, 100, 1, p.PLATFORM_ANDROID, B[0], B[1]))
        self.assertEqual(replies[B], p.Match(2, 100, 2, p.PLATFORM_WINDOWS, A[0], A[1]))
        self.assertEqual(self.m.waiting_count, 0)

    def test_earlier_registrant_is_player_one(self):
        self.send(register(1), A, now=0.0)
        self.send(register(2), B, now=1.0)
        replies = dict((to, msg) for msg, to in self.send(register(2), B, now=1.5))
        self.assertEqual(replies, {B: replies[B]})  # B resends only its own MATCH
        self.assertEqual(replies[B].player, 2)

    def test_lost_match_is_resent_identically(self):
        self.send(register(1), A)
        first = dict((to, raw) for raw, to in self.m.handle(register(2), B, 0.0))
        again = self.m.handle(register(1), A, 1.0)
        self.assertEqual(again, [(first[A], A)])

    def test_third_client_waits_for_a_fourth(self):
        self.send(register(1), A)
        self.send(register(2), B)
        self.assertEqual(self.send(register(3), C), [(p.Waiting(3, 1), C)])

    def test_restarted_client_with_new_nonce_requeues(self):
        self.send(register(1), A)
        self.send(register(2), B)
        self.assertEqual(self.send(register(9), A, now=2.0), [(p.Waiting(9, 1), A)])

    def test_reregistering_does_not_pair_a_client_with_itself(self):
        self.send(register(1), A)
        self.assertEqual(self.send(register(1), A, now=1.0), [(p.Waiting(1, 1), A)])


class Expiry(unittest.TestCase):
    def setUp(self):
        self.m = mm.Matchmaker(session_ids=lambda: 1)

    def test_idle_waiting_client_is_forgotten(self):
        self.m.handle(register(1), A, 0.0)
        replies = self.m.handle(register(2), B, mm.WAITING_EXPIRY_S + 0.1)
        self.assertEqual([p.decode(r) for r, _ in replies], [p.Waiting(2, 1)])

    def test_refreshed_waiting_client_stays(self):
        for t in range(0, 30, 2):
            self.m.handle(register(1), A, float(t))
        replies = self.m.handle(register(2), B, 30.0)
        self.assertEqual(len(replies), 2)

    def test_session_expires(self):
        self.m.handle(register(1), A, 0.0)
        self.m.handle(register(2), B, 0.0)
        later = mm.SESSION_EXPIRY_S + 1.0
        replies = self.m.handle(register(1), A, later)
        self.assertEqual([p.decode(r) for r, _ in replies], [p.Waiting(1, 1)])


class Abuse(unittest.TestCase):
    def setUp(self):
        self.m = mm.Matchmaker()

    def test_non_register_and_garbage_get_no_reply(self):
        for data in (b"", b"hello", p.encode(p.Hello(1, 1)), p.encode(p.Punch(1, 2, 3)),
                     p.encode(p.Waiting(1, 1)), register(1)[:-1], register(1) + b"\x00"):
            with self.subTest(data=data):
                self.assertEqual(self.m.handle(data, A, 0.0), [])

    def test_other_version_gets_version_error_no_larger_than_request(self):
        data = bytearray(register(42))
        data[4] = 9
        replies = self.m.handle(bytes(data), A, 0.0)
        self.assertEqual([p.decode(r) for r, _ in replies], [p.Error(42, p.ERROR_VERSION_MISMATCH)])
        self.assertLessEqual(len(replies[0][0]), len(data))

    def test_rate_limit_per_source_ip(self):
        results = [self.m.handle(register(1), A, 0.0)[0][0] for _ in range(int(mm.RATE_BURST) + 5)]
        decoded = [p.decode(r) for r in results]
        self.assertIn(p.Error(1, p.ERROR_RATE_LIMITED), decoded)
        self.assertNotIn(p.Error(1, p.ERROR_RATE_LIMITED), decoded[:int(mm.RATE_BURST)])
        # Tokens refill with time.
        self.assertEqual(p.decode(self.m.handle(register(1), A, 1.0)[0][0]), p.Waiting(1, 1))

    def test_server_full(self):
        original = mm.MAX_WAITING
        mm.MAX_WAITING = 2
        try:
            # Waiting clients never coexist in normal pairing, so seed the queue.
            m = mm.Matchmaker()
            m._waiting[("192.0.2.1", 1)] = mm._Client(1, 0, 0.0)
            m._waiting[("192.0.2.2", 1)] = mm._Client(2, 0, 0.0)
            replies = m.handle(register(3), ("192.0.2.3", 1), 0.0)
            self.assertEqual([p.decode(r) for r, _ in replies], [p.Error(3, p.ERROR_SERVER_FULL)])
        finally:
            mm.MAX_WAITING = original

    def test_every_reply_is_no_larger_than_its_request(self):
        m = mm.Matchmaker()
        addresses = [(f"192.0.2.{i}", 7000 + i) for i in range(1, 9)]
        for i, address in enumerate(addresses):
            request = register(i + 1)
            for reply, _ in m.handle(request, address, float(i)):
                self.assertLessEqual(len(reply), len(request))


if __name__ == "__main__":
    unittest.main()
