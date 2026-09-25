# 04 — Authoritative server & snapshots

**Goal:** have one source of truth for the game world, and show it smoothly on
clients even though updates arrive only ~20 times per second.

## The idea

The **server is authoritative**: it runs the real simulation at 60 Hz and no
client can cheat by lying about its position. Every few ticks the server
**broadcasts a snapshot** of the world (each entity's position/velocity) at a
lower rate (here 20 Hz) to save bandwidth.

A client therefore only hears about *other* players 20 times a second, at
irregular arrival times. If it drew them the instant each snapshot arrived,
they'd stutter. So the client renders remote entities **~100 ms in the past** and
**interpolates** between the two snapshots that straddle that render time. The
small delay buys smoothness — you're always drawing between two known points
rather than guessing.

## Files to read

- `src/game/GameTypes.hpp` — `Vec2`, `EntityState` (id, position, velocity,
  color), and the wire structs. Note `#pragma pack` so structs have a fixed
  byte layout on the wire.
- `src/game/Simulation.hpp` — `simulate_player()`: the movement/physics. It's
  deliberately simple and **deterministic** (crucial for step 5).
- `src/game/SnapshotBuffer.hpp` — stores recent snapshots and interpolates remote
  entity positions at `now - interpolation_delay`.
- `src/apps/server.cpp` — the 60 Hz loop, snapshot broadcast at 20 Hz.

## Try it

There's no standalone snapshot test, but the physics determinism this relies on
is checked by `./bin/prediction_test`, and you can watch snapshots flow with
`make run`.

## What to notice

- **Tick rate (60 Hz sim) vs. snapshot rate (20 Hz send)** are different on
  purpose: simulate finely, send coarsely.
- Interpolation trades a little latency for a lot of smoothness — a very common
  trade in games.
- In the current demo the client consumes decoded snapshots directly for its
  local player; `SnapshotBuffer` is the piece you'd use to interpolate *other*
  players when you add rendering.
