"""UDP front end for the lobby and the relay: ``python -m matchmaking``."""

import argparse
import asyncio
import logging
import signal
import time

from .matchmaker import Matchmaker
from .relay import RelayPool

LOG = logging.getLogger("matchmaking")
EXPIRY_INTERVAL_S = 5.0


class _LobbyProtocol(asyncio.DatagramProtocol):
    def __init__(self, server):
        self.server = server

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, address):
        # IPv4 only: GGPO peers can only be given IPv4 endpoints.
        for reply, destination in self.server.matchmaker.handle(data, address[:2], self.server.clock()):
            self.transport.sendto(reply, destination)


class _RelayProtocol(asyncio.DatagramProtocol):
    def __init__(self, server, port):
        self.server = server
        self.port = port

    def datagram_received(self, data, address):
        server = self.server
        for from_port, payload, destination in server.relay.handle(
                self.port, data, address[:2], server.clock()):
            transport = server.relay_transports.get(from_port)
            if transport is not None:
                transport.sendto(payload, destination)


class Server:
    def __init__(self, relay_ports, clock=time.monotonic):
        self.clock = clock
        self.relay = RelayPool(relay_ports)
        self.matchmaker = Matchmaker(self.relay)
        self.relay_transports = {}
        self.lobby_transport = None

    async def start(self, host, lobby_port):
        loop = asyncio.get_running_loop()
        self.lobby_transport, _ = await loop.create_datagram_endpoint(
            lambda: _LobbyProtocol(self), local_addr=(host, lobby_port))
        for port in self.relay.ports:
            transport, _ = await loop.create_datagram_endpoint(
                lambda port=port: _RelayProtocol(self, port), local_addr=(host, port))
            self.relay_transports[port] = transport
        return self.lobby_transport.get_extra_info("sockname")[:2]

    def close(self):
        if self.lobby_transport is not None:
            self.lobby_transport.close()
        for transport in self.relay_transports.values():
            transport.close()


async def serve(host, lobby_port, relay_ports, ready=None, stop=None):
    """Serve until ``stop`` is set; ``ready`` receives the bound lobby address."""
    server = Server(relay_ports)
    bound = await server.start(host, lobby_port)
    LOG.info("lobby on %s:%d/udp, %d relay ports", bound[0], bound[1], len(server.relay_transports))
    if ready is not None:
        ready(bound)
    stop = stop or asyncio.Event()
    try:
        while not stop.is_set():
            try:
                await asyncio.wait_for(stop.wait(), EXPIRY_INTERVAL_S)
            except asyncio.TimeoutError:
                pass
            server.relay.expire(server.clock())
    finally:
        server.close()
        # Counts only: the server keeps no record of players' addresses.
        LOG.info("stopped after %d sessions", server.relay.sessions_created)


def parse_port_range(text):
    first, _, last = text.partition("-")
    first, last = int(first), int(last or first)
    if not 1024 <= first <= last <= 65535:
        raise argparse.ArgumentTypeError("relay range must be FIRST-LAST within 1024-65535")
    return range(first, last + 1)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Vector War lobby and relay server")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=7100, help="lobby port")
    parser.add_argument("--relay-ports", type=parse_port_range, default=range(7101, 7500),
                        help="relay port range, e.g. 7101-7499")
    args = parser.parse_args(argv)
    if args.port in args.relay_ports:
        parser.error("the lobby port must not be inside the relay range")
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

    async def run():
        stop = asyncio.Event()
        loop = asyncio.get_running_loop()
        for name in ("SIGINT", "SIGTERM"):
            try:
                loop.add_signal_handler(getattr(signal, name), stop.set)
            except (AttributeError, NotImplementedError):
                pass  # Not available on this platform; Ctrl+C still stops asyncio.run.
        await serve(args.host, args.port, args.relay_ports, stop=stop)

    asyncio.run(run())


if __name__ == "__main__":
    main()
