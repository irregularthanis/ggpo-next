"""Lobby logic: group 2-4 players and hand each a relay session.

Players who send the same lobby code and group size are grouped first come,
first served; an empty code is the public queue for that size. When a group is
full the relay allocates one port per player and every member receives a
``MATCH`` naming its player number, the session token, and every player's
relay port and platform.

After ``MATCH``, version-3 clients send ``PEERS_REQUEST`` from the same
socket to learn every member's hole-punching candidates: the public address the
request arrived from, and the private LAN address the client reported. Only
members holding a live session's token are answered, and the reply is the
request's size.

``Matchmaker.handle`` is pure apart from its own state and the relay pool: it
takes one datagram, its source address and the current time, and returns the
datagrams to send. ``server.py`` only moves bytes.
"""

from collections import OrderedDict
from dataclasses import dataclass

from . import protocol

WAITING_EXPIRY_S = 10.0
MATCH_RESEND_S = 60.0
MAX_WAITING = 1000
RATE_PER_SECOND = 10.0
RATE_BURST = 20.0


@dataclass
class _Client:
    nonce: int
    platform: int
    last_seen: float


@dataclass
class _Candidates:
    public: list                     # player - 1 -> (ip, port) or None
    lan: list


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
    """Groups players by (lobby code, group size) and allocates relay sessions."""

    def __init__(self, relay):
        self._relay = relay
        self._groups = {}               # (lobby, size) -> OrderedDict(address -> _Client)
        self._member_of = {}            # address -> (lobby, size)
        self._assigned = {}             # address -> _Assignment
        self._buckets = {}              # source IP -> _TokenBucket
        self._candidates = {}           # session token -> _Candidates

    @property
    def waiting_count(self):
        return len(self._member_of)

    def handle(self, data, address, now):
        """Return ``[(bytes, address), ...]`` to send in response."""
        self._expire(now)
        try:
            version, message_type = protocol.peek_header(data)
        except protocol.ProtocolError:
            return []
        if (message_type == protocol.PEERS_REQUEST and version == protocol.PEERS_VERSION
                and len(data) == protocol.PEERS_SIZE):
            return self._peers(data, address, now)
        # Only a full-size JOIN earns a reply, so no reply can exceed its request.
        if message_type != protocol.JOIN or len(data) != protocol.JOIN_SIZE:
            return []
        if version != protocol.VERSION:
            nonce = int.from_bytes(data[8:16], "little")
            return [(protocol.encode(protocol.Error(nonce, protocol.ERROR_VERSION_MISMATCH)), address)]
        try:
            join = protocol.decode(data)
        except protocol.ProtocolError:
            return []

        bucket = self._buckets.setdefault(address[0], _TokenBucket(now))
        if not bucket.take(now):
            return [(protocol.encode(protocol.Error(join.nonce, protocol.ERROR_RATE_LIMITED)), address)]

        assignment = self._assigned.get(address)
        if assignment is not None:
            if assignment.nonce == join.nonce:
                return [(assignment.reply, address)]  # The client missed its MATCH.
            del self._assigned[address]             # A restarted client: queue it again.

        key = (join.lobby, join.group_size)
        if self._member_of.get(address) not in (None, key):
            self._leave(address)                    # Changed lobby or group size.
        group = self._groups.setdefault(key, OrderedDict())
        client = group.get(address)
        if client is not None and client.nonce == join.nonce:
            client.last_seen = now
        else:
            if client is None and len(self._member_of) >= MAX_WAITING:
                return [(protocol.encode(protocol.Error(join.nonce, protocol.ERROR_SERVER_FULL)), address)]
            group.pop(address, None)
            group[address] = _Client(join.nonce, join.platform, now)
            self._member_of[address] = key

        if len(group) < join.group_size:
            return [(protocol.encode(protocol.Waiting(join.nonce, len(group), join.group_size)), address)]
        return self._start(key, group, now)

    def _start(self, key, group, now):
        members = list(group.items())[:key[1]]
        session = self._relay.allocate(len(members), now)
        if session is None:
            # No relay ports: everyone keeps waiting and is told why.
            return [(protocol.encode(protocol.Error(c.nonce, protocol.ERROR_SERVER_FULL)), a)
                    for a, c in members]
        platforms = tuple(client.platform for _, client in members)
        self._candidates[session.token] = _Candidates([None] * len(members), [None] * len(members))
        replies = []
        for player, (address, client) in enumerate(members, start=1):
            self._leave(address)
            reply = protocol.encode(protocol.Match(
                client.nonce, session.token, player, session.ports, platforms))
            self._assigned[address] = _Assignment(client.nonce, reply, now + MATCH_RESEND_S)
            replies.append((reply, address))
        return replies

    def _peers(self, data, address, now):
        try:
            request = protocol.decode(data)
        except protocol.ProtocolError:
            return []
        bucket = self._buckets.setdefault(address[0], _TokenBucket(now))
        if not bucket.take(now):
            return []
        session = self._relay.session(request.token)
        candidates = self._candidates.get(request.token)
        # Unknown tokens and out-of-range players get no reply: no oracle.
        if session is None or candidates is None or request.player > session.player_count:
            return []
        index = request.player - 1
        candidates.public[index] = (address[0], address[1])
        lan_ip, lan_port = request.lan
        private = lan_port != 0 and protocol.is_private_lan(lan_ip)
        # Only RFC 1918 LAN addresses are passed on, so a member cannot point
        # the others' probes at an arbitrary host.
        candidates.lan[index] = (lan_ip, lan_port) if private else None
        none = protocol.NO_CANDIDATE
        reply = protocol.Peers(
            request.token,
            tuple(c or none for c in candidates.public),
            tuple(c or none for c in candidates.lan))
        return [(protocol.encode(reply), address)]

    def _leave(self, address):
        key = self._member_of.pop(address, None)
        if key is None:
            return
        group = self._groups.get(key)
        if group is not None:
            group.pop(address, None)
            if not group:
                del self._groups[key]

    def _expire(self, now):
        stale = [address for key, group in self._groups.items()
                 for address, client in group.items() if now - client.last_seen > WAITING_EXPIRY_S]
        for address in stale:
            self._leave(address)
        for address in [a for a, s in self._assigned.items() if now > s.expires]:
            del self._assigned[address]
        for ip in [ip for ip, b in self._buckets.items() if now - b.updated > WAITING_EXPIRY_S]:
            del self._buckets[ip]
        # Candidates live exactly as long as their relay session.
        for token in [t for t in self._candidates if self._relay.session(t) is None]:
            del self._candidates[token]
