# PRD — Internet play for Vector War

Status: Draft

Source: `4587549` (`master` after v0.6.0)

Scope: let 2 to 4 friends on different networks play Vector War over the
Internet without a VPN, through a small lobby and relay server that this
repository builds and tests, and that a maintainer brings up and tears down by
hand with Terraform on a free-tier host. Vector War gains a start screen when
launched without arguments and an in-game legend showing each player's
platform. Sessions start on the relay and then use UDP hole punching to move
each pair of players to a direct path when one works, keeping the relay as a
live fallback. Excluded: a
server-run simulation, IPv6, accounts, and any GGPO library, public API, ABI,
export or wire format change.

## Motivation

Vector War already plays over the Internet when each peer can reach the other's
exact `IP:PORT`. Behind home routers that rarely holds: a router rewrites the
outgoing port, and neither player knows the address the other must use. Today
the only working setups are port forwarding or an overlay VPN such as Tailscale,
both of which ask non-technical friends to configure something first.

A relay server solves this for every kind of router, including mobile data and
carrier-grade NAT where hole punching fails: each player only sends to the
server, which forwards GGPO's packets unchanged to the other players. Each
client sees every remote player at `server:port`, so GGPO's peer-to-peer
rollback works exactly as it does on a LAN. It needs only a tiny host while
people play, which fits in a cloud free tier.

## Outcomes

### Infrastructure

- One Terraform configuration creates everything the server needs: a free-tier
  VM, its firewall rules, the container runtime that runs the server image, and
  a DNS name for it.
- The host and DNS cost nothing to run, staying inside always-free tiers.
- The Terraform code is covered by unit tests that run without cloud
  credentials, asserting the planned resources and their attributes, and by
  tests of the resulting state.
- Automatic CI never creates real infrastructure. The server is applied and
  destroyed at will, from a maintainer's machine or with one click on the
  manual, non-blocking **Deploy matchmaking server** workflow.
- The server keeps its own DuckDNS record current after every boot; destroying
  it clears the record.
- The matchmaking server is a container image built and tested in CI and
  published to a free registry.

### Vector War

- **Launched with arguments:** behavior is unchanged on every platform.
- **Launched without arguments:** a start screen asks for either:
  - a remote `IP:PORT` (plus the local port) to connect directly, or
  - **Matchmaking**, which connects to the matchmaking server shown in an
    editable field. The default server name is compiled into the code.
- **Matchmaking** groups 2, 3 or 4 players, either the next ones waiting for
  that group size or the friends who typed the same lobby code. Each gets a
  player number and the relay address of every other player, and all start the
  normal GGPO peer-to-peer session through the relay.
- **Platform legend:** during play, a small legend shows one row per player in
  that player's ship color, naming the platform they run on (Windows, Linux,
  Android). It works for both matchmade and direct sessions.
- **Every client gets Matchmaking**, not only one platform: Windows
  (Win32/GDI), Linux (SDL2) and Android (SDL2 with the existing launcher) all
  show the same start screen choices and the same legend, and any client can be
  matched with any other.

### Direct paths with relay fallback

- Every session still starts on the relay, so it works behind any router.
- In the background each client tests a direct path to every other player
  (public address from the server, plus LAN address for players on the same
  network). A pair switches to the direct path once it is authenticated and
  measurably faster, and switches back to the relay as soon as it degrades.
- Symmetric and carrier-grade NAT pairs never find a direct path and stay on
  the relay, with no change in behavior.
- GGPO is untouched: an in-process path proxy in Vector War sits between GGPO
  and the network.
- Gameplay traffic through the server drops for every pair that goes direct;
  the server remains required to start a session and as the fallback route.

## Users and use cases

| User | Wants to |
| --- | --- |
| Friend with no networking knowledge | Start Vector War, press Matchmaking, and play |
| Group of friends | Agree on a lobby code, all press Matchmaking with 2–4 players, and play together |
| Developer testing GGPO across the Internet | Connect two specific machines by `IP:PORT` without memorizing the CLI |
| Maintainer | Bring the server up before a play session and destroy it afterwards, each with one Terraform command or one click in GitHub Actions, at no cost |
| Reviewer | Trust infrastructure changes because tests prove the plan before it is applied |

## Success criteria

- Two and four players on different networks (home routers and mobile data,
  no port forwarding) connect through Matchmaking and converge for a full
  session.
- In CI, 2- and 4-player matchmade sessions converge to the frozen Vector War
  checksum through every emulated router type: full-cone, port-restricted and
  symmetric NAT.
- `terraform test` passes in CI with mocked providers, and a fresh apply on an
  empty account (locally or from the deploy workflow) creates a server that
  answers at the configured DNS name; the workflow verifies both the DNS update
  and the lobby answer.
- Monthly hosting cost: $0.
- Existing Vector War command lines, tests, release packages and E2E checks
  are unchanged.

## Non-goals

- Port prediction for symmetric NAT. Pairs whose direct path cannot be
  established simply stay on the relay.
- A server that runs the simulation (authoritative host for desync detection,
  late join or replays). It needs a new GGPO backend and is a separate plan.
- IPv6 and hostnames inside the GGPO library (the frozen ABI holds a 32-byte
  IPv4 string). Vector War resolves hostnames itself before calling GGPO.
- Accounts, authentication, persistent lobbies, ranking or chat.
- More than four players (GGPO's `GGPO_MAX_PLAYERS`).

## Risks

| Risk | Mitigation |
| --- | --- |
| The relay adds latency (every packet detours through the server) | Direct path negotiation moves each pair off the relay when a direct path is healthy; pick a central region for the rest; show ping and the active path in game |
| A direct path breaks mid-match (network change, NAT rebinding) | The relay stays bound and kept alive; a pair falls back within about half a second and rollback absorbs the gap |
| Probes used to hijack a player's traffic | Probes carry a MAC keyed by the session token; a path is used only after an authenticated round trip |
| The relay mapping is lost between the client's `BIND` socket closing and GGPO binding the same port | Bind GGPO immediately; GGPO's 200 ms keep-alives refresh it; measured in the NAT emulation tests |
| Relay used as an open proxy | Forwards only between addresses that bound with the session's secret token, only to other members, never more bytes than it received; sessions expire |
| Free tier terms change or capacity is unavailable | Provider isolated behind one Terraform module; a second provider documented |
| Server abuse (floods, reflection) | Replies never larger than requests, rate limits per source, per-player forwarding caps, short session expiry |
| Players' public IP addresses are visible to the server and to each other | Stated in the start screen and docs; server keeps no logs of addresses beyond session expiry |
