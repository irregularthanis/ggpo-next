"""Relay that forwards GGPO's own packets between the players of a session.

Each player ``j`` of a session owns one relay port ``R[j]``. Every client
configures remote player ``k`` at ``server:R[k]``. Before GGPO starts, player
``i`` sends ``BIND(token, i)`` from its GGPO port to every ``R[k]`` (k != i);
the relay records the exact source address, ``addr[i][k]``, because routers
may use a different public port per destination.

A datagram arriving at ``R[k]`` from ``addr[i][k]`` is player ``i``'s packet
for player ``k``: it is forwarded unchanged *from* ``R[i]`` *to*
``addr[k][i]``. Player ``k`` therefore sees it coming from ``server:R[i]``, the
endpoint it configured for player ``i``, so GGPO needs no changes.

Anything from an address that did not bind with the session token is dropped,
output never exceeds input, and each player has a forwarding rate cap, so the
relay is useless as an open proxy or amplifier. Like the matchmaker this is
pure logic with an injected clock; ``server.py`` moves the bytes.
"""

import secrets

from . import protocol

IDLE_EXPIRY_S = 30.0
MAX_SESSION_S = 2 * 60 * 60.0
# GGPO sends at most a few packets per frame at 60 frames per second.
FORWARD_RATE_PER_SECOND = 300.0
FORWARD_BURST = 600.0


class _Bucket:
    def __init__(self, now):
        self.tokens = FORWARD_BURST
        self.updated = now

    def take(self, now):
        self.tokens = min(FORWARD_BURST, self.tokens + (now - self.updated) * FORWARD_RATE_PER_SECOND)
        self.updated = now
        if self.tokens < 1.0:
            return False
        self.tokens -= 1.0
        return True


class Session:
    def __init__(self, token, ports, now):
        self.token = token
        self.ports = tuple(ports)       # ports[j - 1] is R[j]
        self.created = now
        self.last_activity = now
        self.addr = {}                   # (i, k) -> address player i uses to reach R[k]
        self.sender = {}                 # (k, address) -> i
        self.buckets = {}                # i -> _Bucket

    @property
    def player_count(self):
        return len(self.ports)

    def player_of_port(self, port):
        return self.ports.index(port) + 1

    def bound(self, i, k):
        return (i, k) in self.addr


class RelayPool:
    def __init__(self, ports, tokens=None):
        self.ports = tuple(ports)
        self._free = list(self.ports)
        self._by_port = {}
        self._tokens = tokens or (lambda: secrets.randbits(64))
        self.sessions_created = 0

    @property
    def free_ports(self):
        return len(self._free)

    @property
    def active_sessions(self):
        return len({id(s) for s in self._by_port.values()})

    def allocate(self, player_count, now):
        """Reserve ``player_count`` relay ports; ``None`` if the pool is exhausted."""
        if len(self._free) < player_count:
            return None
        ports = [self._free.pop(0) for _ in range(player_count)]
        session = Session(self._tokens(), ports, now)
        for port in ports:
            self._by_port[port] = session
        self.sessions_created += 1
        return session

    def handle(self, port, data, source, now):
        """Return ``[(from_port, bytes, destination), ...]`` for one datagram."""
        session = self._by_port.get(port)
        if session is None:
            return []
        k = session.player_of_port(port)
        if len(data) == protocol.BIND_SIZE:
            reply = self._bind(session, k, port, data, source, now)
            if reply is not None:
                return reply
        i = session.sender.get((k, source))
        if i is None:
            return []
        destination = session.addr.get((k, i))
        if destination is None:
            return []  # Player k has not bound towards player i yet.
        bucket = session.buckets.setdefault(i, _Bucket(now))
        if not bucket.take(now):
            return []
        session.last_activity = now
        return [(session.ports[i - 1], data, destination)]

    def _bind(self, session, k, port, data, source, now):
        try:
            message = protocol.decode(data)
        except protocol.ProtocolError:
            return None
        if not isinstance(message, protocol.Bind):
            return None
        i = message.player
        if message.token != session.token or not 1 <= i <= session.player_count or i == k:
            return []
        previous = session.addr.get((i, k))
        if previous is not None and previous != source:
            session.sender.pop((k, previous), None)
        owner = session.sender.get((k, source))
        if owner is not None and owner != i:
            return []  # One address cannot speak for two players on the same port.
        session.addr[(i, k)] = source
        session.sender[(k, source)] = i
        session.last_activity = now
        return [(port, protocol.encode(protocol.Bound(session.token, i)), source)]

    def expire(self, now):
        """End idle or overlong sessions and return their ports to the pool."""
        ended = {id(s): s for s in self._by_port.values()
                 if now - s.last_activity > IDLE_EXPIRY_S or now - s.created > MAX_SESSION_S}
        for session in ended.values():
            for port in session.ports:
                del self._by_port[port]
                self._free.append(port)
        return len(ended)
