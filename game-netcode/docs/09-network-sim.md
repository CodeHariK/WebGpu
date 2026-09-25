# 09 — Network simulator in the live loop

**Goal:** actually experience latency, jitter, and loss, so every earlier
technique can be seen doing its job.

## The idea

Steps 1–8 are hard to appreciate on localhost, where the network is perfect. So
we let a `Connection` route its **outgoing** packets through the
`NetworkSimulator` from step 1 instead of straight to the socket. With a
simulator attached, packets are delayed/jittered/dropped/duplicated before they
hit the wire; the app calls the simulator's `update()` each frame to flush the
ones whose delivery time has come. With no simulator attached, nothing changes —
it's a transparent passthrough.

Both sides can be impaired independently: the server's connections impair
server→client, the client's connection impairs client→server.

## Files to read

- `src/net/Connection.hpp/.cpp` — `set_network_simulator(sim)` and the two send
  sites that route through it when set.
- `src/game/SimConfig.hpp` — reads `NETSIM_LATENCY_MS`, `NETSIM_JITTER_MS`,
  `NETSIM_LOSS`, `NETSIM_DUP` from the environment.
- `src/apps/server.cpp` / `client.cpp` — attach a simulator when those env vars
  are set, and flush it each frame.

## Try it

```
./bin/netsim_test    # a Connection with a simulator delays delivery, and drops at 100% loss
make run-sim         # server + client under 80ms +/-20ms, 5% loss, 2% dup
make run-sim NETSIM_LOSS=0.5 NETSIM_JITTER_MS=30   # crank it to force reconciliations
```

## What to notice

- Under a mild link: ping climbs (~180 ms round trip) but hits still land — the
  redundant input batches absorb the loss, so reconciliations stay near 0.
- Under 50% loss: reconciliations start firing and recovering, yet hit rate stays
  high thanks to lag compensation and the reliable channel. That's every earlier
  milestone paying off at once.
