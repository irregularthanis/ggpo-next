# Vector War matchmaking server

Pairs two waiting Vector War players and tells each the other's public
`IP:PORT`, so both can open their routers by sending to each other (UDP hole
punching) before GGPO starts. It never relays game traffic. See the
[internet play plan](../../doc/planning/2026-11-11-internet_play/TECH_PLAN.md).

| Path | Contents |
| --- | --- |
| [`matchmaking/protocol.py`](matchmaking/protocol.py) | Version-1 wire format |
| [`matchmaking/matchmaker.py`](matchmaking/matchmaker.py) | Pairing, expiry, rate limits; pure and clock-injected |
| [`matchmaking/server.py`](matchmaking/server.py) | `asyncio` UDP front end |
| [`protocol_vectors.json`](protocol_vectors.json) | Encoded examples shared with the C++ client tests |
| [`tests/`](tests) | Unit and loopback tests, registered in CTest as `matchmaking_server_contract` |

Run locally:

```bash
python3 -m matchmaking --port 7100          # from this directory
python3 -m unittest discover -s tests -t .  # tests
docker build -t ggpo-matchmaking .          # container
docker run --rm -p 7100:7100/udp --read-only ggpo-matchmaking
```

Rules the tests enforce:

- Only a full 64-byte `REGISTER` gets a reply, and every reply is smaller, so
  the server cannot amplify spoofed traffic.
- First come, first served: the earlier registrant is player 1.
- Waiting clients expire after 10 s without a `REGISTER`; matches after 60 s.
- Per-source-IP rate limit; a capped waiting queue.
- No player addresses are logged.
