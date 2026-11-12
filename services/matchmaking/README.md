# Vector War lobby and relay server

Groups 2 to 4 Vector War players and relays GGPO's packets between them, so
friends behind any kind of router (including mobile data and carrier-grade NAT)
can play without a VPN. GGPO is unchanged: each client sees every remote player
at `server:relay_port`. See the
[internet play plan](../../doc/planning/2026-11-11-internet_play/TECH_PLAN.md#relay-forwarding).

| Path | Contents |
| --- | --- |
| [`matchmaking/protocol.py`](matchmaking/protocol.py) | Version-2 wire format: `JOIN`, `WAITING`, `MATCH`, `BIND`, `BOUND`, `HELLO`, `ERROR` |
| [`matchmaking/matchmaker.py`](matchmaking/matchmaker.py) | Lobbies: groups of 2-4 by lobby code and size, expiry, rate limits |
| [`matchmaking/relay.py`](matchmaking/relay.py) | Relay port pool, `BIND` latching, forwarding with source rewrite, caps, expiry |
| [`matchmaking/server.py`](matchmaking/server.py) | `asyncio` UDP front end: lobby port plus one socket per relay port |
| [`protocol_vectors.json`](protocol_vectors.json) | Encoded examples shared with the C++ client tests |
| [`tests/`](tests) | Unit tests and real-socket loopback tests for 2 and 4 players, registered in CTest as `matchmaking_server_contract` |

## How a session works

1. Each client sends `JOIN` (group size 2-4, optional lobby code) to the lobby
   port from the UDP port its GGPO session will use.
2. When the group is full, every member gets `MATCH`: its player number, a
   secret session token, and each player's relay port `R[j]` and platform.
3. Each client sends `BIND(token, me)` to every other player's relay port and
   waits for `BOUND`. The relay records the exact source address per port.
4. GGPO starts with each remote player `k` at `server:R[k]`. A packet from
   player `i` arriving at `R[k]` is forwarded unchanged from `R[i]` to player
   `k`, who therefore sees it from the endpoint it configured for player `i`.

## Run locally

```bash
python3 -m matchmaking --port 7100 --relay-ports 7101-7499   # from this directory
python3 -m unittest discover -s tests -t .                   # tests
docker build -t ggpo-matchmaking .                           # container
docker run --rm --network host --read-only ggpo-matchmaking
```

## Rules the tests enforce

- Only a full 64-byte `JOIN` gets a lobby reply and every reply is smaller;
  `BOUND` is the same size as `BIND`; forwarding never sends more bytes than it
  received. Nothing amplifies traffic.
- Groups form first come, first served within one lobby code and size; earlier
  joiners get lower player numbers.
- Only addresses that bound with the session token are forwarded, only to
  other members of the same session; per-player forwarding rate cap.
- Waiting clients expire after 10 s without a `JOIN`; sessions after 30 s
  without traffic or 2 hours in total, and their ports return to the pool.
- No player addresses are logged.
