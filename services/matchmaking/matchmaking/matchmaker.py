"""Pairing logic for the Vector War matchmaking server.

``Matchmaker.handle`` is pure apart from its own state: it takes one datagram,
its source address and the current time, and returns the datagrams to send.
The UDP server in ``server.py`` only moves bytes, so every rule here is tested
without sockets or real time.
"""

import secrets
from collections import OrderedDict
from dataclasses import dataclass

from . import protocol

WAITING_EXPIRY_S = 10.0
SESSION_EXPIRY_S = 60.0
MAX_WAITING = 1000
RATE_PER_SECOND = 10.0
RATE_BURST = 20.0


@dataclass
class _Client:
    nonce: int
    platform: int
    last_seen: float


@dataclass
class _Assignment:
    nonce: int
    reply: bytes
    expires: float


class _TokenBucket:
    def __init__(self, now):
        self.tokens = RATE_BURST
        self.updated = now

    def take(self, now):
        self.tokens = min(RATE_BURST, self.tokens + (now - self.updated) * RATE_PER_SECOND)
        self.updated = now
        if self.tokens < 1.0:
            return False
        self.tokens -= 1.0
        return True


class Matchmaker:
    """First come, first served pairing of two players per session."""

    def __init__(self, session_ids=None):
        self._waiting = OrderedDict()   # address -> _Client, oldest first
        self._assigned = {}             # address -> _Assignment
        self._buckets = {}              # source IP -> _TokenBucket
        self._session_ids = session_ids or (lambda: secrets.randbits(64))
        self.sessions_created = 0

    @property
    def waiting_count(self):
        return len(self._waiting)

    def handle(self, data, address, now):
        """Return ``[(bytes, address), ...]`` to send in response."""
        self._expire(now)
        try:
            version, message_type = protocol.peek_header(data)
        except protocol.ProtocolError:
            return []
        # Only a full-size REGISTER earns a reply, so no reply can exceed its request.
        if message_type != protocol.REGISTER or len(data) != protocol.REGISTER_SIZE:
            return []
        if version != protocol.VERSION:
            nonce = int.from_bytes(data[8:16], "little")
            return [(protocol.encode(protocol.Error(nonce, protocol.ERROR_VERSION_MISMATCH)), address)]
        try:
            message = protocol.decode(data)
        except protocol.ProtocolError:
            return []

        bucket = self._buckets.setdefault(address[0], _TokenBucket(now))
        if not bucket.take(now):
            return [(protocol.encode(protocol.Error(message.nonce, protocol.ERROR_RATE_LIMITED)), address)]

        assignment = self._assigned.get(address)
        if assignment is not None:
            if assignment.nonce == message.nonce:
                # The client missed its MATCH; resend the same one.
                return [(assignment.reply, address)]
            # A restarted client: forget its old session and queue it again.
            del self._assigned[address]

        client = self._waiting.get(address)
        if client is not None and client.nonce == message.nonce:
            client.last_seen = now
        else:
            if client is None and len(self._waiting) >= MAX_WAITING:
                return [(protocol.encode(protocol.Error(message.nonce, protocol.ERROR_SERVER_FULL)), address)]
            self._waiting.pop(address, None)
            self._waiting[address] = _Client(message.nonce, message.platform, now)

        opponent = next((other for other in self._waiting if other != address), None)
        if opponent is None:
            return [(protocol.encode(protocol.Waiting(message.nonce, 1)), address)]
        return self._pair(opponent, address, now)

    def _pair(self, first_address, second_address, now):
        first = self._waiting.pop(first_address)
        second = self._waiting.pop(second_address)
        session = self._session_ids()
        self.sessions_created += 1
        replies = []
        for player, (me, me_address, peer, peer_address) in enumerate(
                ((first, first_address, second, second_address),
                 (second, second_address, first, first_address)), start=1):
            reply = protocol.encode(protocol.Match(
                me.nonce, session, player, peer.platform, peer_address[0], peer_address[1]))
            self._assigned[me_address] = _Assignment(me.nonce, reply, now + SESSION_EXPIRY_S)
            replies.append((reply, me_address))
        return replies

    def _expire(self, now):
        for address in [a for a, c in self._waiting.items() if now - c.last_seen > WAITING_EXPIRY_S]:
            del self._waiting[address]
        for address in [a for a, s in self._assigned.items() if now > s.expires]:
            del self._assigned[address]
        for ip in [ip for ip, b in self._buckets.items() if now - b.updated > WAITING_EXPIRY_S]:
            del self._buckets[ip]
