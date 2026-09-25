# 05 — Client prediction & reconciliation

**Goal:** make the local player feel instant even though the server is the
authority and is milliseconds away.

## The idea

If the client waited for the server to confirm every move, controls would feel
laggy. So the client **predicts**: it applies your input immediately using the
*same* `simulate_player()` the server runs, and shows the result now.

It also **remembers** every input it applied (and the state it produced) in an
`InputHistory`. Each snapshot from the server includes `last_client_input_tick`
— the most recent input the server has processed.

When a snapshot arrives, the client compares the server's authoritative state at
that tick to what it had predicted:

- **They match** (the normal case): great — just drop the acknowledged inputs
  from history.
- **They differ** (packet loss, or the server did something the client couldn't
  predict): **reconcile** — snap to the server's state, then **replay** all
  inputs newer than that tick on top of it. The player barely notices; the world
  quietly corrects itself.

This only works because the simulation is deterministic (same input + same start
= same result) on both sides.

## Files to read

- `src/game/InputHistory.hpp` — records inputs + predicted states; `has_diverged`,
  `reconcile`, `discard_acknowledged`, and `get_recent_inputs` (used to send the
  last few inputs redundantly so a dropped input packet is usually recoverable).
- `src/game/Simulation.hpp` — the shared deterministic physics (see step 4).
- `src/apps/client.cpp` — the predict → send → reconcile loop.

## Try it

```
./bin/prediction_test   # determinism, divergence detection, replay convergence
```

## What to notice

- **Redundant input batching**: each packet carries the last ~5 inputs, so one
  lost packet rarely costs the server an input.
- Under a good network you'll see **0 reconciliations** — prediction was right.
  Crank packet loss (step 9) and you'll watch reconciliations climb and recover.
