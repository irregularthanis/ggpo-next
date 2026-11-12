"""The UDP server end to end on loopback: two clients get matched.

Test clients are plain blocking sockets run on worker threads, so the test
behaves the same with every event loop (including Windows' proactor loop).
"""

import asyncio
import socket
import unittest

from matchmaking import protocol as p
from matchmaking.server import serve


class LoopbackServer(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.stop = asyncio.Event()
        bound = asyncio.get_running_loop().create_future()
        self.task = asyncio.create_task(serve("127.0.0.1", 0, ready=bound.set_result, stop=self.stop))
        self.address = await asyncio.wait_for(bound, 5)

    async def asyncTearDown(self):
        self.stop.set()
        await asyncio.wait_for(self.task, 5)

    def client(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("127.0.0.1", 0))
        sock.settimeout(5)
        self.addCleanup(sock.close)
        return sock

    async def send(self, sock, message):
        await asyncio.to_thread(sock.sendto, p.encode(message), self.address)

    async def receive(self, sock):
        return p.decode(await asyncio.to_thread(sock.recv, 512))

    async def test_two_clients_are_matched_with_each_others_endpoints(self):
        a, b = self.client(), self.client()
        await self.send(a, p.Register(11, p.PLATFORM_WINDOWS, 1))
        self.assertEqual(await self.receive(a), p.Waiting(11, 1))

        await self.send(b, p.Register(22, p.PLATFORM_ANDROID, 1))
        match_b = await self.receive(b)
        match_a = await self.receive(a)

        self.assertEqual((match_a.player, match_b.player), (1, 2))
        self.assertEqual(match_a.session, match_b.session)
        self.assertEqual((match_a.peer_ip, match_a.peer_port), b.getsockname())
        self.assertEqual((match_b.peer_ip, match_b.peer_port), a.getsockname())
        self.assertEqual(match_a.peer_platform, p.PLATFORM_ANDROID)
        self.assertEqual(match_b.peer_platform, p.PLATFORM_WINDOWS)


if __name__ == "__main__":
    unittest.main()
