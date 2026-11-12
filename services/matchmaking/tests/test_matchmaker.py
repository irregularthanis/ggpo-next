"""Lobby rules, expiry, limits and abuse resistance, without sockets."""

import unittest

from matchmaking import matchmaker as mm
from matchmaking import protocol as p
from matchmaking.relay import RelayPool

A = ("198.51.100.10", 40001)
B = ("203.0.113.20", 50002)
C = ("192.0.2.30", 60003)
D = ("192.0.2.40", 60004)


def join(nonce, size=2, lobby="", platform=p.PLATFORM_LINUX):
    return p.encode(p.Join(nonce, platform, size, lobby))


class Lobby(unittest.TestCase):
    def setUp(self):
        self.relay = RelayPool(range(7101, 7121), tokens=lambda: 0xABCD)
        self.m = mm.Matchmaker(self.relay)

    def send(self, data, address, now=0.0):
        return {to: p.decode(reply) for reply, to in self.m.handle(data, address, now)}

    def test_first_client_waits_with_progress(self):
        self.assertEqual(self.send(join(1, 4), A), {A: p.Waiting(1, 1, 4)})
        self.assertEqual(self.send(join(2, 4), B), {B: p.Waiting(2, 2, 4)})

    def test_two_players_get_relay_ports_numbers_and_platforms(self):
        self.send(join(1, platform=p.PLATFORM_WINDOWS), A)
        replies = self.send(join(2, platform=p.PLATFORM_ANDROID), B)
        self.assertEqual(replies[A], p.Match(1, 0xABCD, 1, (7101, 7102), (1, 3)))
        self.assertEqual(replies[B], p.Match(2, 0xABCD, 2, (7101, 7102), (1, 3)))
        self.assertEqual(self.m.waiting_count, 0)

    def test_four_player_group_numbers_in_join_order(self):
        for nonce, address in enumerate((A, B, C), start=1):
            self.send(join(nonce, 4), address)
        replies = self.send(join(4, 4), D)
        self.assertEqual([replies[a].player for a in (A, B, C, D)], [1, 2, 3, 4])
        self.assertTrue(all(r.relay_ports == (7101, 7102, 7103, 7104) for r in replies.values()))

    def test_lobby_codes_and_sizes_keep_groups_apart(self):
        self.send(join(1, 2, "K7Q2"), A)
        self.assertEqual(self.send(join(2, 2), B), {B: p.Waiting(2, 1, 2)})       # public queue
        self.assertEqual(self.send(join(3, 3, "K7Q2"), C), {C: p.Waiting(3, 1, 3)})  # other size
        replies = self.send(join(4, 2, "k7q2"), D)                                  # same code
        self.assertEqual(set(replies), {A, D})

    def test_changing_lobby_leaves_the_old_group(self):
        self.send(join(1, 2, "ONE"), A)
        self.send(join(1, 2, "TWO"), A)
        self.assertEqual(self.send(join(2, 2, "ONE"), B), {B: p.Waiting(2, 1, 2)})

    def test_lost_match_is_resent_identically(self):
        self.send(join(1), A)
        first = {to: raw for raw, to in self.m.handle(join(2), B, 0.0)}
        self.assertEqual(self.m.handle(join(1), A, 1.0), [(first[A], A)])

    def test_restarted_client_with_new_nonce_rejoins(self):
        self.send(join(1), A)
        self.send(join(2), B)
        self.assertEqual(self.send(join(9), A, now=2.0), {A: p.Waiting(9, 1, 2)})

    def test_rejoining_does_not_group_a_client_with_itself(self):
        self.send(join(1), A)
        self.assertEqual(self.send(join(1), A, now=1.0), {A: p.Waiting(1, 1, 2)})

    def test_no_relay_ports_reports_server_full_and_keeps_waiting(self):
        m = mm.Matchmaker(RelayPool(range(7101, 7102)))
        m.handle(join(1), A, 0.0)
        replies = {to: p.decode(r) for r, to in m.handle(join(2), B, 0.0)}
        self.assertEqual(replies, {A: p.Error(1, p.ERROR_SERVER_FULL), B: p.Error(2, p.ERROR_SERVER_FULL)})
        self.assertEqual(m.waiting_count, 2)


class Expiry(unittest.TestCase):
    def setUp(self):
        self.m = mm.Matchmaker(RelayPool(range(7101, 7121)))

    def test_idle_waiting_client_is_forgotten(self):
        self.m.handle(join(1), A, 0.0)
        replies = self.m.handle(join(2), B, mm.WAITING_EXPIRY_S + 0.1)
        self.assertEqual([p.decode(r) for r, _ in replies], [p.Waiting(2, 1, 2)])

    def test_refreshed_waiting_client_stays(self):
        for t in range(0, 30, 2):
            self.m.handle(join(1), A, float(t))
        self.assertEqual(len(self.m.handle(join(2), B, 30.0)), 2)


class Abuse(unittest.TestCase):
    def setUp(self):
        self.m = mm.Matchmaker(RelayPool(range(7101, 7121)))

    def test_non_join_and_garbage_get_no_reply(self):
        for data in (b"", b"hello", p.encode(p.Hello(1, 1)), p.encode(p.Bind(1, 1)),
                     p.encode(p.Waiting(1, 1, 2)), join(1)[:-1], join(1) + b"\x00"):
            with self.subTest(data=data):
                self.assertEqual(self.m.handle(data, A, 0.0), [])

    def test_other_version_gets_version_error_no_larger_than_request(self):
        data = bytearray(join(42))
        data[4] = 9
        replies = self.m.handle(bytes(data), A, 0.0)
        self.assertEqual([p.decode(r) for r, _ in replies], [p.Error(42, p.ERROR_VERSION_MISMATCH)])
        self.assertLessEqual(len(replies[0][0]), len(data))

    def test_rate_limit_per_source_ip(self):
        results = [p.decode(self.m.handle(join(1, 4), A, 0.0)[0][0]) for _ in range(int(mm.RATE_BURST) + 5)]
        self.assertIn(p.Error(1, p.ERROR_RATE_LIMITED), results)
        self.assertNotIn(p.Error(1, p.ERROR_RATE_LIMITED), results[:int(mm.RATE_BURST)])
        self.assertEqual(p.decode(self.m.handle(join(1, 4), A, 1.0)[0][0]), p.Waiting(1, 1, 4))

    def test_waiting_queue_is_capped(self):
        original = mm.MAX_WAITING
        mm.MAX_WAITING = 2
        try:
            self.m.handle(join(1, 4), ("192.0.2.1", 1), 0.0)
            self.m.handle(join(2, 4), ("192.0.2.2", 1), 0.0)
            replies = self.m.handle(join(3, 4), ("192.0.2.3", 1), 0.0)
            self.assertEqual([p.decode(r) for r, _ in replies], [p.Error(3, p.ERROR_SERVER_FULL)])
        finally:
            mm.MAX_WAITING = original

    def test_every_reply_is_no_larger_than_its_request(self):
        for i in range(8):
            request = join(i + 1, 4)
            for reply, _ in self.m.handle(request, (f"192.0.2.{i}", 7000 + i), float(i)):
                self.assertLessEqual(len(reply), len(request))


if __name__ == "__main__":
    unittest.main()


class HolePunchingCandidates(unittest.TestCase):
    """Version 3: every member learns the others' public and LAN candidates."""

    def setUp(self):
        self.relay = RelayPool(range(7101, 7121), tokens=lambda: 0xABCD)
        self.m = mm.Matchmaker(self.relay)
        self.m.handle(join(1), A, 0.0)
        self.m.handle(join(2), B, 0.0)

    def ask(self, player, address, lan=p.NO_CANDIDATE, token=0xABCD, now=1.0):
        replies = self.m.handle(p.encode(p.PeersRequest(token, player, lan)), address, now)
        return [p.decode(raw) for raw, to in replies if to == address]

    def test_each_member_learns_the_others_candidates(self):
        first = self.ask(1, A, ("192.168.1.10", 7000))
        self.assertEqual(first, [p.Peers(0xABCD, (A, p.NO_CANDIDATE),
                                         (("192.168.1.10", 7000), p.NO_CANDIDATE))])
        second = self.ask(2, B, ("10.0.0.7", 7001))
        self.assertEqual(second, [p.Peers(0xABCD, (A, B),
                                          (("192.168.1.10", 7000), ("10.0.0.7", 7001)))])

    def test_public_candidate_is_the_observed_source_address(self):
        moved = ("198.51.100.10", 45555)              # e.g. a NAT rebinding
        reply = self.ask(1, moved)[0]
        self.assertEqual(reply.public[0], moved)

    def test_non_private_lan_addresses_are_dropped(self):
        for lan in (("8.8.8.8", 53), ("100.64.1.1", 7000), ("127.0.0.1", 7000)):
            with self.subTest(lan=lan):
                self.assertEqual(self.ask(1, A, lan)[0].lan[0], p.NO_CANDIDATE)

    def test_unknown_token_or_player_gets_no_reply(self):
        self.assertEqual(self.ask(1, A, token=0x1234), [])
        self.assertEqual(self.ask(3, C), [])

    def test_candidates_end_with_the_relay_session(self):
        self.assertTrue(self.ask(1, A))
        self.relay.expire(1000.0)
        self.assertEqual(self.ask(1, A, now=1000.0), [])

    def test_replies_are_request_sized_and_rate_limited(self):
        request = p.encode(p.PeersRequest(0xABCD, 1))
        answered = 0
        for _ in range(int(mm.RATE_BURST) + 5):
            replies = self.m.handle(request, A, 2.0)
            for raw, _ in replies:
                self.assertEqual(len(raw), len(request))
            answered += len(replies)
        self.assertLessEqual(answered, int(mm.RATE_BURST))

    def test_version_two_peers_request_is_ignored(self):
        data = bytearray(p.encode(p.PeersRequest(0xABCD, 1)))
        data[4] = p.VERSION
        self.assertEqual(self.m.handle(bytes(data), A, 1.0), [])
