"""Vector War matchmaking wire protocol, version 1.

Every datagram starts with an 8-byte header: the magic ``VWMM``, the protocol
version, the message type and two reserved zero bytes. Integers are
little-endian. The C++ client mirrors these layouts; ``protocol_vectors.json``
holds encoded examples that both sides' tests decode.

A server reply is never larger than the request that caused it: ``REGISTER``
is padded to 64 bytes and every server message is shorter, so the server
cannot amplify a spoofed flood.
"""

import ipaddress
import struct
from dataclasses import dataclass

MAGIC = b"VWMM"
VERSION = 1

HEADER = struct.Struct("<4sBBH")

REGISTER = 1
WAITING = 2
MATCH = 3
PUNCH = 4
HELLO = 5
ERROR = 6
RELAY = 7  # Reserved for a future relay; never sent by version 1.

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

REGISTER_SIZE = 64

_REGISTER_BODY = struct.Struct("<QBBH")    # nonce, platform, reserved, build
_WAITING_BODY = struct.Struct("<QHH")      # nonce, queue position, reserved
_MATCH_BODY = struct.Struct("<QQBBH4s")    # nonce, session, player, peer platform, peer port, peer IPv4
_PUNCH_BODY = struct.Struct("<QQB3x")      # session, sender nonce, sender platform
_HELLO_BODY = struct.Struct("<QB3x")       # sender nonce, sender platform
_ERROR_BODY = struct.Struct("<QB3x")       # nonce, error code


class ProtocolError(ValueError):
    """A datagram that is not a valid version-1 message."""


@dataclass(frozen=True)
class Register:
    nonce: int
    platform: int
    build: int


@dataclass(frozen=True)
class Waiting:
    nonce: int
    position: int


@dataclass(frozen=True)
class Match:
    nonce: int
    session: int
    player: int
    peer_platform: int
    peer_ip: str
    peer_port: int


@dataclass(frozen=True)
class Punch:
    session: int
    nonce: int
    platform: int


@dataclass(frozen=True)
class Hello:
    nonce: int
    platform: int


@dataclass(frozen=True)
class Error:
    nonce: int
    code: int


def _header(message_type):
    return HEADER.pack(MAGIC, VERSION, message_type, 0)


def encode(message):
    """Encode one message object to its exact wire bytes."""
    if isinstance(message, Register):
        body = _REGISTER_BODY.pack(message.nonce, message.platform, 0, message.build)
        data = _header(REGISTER) + body
        return data + bytes(REGISTER_SIZE - len(data))
    if isinstance(message, Waiting):
        return _header(WAITING) + _WAITING_BODY.pack(message.nonce, message.position, 0)
    if isinstance(message, Match):
        address = ipaddress.IPv4Address(message.peer_ip).packed
        return _header(MATCH) + _MATCH_BODY.pack(
            message.nonce, message.session, message.player,
            message.peer_platform, message.peer_port, address)
    if isinstance(message, Punch):
        return _header(PUNCH) + _PUNCH_BODY.pack(
            message.session, message.nonce, message.platform)
    if isinstance(message, Hello):
        return _header(HELLO) + _HELLO_BODY.pack(message.nonce, message.platform)
    if isinstance(message, Error):
        return _header(ERROR) + _ERROR_BODY.pack(message.nonce, message.code)
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
    """Decode exact version-1 wire bytes; raise ``ProtocolError`` otherwise."""
    version, message_type = peek_header(data)
    if version != VERSION:
        raise ProtocolError(f"unsupported version {version}")
    body = data[HEADER.size:]

    def exact(layout):
        if len(body) != layout.size:
            raise ProtocolError(f"type {message_type}: expected {layout.size}-byte body, got {len(body)}")
        return layout.unpack(body)

    if message_type == REGISTER:
        if len(data) != REGISTER_SIZE:
            raise ProtocolError(f"REGISTER must be {REGISTER_SIZE} bytes")
        nonce, platform, reserved, build = _REGISTER_BODY.unpack_from(body)
        if reserved != 0 or any(body[_REGISTER_BODY.size:]):
            raise ProtocolError("REGISTER padding must be zero")
        return Register(nonce, platform, build)
    if message_type == WAITING:
        nonce, position, _ = exact(_WAITING_BODY)
        return Waiting(nonce, position)
    if message_type == MATCH:
        nonce, session, player, peer_platform, peer_port, address = exact(_MATCH_BODY)
        if player not in (1, 2):
            raise ProtocolError("player number must be 1 or 2")
        return Match(nonce, session, player, peer_platform,
                     str(ipaddress.IPv4Address(address)), peer_port)
    if message_type == PUNCH:
        return Punch(*exact(_PUNCH_BODY))
    if message_type == HELLO:
        return Hello(*exact(_HELLO_BODY))
    if message_type == ERROR:
        return Error(*exact(_ERROR_BODY))
    raise ProtocolError(f"unknown message type {message_type}")
