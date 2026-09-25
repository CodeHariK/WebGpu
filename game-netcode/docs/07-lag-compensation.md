# 07 — Lag compensation (hit rewind)

**Goal:** make shooting fair. If you clearly hit a moving target on your screen,
it should count — even though the server sees the target somewhere else by the
time your shot arrives.

## The idea

Because of interpolation (step 4) and latency, a client sees other entities **in
the past**. If the server hit-tested against the *present* world, players would
have to "lead" every shot and obvious hits would miss.

So the server keeps a short **history** of where every entity was on each tick.
When a shot arrives, the server **rewinds** the world to the moment the shooter
was actually looking at, runs the hit test there, then continues. This is the
"what you see is what you get" model used by Valve's Source engine.

The client stamps each shot with `view_server_tick` (the snapshot it was looking
at), so the server rewinds to exactly that tick rather than guessing.

## Files to read

- `src/game/LagCompensation.hpp` —
  - `WorldHistory`: a ring buffer of time-stamped world states; look up by exact
    tick (`get_at_tick`) or interpolate by time (`sample`).
  - `ray_circle_intersect` / `hitscan`: the actual hit test against circular
    hitboxes, nearest-first, skipping the shooter.
- `src/game/GameTypes.hpp` — `FireCommand` (client→server) and `HitNotification`
  (server→client), sent on the reliable channel.
- `src/apps/server.cpp` — records history each tick; on a `FireCommand`, rewinds
  and hit-tests. `src/apps/client.cpp` — auto-fires at the nearest visible target.

## Try it

```
./bin/lagcomp_test   # includes the key test: a shot MISSES against the present
                     # world but HITS after rewinding to what the shooter saw
make run             # watch the client hit a fast-moving bot
```

## What to notice

- The rewind buffer is the **full** world and is kept separate from what each
  client is *sent* (step 8), so culling never affects hit fairness.
- On localhost the rewind is tiny (~ms); run under `make run-sim` to see it
  rewind further as latency grows.
