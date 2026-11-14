"""Vector War matchmaking and relay wire protocol, versions 2 and 3.

Every datagram starts with an 8-byte header: the magic ``VWMM``, the protocol
version, the message type and two reserved zero bytes. Integers are
little-endian. The C++ client mirrors these layouts; ``protocol_vectors.json``
holds encoded examples that both sides' tests decode.

No reply is ever larger than the request that caused it: ``JOIN`` is padded to
64 bytes and ``MATCH`` is 44; ``BOUND`` is exactly the size of ``BIND``. The
server therefore cannot amplify a spoofed flood.

Version 3 adds only hole-punching candidates: ``PEERS_REQUEST`` and ``PEERS``
carry version 3 in their header and are the same size, so the rule still
holds. Every other message stays version 2; a version-2 server never answers
a ``PEERS_REQUEST``, and the client then stays on the relay.
"""

import ipaddress

import struct
from dataclasses import dataclass

MAGIC = b"VWMM"
VERSION = 2
PEERS_VERSION = 3
MAX_PLAYERS = 4

HEADER = struct.Struct("<4sBBH")

JOIN = 1
WAITING = 2
MATCH = 3
BIND = 4
BOUND = 5
HELLO = 6
ERROR = 7
PEERS_REQUEST = 8
PEERS = 9

PLATFORM_UNKNOWN = 0
PLATFORM_WINDOWS = 1
PLATFORM_LINUX = 2
PLATFORM_ANDROID = 3
PLATFORM_NAMES = {
    PLATFORM_UNKNOWN: "unknown",
    PLATFORM_WINDOWS: "Windows",
    PLATFORM_LINUX: "Linux",
    PLATFORM_ANDROID: "Android",
}

ERROR_VERSION_MISMATCH = 1
ERROR_RATE_LIMITED = 2
ERROR_SERVER_FULL = 3

JOIN_SIZE = 64
LOBBY_CODE_SIZE = 8
LOBBY_ALPHABET = frozenset(b"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")

_JOIN_BODY = struct.Struct("<QBBH8s")     # nonce, platform, group size, reserved, lobby code
_WAITING_BODY = struct.Struct("<QBBH")    # nonce, players joined, group size, reserved
_MATCH_BODY = struct.Struct("<QQBBH")     # nonce, session token, player, player count, reserved
_MATCH_SLOT = struct.Struct("<HBB")       # relay port, platform, reserved (x MAX_PLAYERS)
_BIND_BODY = struct.Struct("<QB3x")       # session token, sender player
_HELLO_BODY = struct.Struct("<QB3x")      # sender nonce, sender platform
_ERROR_BODY = struct.Struct("<QB3x")      # nonce, error code
_PEERS_REQUEST_BODY = struct.Struct("<QB3x4sH2x")  # token, sender player, LAN IPv4, LAN port
_PEERS_BODY = struct.Struct("<QB3x")      # session token, player count
_PEERS_SLOT = struct.Struct("<4sH4sH")    # public IPv4, port, LAN IPv4, port (x MAX_PLAYERS)

MATCH_SIZE = HEADER.size + _MATCH_BODY.size + MAX_PLAYERS * _MATCH_SLOT.size
BIND_SIZE = HEADER.size + _BIND_BODY.size
PEERS_SIZE = HEADER.size + _PEERS_BODY.size + MAX_PLAYERS * _PEERS_SLOT.size
NO_CANDIDATE = ("0.0.0.0", 0)


class ProtocolError(ValueError):
    """A datagram that is not a valid protocol message."""


def _pack_ip(address):
    try:
        return ipaddress.IPv4Address(address).packed
    except ValueError:
        raise ValueError(f"not an IPv4 address: {address!r}") from None


def _unpack_ip(raw):
    return str(ipaddress.IPv4Address(raw))


def is_private_lan(address):
    """True for RFC 1918 addresses, the only LAN candidates the server relays."""
    ip = ipaddress.IPv4Address(address)
    return any(ip in ipaddress.IPv4Network(net)
               for net in ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16"))


def normalize_lobby(code):
    """Return the 8-byte wire form of a lobby code ('' means the public queue)."""
    try:
        raw = code.strip().upper().encode("ascii") if isinstance(code, str) else bytes(code)
    except UnicodeEncodeError:
        raise ProtocolError("lobby code must be up to 8 letters or digits") from None
    raw = raw.rstrip(b"\x00")
    if len(raw) > LOBBY_CODE_SIZE or any(c not in LOBBY_ALPHABET for c in raw):
        raise ProtocolError("lobby code must be up to 8 letters or digits")
    return raw.ljust(LOBBY_CODE_SIZE, b"\x00")


@dataclass(frozen=True)
class Join:
    nonce: int
    platform: int
    group_size: int
    lobby: str = ""


@dataclass(frozen=True)
class Waiting:
    nonce: int
    joined: int
    group_size: int


@dataclass(frozen=True)
class Match:
    nonce: int
    token: int
    player: int
    relay_ports: tuple
    platforms: tuple


@dataclass(frozen=True)
class Bind:
    token: int
    player: int


@dataclass(frozen=True)
class Bound:
    token: int
    player: int


@dataclass(frozen=True)
class Hello:
    nonce: int
    platform: int


@dataclass(frozen=True)
class Error:
    nonce: int
    code: int


@dataclass(frozen=True)
class PeersRequest:
    """Version 3: ask for every member's hole-punching candidates and report
    this client's LAN address (``0.0.0.0``/0 when none). The server records
    the request's source address as this player's public candidate."""
    token: int
    player: int
    lan: tuple = NO_CANDIDATE


@dataclass(frozen=True)
class Peers:
    """Version 3: public and LAN ``(ip, port)`` per player; unknown slots are
    ``0.0.0.0``/0 until that player has sent its own request."""
    token: int
    public: tuple
    lan: tuple


def _header(message_type, version=VERSION):
    return HEADER.pack(MAGIC, version, message_type, 0)


def encode(message):
    """Encode one message object to its exact wire bytes."""
    if isinstance(message, Join):
        body = _JOIN_BODY.pack(message.nonce, message.platform, message.group_size, 0,
                               normalize_lobby(message.lobby))
        data = _header(JOIN) + body
        return data + bytes(JOIN_SIZE - len(data))
    if isinstance(message, Waiting):
        return _header(WAITING) + _WAITING_BODY.pack(message.nonce, message.joined, message.group_size, 0)
    if isinstance(message, Match):
        count = len(message.relay_ports)
        if not 2 <= count <= MAX_PLAYERS or len(message.platforms) != count:
            raise ValueError("MATCH needs 2-4 players with one platform each")
        slots = b"".join(_MATCH_SLOT.pack(port, platform, 0)
                         for port, platform in zip(message.relay_ports, message.platforms))
        slots += bytes((MAX_PLAYERS - count) * _MATCH_SLOT.size)
        return _header(MATCH) + _MATCH_BODY.pack(
            message.nonce, message.token, message.player, count, 0) + slots
    if isinstance(message, Bind):
        return _header(BIND) + _BIND_BODY.pack(message.token, message.player)
    if isinstance(message, Bound):
        return _header(BOUND) + _BIND_BODY.pack(message.token, message.player)
    if isinstance(message, Hello):
        return _header(HELLO) + _HELLO_BODY.pack(message.nonce, message.platform)
    if isinstance(message, Error):
        return _header(ERROR) + _ERROR_BODY.pack(message.nonce, message.code)
    if isinstance(message, PeersRequest):
        ip, port = message.lan
        data = _header(PEERS_REQUEST, PEERS_VERSION) + _PEERS_REQUEST_BODY.pack(
            message.token, message.player, _pack_ip(ip), port)
        return data + bytes(PEERS_SIZE - len(data))
    if isinstance(message, Peers):
        count = len(message.public)
        if not 2 <= count <= MAX_PLAYERS or len(message.lan) != count:
            raise ValueError("PEERS needs 2-4 players with public and LAN candidates")
        slots = b"".join(_PEERS_SLOT.pack(_pack_ip(pub[0]), pub[1], _pack_ip(lan[0]), lan[1])
                         for pub, lan in zip(message.public, message.lan))
        slots += bytes((MAX_PLAYERS - count) * _PEERS_SLOT.size)
        return _header(PEERS, PEERS_VERSION) + _PEERS_BODY.pack(message.token, count) + slots
    raise TypeError(f"not a protocol message: {message!r}")


def peek_header(data):
    """Return ``(version, type)`` if ``data`` carries the magic, else raise."""
    if len(data) < HEADER.size:
        raise ProtocolError("datagram shorter than the header")
    magic, version, message_type, reserved = HEADER.unpack_from(data)
    if magic != MAGIC:
        raise ProtocolError("bad magic")
    if reserved != 0:
        raise ProtocolError("reserved header bytes must be zero")
    return version, message_type


def decode(data):
    """Decode exact wire bytes; raise ``ProtocolError`` otherwise.

    ``PEERS_REQUEST`` and ``PEERS`` must carry version 3, every other message
    version 2."""
    version, message_type = peek_header(data)
    expected = PEERS_VERSION if message_type in (PEERS_REQUEST, PEERS) else VERSION
    if version != expected:
        raise ProtocolError(f"unsupported version {version} for type {message_type}")
    body = data[HEADER.size:]

    def exact(layout):
        if len(body) != layout.size:
            raise ProtocolError(f"type {message_type}: expected {layout.size}-byte body, got {len(body)}")
        return layout.unpack(body)

    if message_type == JOIN:
        if len(data) != JOIN_SIZE:
            raise ProtocolError(f"JOIN must be {JOIN_SIZE} bytes")
        nonce, platform, group_size, reserved, lobby = _JOIN_BODY.unpack_from(body)
        if reserved != 0 or any(body[_JOIN_BODY.size:]):
            raise ProtocolError("JOIN padding must be zero")
        if not 2 <= group_size <= MAX_PLAYERS:
            raise ProtocolError("group size must be 2-4")
        code = normalize_lobby(lobby).rstrip(b"\x00").decode("ascii")
        return Join(nonce, platform, group_size, code)
    if message_type == WAITING:
        nonce, joined, group_size, _ = exact(_WAITING_BODY)
        return Waiting(nonce, joined, group_size)
    if message_type == MATCH:
        if len(data) != MATCH_SIZE:
            raise ProtocolError(f"MATCH must be {MATCH_SIZE} bytes")
        nonce, token, player, count, _ = _MATCH_BODY.unpack_from(body)
        if not 2 <= count <= MAX_PLAYERS or not 1 <= player <= count:
            raise ProtocolError("invalid player number or count")
        slots = [_MATCH_SLOT.unpack_from(body, _MATCH_BODY.size + i * _MATCH_SLOT.size)
                 for i in range(MAX_PLAYERS)]
        if any(any(slot) for slot in slots[count:]):
            raise ProtocolError("unused MATCH slots must be zero")
        return Match(nonce, token, player,
                     tuple(port for port, _, _ in slots[:count]),
                     tuple(platform for _, platform, _ in slots[:count]))
    if message_type in (BIND, BOUND):
        token, player = exact(_BIND_BODY)
        if not 1 <= player <= MAX_PLAYERS:
            raise ProtocolError("invalid player number")
        return (Bind if message_type == BIND else Bound)(token, player)
    if message_type == HELLO:
        return Hello(*exact(_HELLO_BODY))
    if message_type == ERROR:
        return Error(*exact(_ERROR_BODY))
    if message_type == PEERS_REQUEST:
        if len(data) != PEERS_SIZE:
            raise ProtocolError(f"PEERS_REQUEST must be {PEERS_SIZE} bytes")
        token, player, lan_ip, lan_port = _PEERS_REQUEST_BODY.unpack_from(body)
        if any(body[_PEERS_REQUEST_BODY.size:]) or any(body[9:12]) or any(body[18:20]):
            raise ProtocolError("PEERS_REQUEST padding must be zero")
        if not 1 <= player <= MAX_PLAYERS:
            raise ProtocolError("invalid player number")
        return PeersRequest(token, player, (_unpack_ip(lan_ip), lan_port))
    if message_type == PEERS:
        if len(data) != PEERS_SIZE:
            raise ProtocolError(f"PEERS must be {PEERS_SIZE} bytes")
        token, count = _PEERS_BODY.unpack_from(body)
        if any(body[9:12]):
            raise ProtocolError("PEERS reserved bytes must be zero")
        if not 2 <= count <= MAX_PLAYERS:
            raise ProtocolError("invalid player count")
        slots = [_PEERS_SLOT.unpack_from(body, _PEERS_BODY.size + i * _PEERS_SLOT.size)
                 for i in range(MAX_PLAYERS)]
        if any(body[_PEERS_BODY.size + count * _PEERS_SLOT.size:]):
            raise ProtocolError("unused PEERS slots must be zero")
        return Peers(token,
                     tuple((_unpack_ip(ip), port) for ip, port, _, _ in slots[:count]),
                     tuple((_unpack_ip(ip), port) for _, _, ip, port in slots[:count]))
    raise ProtocolError(f"unknown message type {message_type}")
