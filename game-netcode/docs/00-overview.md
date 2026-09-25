# Game Netcode from Scratch — Learning Guide

This project builds a small but real game-networking stack in C++20, one concept
at a time. Each numbered doc below explains **one idea**: the problem it solves,
how it works, which files to read, and how to run it.

## How the code is laid out

```
src/net/     The reusable networking library (not game-specific):
             sockets, virtual connection, reliability, channels,
             bitstream, network simulator.
src/game/    Game-specific logic, all header-only so it's easy to read:
             world types, deterministic simulation, client prediction,
             delta compression, lag compensation, interest management.
src/apps/    The two demo programs: server.cpp and client.cpp.
tests/       One small program per concept. Run them with `make test`.
docs/        You are here.
```

A good mental model: **`src/net/` is the "engine"** you could reuse for any game;
**`src/game/` is this particular game** using that engine; **`src/apps/` wires
them together** into a running server and client.

## Suggested reading order

1. [01 — Sockets & the network simulator](01-sockets.md)
2. [02 — Virtual connection & reliability](02-connection.md)
3. [03 — Channels (delivery guarantees)](03-channels.md)
4. [04 — Authoritative server & snapshots](04-snapshots.md)
5. [05 — Client prediction & reconciliation](05-prediction.md)
6. [06 — Bit packing & delta compression](06-compression.md)
7. [07 — Lag compensation (hit rewind)](07-lag-compensation.md)
8. [08 — Interest management (area of interest)](08-interest-management.md)
9. [09 — Network simulator in the live loop](09-network-sim.md)

Also see [CODE_REVIEW.md](CODE_REVIEW.md) for notes on the code's strengths,
rough edges, and where to simplify as you learn.

## Building & running

```
make            # build apps + tests
make test       # build and run every test (each prints [PASS] lines)
make run        # server + client on a clean local network
make run-sim    # server + client under a simulated bad network
make clean      # delete build/ and bin/
```

Everything is plain C++20 + a Makefile — no external dependencies.

## The big picture (how a frame flows)

```
        client                                   server
   ┌──────────────┐                        ┌──────────────────┐
   │ read input   │ ── input (ch STATE) ─▶ │ apply input      │
   │ predict move │                        │ (authoritative)  │
   │ (immediate)  │ ◀─ snapshot (delta) ── │ broadcast world  │
   │ reconcile if │                        │ (AOI-culled)     │
   │ server differs                        │                  │
   │ fire (ch REL)│ ── FireCommand ──────▶ │ rewind + hit-test│
   │ show hit     │ ◀─ HitNotification ─── │ (lag comp)       │
   └──────────────┘                        └──────────────────┘
   every packet may be delayed/dropped by the network simulator (M9)
```
