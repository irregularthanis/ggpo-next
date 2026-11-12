"""UDP front end for the matchmaker: ``python -m matchmaking``."""

import argparse
import asyncio
import logging
import signal
import time

from .matchmaker import Matchmaker

LOG = logging.getLogger("matchmaking")


class MatchmakingProtocol(asyncio.DatagramProtocol):
    def __init__(self, matchmaker=None, clock=time.monotonic):
        self.matchmaker = matchmaker or Matchmaker()
        self.clock = clock
        self.transport = None

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, address):
        # IPv4 only: GGPO peers can only be given IPv4 endpoints.
        for reply, destination in self.matchmaker.handle(data, address[:2], self.clock()):
            self.transport.sendto(reply, destination)


async def serve(host, port, ready=None, stop=None):
    """Serve until ``stop`` is set; ``ready`` receives the bound address."""
    loop = asyncio.get_running_loop()
    protocol = MatchmakingProtocol()
    transport, _ = await loop.create_datagram_endpoint(
        lambda: protocol, local_addr=(host, port))
    bound = transport.get_extra_info("sockname")[:2]
    LOG.info("listening on %s:%d/udp", *bound)
    if ready is not None:
        ready(bound)
    stop = stop or asyncio.Event()
    try:
        await stop.wait()
    finally:
        transport.close()
        # Counts only: the server keeps no record of players' addresses.
        LOG.info("stopped after %d sessions", protocol.matchmaker.sessions_created)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Vector War matchmaking server")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=7100)
    args = parser.parse_args(argv)
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

    async def run():
        stop = asyncio.Event()
        loop = asyncio.get_running_loop()
        for name in ("SIGINT", "SIGTERM"):
            if hasattr(signal, name):
                loop.add_signal_handler(getattr(signal, name), stop.set)
        await serve(args.host, args.port, stop=stop)

    asyncio.run(run())


if __name__ == "__main__":
    main()
