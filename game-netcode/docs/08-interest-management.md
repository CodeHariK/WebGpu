# 08 — Interest management (area of interest)

**Goal:** stop telling each client about entities it can't see, so bandwidth
scales with what a player perceives — not with the size of the whole world.

## The idea

In a big world a client only cares about what's near it. So per client, the
server computes a **visible set** — the player plus every entity within an
interest radius (with an optional hard cap of the nearest N) — and only sends
that. Entities that wander out of range simply stop being sent; ones that wander
in appear as "new".

This stacks with delta compression (step 6): fewer entities per snapshot, and
each one still delta-encoded. The catch it introduces: the **delta baseline must
be per client**, because each client received a different subset. So the server
now stores, per client, the exact subset it sent at each tick and deltas against
that.

## Files to read

- `src/game/InterestManagement.hpp` — `compute_visible_set(viewer, entities,
  radius, max_count)`.
- `src/apps/server.cpp` — per-client `sent_history` (the per-client baselines) and
  the AOI filter in the broadcast loop.
- `src/apps/client.cpp` — targets the nearest *visible* entity; prints a `Visible`
  count.

## Try it

```
./bin/aoi_test   # in/out of radius, viewer always included, nearest-N cap
make run         # watch the client's "Visible" count rise and fall
```

## What to notice

- The lag-comp history (step 7) stays **full-world**; only what's *sent* is
  culled. Hit tests are still fair for anything the client could see.
- `max_count` is a hard ceiling: even in a crowd, a snapshot can't blow the
  bandwidth budget.
