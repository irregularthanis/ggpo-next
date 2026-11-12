"""Relay forwarding rules, without sockets."""

import unittest

from matchmaking import protocol as p
from matchmaking import relay as r

TOKEN = 0x1122334455667788
PORTS = (7101, 7102, 7103, 7104)
# Each player's public address *per destination relay port*, as a symmetric
# NAT would present it: same IP, a different port towards each relay port.
IP = {1: "198.51.100.1", 2: "203.0.113.2", 3: "192.0.2.3", 4: "192.0.2.4"}


def addr(i, k):
    return (IP[i], 40000 + 10 * i + k)


def bind(i):
    return p.encode(p.Bind(TOKEN, i))


class Forwarding(unittest.TestCase):
    def setUp(self):
        self.pool = r.RelayPool(PORTS, tokens=lambda: TOKEN)

    def session(self, players):
        session = self.pool.allocate(players, 0.0)
        for i in range(1, players + 1):
            for k in range(1, players + 1):
                if i != k:
                    replies = self.pool.handle(PORTS[k - 1], bind(i), addr(i, k), 0.0)
                    self.assertEqual(replies, [(PORTS[k - 1], p.encode(p.Bound(TOKEN, i)), addr(i, k))])
        return session

    def test_two_players_see_each_other_at_the_relay_port_they_configured(self):
        self.session(2)
        # Player 1 sends to player 2 at R[2]; player 2 receives it from R[1].
        self.assertEqual(self.pool.handle(7102, b"ggpo-1to2", addr(1, 2), 1.0),
                         [(7101, b"ggpo-1to2", addr(2, 1))])
        self.assertEqual(self.pool.handle(7101, b"ggpo-2to1", addr(2, 1), 1.0),
                         [(7102, b"ggpo-2to1", addr(1, 2))])

    def test_four_players_full_mesh(self):
        self.session(4)
        for i in range(1, 5):
            for k in range(1, 5):
                if i == k:
                    continue
                payload = f"{i}->{k}".encode()
                with self.subTest(i=i, k=k):
                    self.assertEqual(self.pool.handle(PORTS[k - 1], payload, addr(i, k), 1.0),
                                     [(PORTS[i - 1], payload, addr(k, i))])

    def test_output_never_exceeds_input(self):
        self.session(2)
        for size in (1, 20, 512, 4096):
            out = self.pool.handle(7102, bytes(size), addr(1, 2), 1.0)
            self.assertEqual(sum(len(data) for _, data, _ in out), size)

    def test_ggpo_packet_of_bind_size_is_forwarded_not_parsed(self):
        self.session(2)
        packet = bytes(range(p.BIND_SIZE))
        self.assertEqual(self.pool.handle(7102, packet, addr(1, 2), 1.0), [(7101, packet, addr(2, 1))])

    def test_nothing_forwarded_until_the_receiver_has_bound(self):
        self.pool.allocate(2, 0.0)
        self.pool.handle(7102, bind(1), addr(1, 2), 0.0)
        self.assertEqual(self.pool.handle(7102, b"early", addr(1, 2), 0.0), [])


class Rejection(unittest.TestCase):
    def setUp(self):
        self.pool = r.RelayPool(PORTS, tokens=lambda: TOKEN)
        self.pool.allocate(2, 0.0)
        for i, k in ((1, 2), (2, 1)):
            self.pool.handle(PORTS[k - 1], bind(i), addr(i, k), 0.0)

    def test_unbound_sources_are_dropped(self):
        self.assertEqual(self.pool.handle(7102, b"x", ("192.0.2.99", 1234), 1.0), [])
        # A bound address on the wrong port is not a sender there.
        self.assertEqual(self.pool.handle(7101, b"x", addr(1, 2), 1.0), [])

    def test_wrong_token_self_bind_and_bad_player_are_ignored(self):
        bad = [p.encode(p.Bind(TOKEN + 1, 1)), p.encode(p.Bind(TOKEN, 2)), p.encode(p.Bind(TOKEN, 4))]
        for data in bad:
            with self.subTest(data=data):
                self.assertEqual(self.pool.handle(7102, data, ("192.0.2.99", 1), 1.0), [])
        self.assertEqual(self.pool.handle(7102, b"x", ("192.0.2.99", 1), 1.0), [])

    def test_one_address_cannot_bind_as_two_players_on_a_port(self):
        pool = r.RelayPool(PORTS, tokens=lambda: TOKEN)
        pool.allocate(3, 0.0)
        pool.handle(7103, bind(1), ("192.0.2.9", 1), 0.0)
        self.assertEqual(pool.handle(7103, bind(2), ("192.0.2.9", 1), 0.0), [])

    def test_rebinding_moves_the_player_to_its_new_address(self):
        moved = ("198.51.100.1", 49999)
        self.pool.handle(7102, bind(1), moved, 1.0)
        self.assertEqual(self.pool.handle(7102, b"x", addr(1, 2), 1.0), [])
        self.assertEqual(self.pool.handle(7102, b"x", moved, 1.0), [(7101, b"x", addr(2, 1))])
        self.assertEqual(self.pool.handle(7101, b"y", addr(2, 1), 1.0), [(7102, b"y", moved)])

    def test_unallocated_port_is_silent(self):
        self.assertEqual(self.pool.handle(7104, bind(1), addr(1, 4), 1.0), [])

    def test_per_player_rate_cap(self):
        burst = int(r.FORWARD_BURST)
        sent = sum(len(self.pool.handle(7102, b"x", addr(1, 2), 1.0)) for _ in range(burst + 50))
        self.assertEqual(sent, burst)
        self.assertEqual(len(self.pool.handle(7102, b"x", addr(1, 2), 2.0)), 1)


class Lifetime(unittest.TestCase):
    def test_idle_session_expires_and_returns_ports(self):
        pool = r.RelayPool(PORTS)
        pool.allocate(4, 0.0)
        self.assertIsNone(pool.allocate(2, 0.0))
        self.assertEqual(pool.expire(r.IDLE_EXPIRY_S + 1), 1)
        self.assertEqual(pool.free_ports, 4)
        self.assertIsNotNone(pool.allocate(2, r.IDLE_EXPIRY_S + 1))

    def test_traffic_keeps_a_session_alive_until_the_hard_limit(self):
        pool = r.RelayPool(PORTS, tokens=lambda: TOKEN)
        pool.allocate(2, 0.0)
        pool.handle(7102, bind(1), addr(1, 2), 0.0)
        pool.handle(7101, bind(2), addr(2, 1), 0.0)
        now = 0.0
        while now < r.MAX_SESSION_S - 10:
            now += 20.0
            pool.handle(7102, b"x", addr(1, 2), now)
            self.assertEqual(pool.expire(now), 0)
        self.assertEqual(pool.expire(r.MAX_SESSION_S + 1), 1)

    def test_ports_are_not_shared_between_sessions(self):
        pool = r.RelayPool(PORTS)
        one = pool.allocate(2, 0.0)
        two = pool.allocate(2, 0.0)
        self.assertFalse(set(one.ports) & set(two.ports))
        self.assertIsNone(pool.allocate(1, 0.0))


if __name__ == "__main__":
    unittest.main()
