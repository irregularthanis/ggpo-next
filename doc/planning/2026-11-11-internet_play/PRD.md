# PRD — Internet play for Vector War

Status: Draft

Source: `4587549` (`master` after v0.6.0)

Scope: let friends on different home networks play Vector War over the
Internet without a VPN, using a small matchmaking and hole-punching server that
this repository builds, deploys with Terraform on a free-tier host, and tests.
Vector War gains a start screen when launched without arguments and an in-game
legend showing each player's platform. Excluded: a relay for networks that
cannot be hole-punched (planned as a follow-up), IPv6, accounts, lobbies with
more than two players, and any GGPO library, public API, ABI, export or wire
format change.

## Motivation

Vector War already plays over the Internet when each peer can reach the other's
exact `IP:PORT`. Behind home routers that rarely holds: a router rewrites the
outgoing port, and neither player knows the address the other must use. Today
the only working setups are port forwarding or an overlay VPN such as Tailscale,
both of which ask non-technical friends to configure something first.

A rendezvous server that tells each peer the other's public `IP:PORT`, followed
by both peers sending at once (UDP hole punching), connects most home routers
with no configuration. It needs only a tiny always-on host, which fits in a
cloud free tier.

## Outcomes

### Infrastructure

- One Terraform configuration creates everything the server needs: a free-tier
  VM, its firewall rules, the container runtime that runs the server image, and
  a DNS name for it.
- The host and DNS cost nothing to run, staying inside always-free tiers.
- The Terraform code is covered by unit tests that run without cloud
  credentials, asserting the planned resources and their attributes, and by
  tests of the resulting state.
- The matchmaking server is a container image built and tested in CI and
  published to a free registry.

### Vector War

- **Launched with arguments:** behavior is unchanged on every platform.
- **Launched without arguments:** a start screen asks for either:
  - a remote `IP:PORT` (plus the local port) to connect directly, or
  - **Matchmaking**, which connects to the matchmaking server shown in an
    editable field. The default server name is compiled into the code.
- **Matchmaking** pairs the next two waiting players, tells each the other's
  public endpoint and player number, and both start the normal GGPO session.
- **Platform legend:** during play, a small legend shows one row per player in
  that player's ship color, naming the platform they run on (Windows, Linux,
  Android). It works for both matchmade and direct sessions.
- **Every client gets Matchmaking**, not only one platform: Windows
  (Win32/GDI), Linux (SDL2) and Android (SDL2 with the existing launcher) all
  show the same start screen choices and the same legend, and any client can be
  matched with any other.

## Users and use cases

| User | Wants to |
| --- | --- |
| Friend with no networking knowledge | Start Vector War, press Matchmaking, and play |
| Developer testing GGPO across the Internet | Connect two specific machines by `IP:PORT` without memorizing the CLI |
| Maintainer | Deploy or update the server with one Terraform apply, at no cost |
| Reviewer | Trust infrastructure changes because tests prove the plan before it is applied |

## Success criteria

- Two players on different home networks (each behind a typical NAT router,
  no port forwarding) connect through Matchmaking and converge for a full
  session.
- In CI, the matchmaking flow converges to the frozen Vector War checksum
  through emulated full-cone and port-restricted NATs, and fails with a clear
  message, not a hang, through an emulated symmetric NAT.
- `terraform test` passes in CI with mocked providers, and a fresh apply on an
  empty account creates a reachable server at the configured DNS name.
- Monthly hosting cost: $0.
- Existing Vector War command lines, tests, release packages and E2E checks
  are unchanged.

## Non-goals

- Relaying traffic for networks that cannot be hole-punched (mobile data,
  carrier-grade NAT). Recorded as follow-up; the server protocol leaves room
  for it.
- IPv6 and hostnames inside the GGPO library (the frozen ABI holds a 32-byte
  IPv4 string). Vector War resolves hostnames itself before calling GGPO.
- Accounts, authentication, persistent lobbies, ranking or chat.
- More than two players per matchmade session.

## Risks

| Risk | Mitigation |
| --- | --- |
| Symmetric NAT or carrier-grade NAT defeats hole punching | Detect and report it clearly; relay follow-up; overlay VPN documented as workaround |
| The NAT mapping is lost between the matchmaking socket closing and GGPO binding the same port | Bind GGPO immediately; keep-alive punches; measured in the NAT emulation tests |
| Free tier terms change or capacity is unavailable | Provider isolated behind one Terraform module; a second provider documented |
| Server abuse (floods, reflection) | Replies never larger than requests, rate limits per source, short session expiry |
| Players' public IP addresses are visible to the server and to each other | Stated in the start screen and docs; server keeps no logs of addresses beyond session expiry |
