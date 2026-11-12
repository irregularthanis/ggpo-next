"""The lobby and relay end to end on loopback with real UDP sockets.

Each test client is one socket, like Vector War's GGPO port: it joins the
lobby, binds to the other players' relay ports, then exchanges payloads that
stand in for GGPO packets. Clients are plain blocking sockets on worker
threads, so the test behaves the same with every event loop.
"""

import asyncio
import socket
import unittest

from matchmaking import protocol as p
from matchmaking.server import serve


def free_udp_ports(count):
    sockets = []
    try:
        for _ in range(count):
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.bind(("127.0.0.1", 0))
            sockets.append(s)
        return sorted(s.getsockname()[1] for s in sockets)
    finally:
        for s in sockets:
            s.close()


class LoopbackServer(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.relay_ports = free_udp_ports(4)
        self.stop = asyncio.Event()
        bound = asyncio.get_running_loop().create_future()
        self.task = asyncio.create_task(serve(
            "127.0.0.1", 0, self.relay_ports, ready=bound.set_result, stop=self.stop))
        self.lobby = await asyncio.wait_for(bound, 5)

    async def asyncTearDown(self):
        self.stop.set()
        await asyncio.wait_for(self.task, 10)

    def client(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("127.0.0.1", 0))
        sock.settimeout(5)
        self.addCleanup(sock.close)
        return sock

    async def send(self, sock, data, address):
        await asyncio.to_thread(sock.sendto, data, address)

    async def receive(self, sock):
        return await asyncio.to_thread(sock.recvfrom, 4096)

    async def play(self, players, lobby=""):
        clients = [self.client() for _ in range(players)]
        matches = {}
        for index, sock in enumerate(clients):
            await self.send(sock, p.encode(p.Join(index + 1, p.PLATFORM_LINUX, players, lobby)), self.lobby)
            if index < players - 1:
                reply, _ = await self.receive(sock)
                self.assertEqual(p.decode(reply), p.Waiting(index + 1, index + 1, players))
        for sock in clients:
            reply, _ = await self.receive(sock)
            match = p.decode(reply)
            matches[match.player] = (sock, match)
        self.assertEqual(sorted(matches), list(range(1, players + 1)))

        # BIND each client to every other player's relay port.
        for i, (sock, match) in matches.items():
            for k, port in enumerate(match.relay_ports, start=1):
                if k == i:
                    continue
                await self.send(sock, p.encode(p.Bind(match.token, i)), ("127.0.0.1", port))
                reply, source = await self.receive(sock)
                self.assertEqual(p.decode(reply), p.Bound(match.token, i))
                self.assertEqual(source, ("127.0.0.1", port))
        return matches

    async def assert_mesh(self, matches):
        for i, (sock_i, match) in matches.items():
            for k, (sock_k, _) in matches.items():
                if i == k:
                    continue
                payload = f"ggpo {i}->{k}".encode()
                await self.send(sock_i, payload, ("127.0.0.1", match.relay_ports[k - 1]))
                data, source = await self.receive(sock_k)
                self.assertEqual(data, payload)
                # Player k sees player i at R[i], exactly where it configured player i.
                self.assertEqual(source, ("127.0.0.1", match.relay_ports[i - 1]))

    async def test_two_players_play_through_the_relay(self):
        await self.assert_mesh(await self.play(2))

    async def test_four_players_play_through_the_relay_with_a_lobby_code(self):
        await self.assert_mesh(await self.play(4, "K7Q2"))


if __name__ == "__main__":
    unittest.main()
