# 01 — Sockets & the network simulator

**Goal:** send and receive bytes between two machines without blocking the game
loop, and be able to test under bad network conditions.

## The idea

Games use **UDP**, not TCP. UDP is connectionless "fire and forget": no
handshake, no automatic retransmission, no ordering. That sounds worse than TCP,
but it's what games want — a late packet is useless in a 60 Hz game, so we'd
rather drop it and move on than wait for TCP to redeliver it in order. We build
back exactly the guarantees we need (and nothing more) in later steps.

The socket is **non-blocking**: `receive()` returns immediately with 0 if no
packet is waiting, so the game loop never stalls waiting on the network.

## Files to read

- `src/net/Address.hpp/.cpp` — an IP + port value type (with a hash so it can be
  a map key).
- `src/net/Socket.hpp/.cpp` — a thin RAII wrapper over a non-blocking UDP socket:
  `open(port)`, `send(addr, data, size)`, `receive(sender, buf, cap)`.
- `src/core/Timer.hpp` — monotonic clock (`now_seconds()`) and `sleep_ms()`.
- `src/net/NetworkSimulator.hpp/.cpp` — wraps a socket to inject latency, jitter,
  packet loss, and duplication. Used for testing (and, in step 9, in the live
  loop).

## How the simulator works

You call `send_packet(dest, data, size, now)`. It rolls dice against the
configured loss/duplicate rates and, for survivors, schedules delivery at
`now + latency ± jitter` in a priority queue. Each frame you call `update(now)`,
which flushes any packets whose delivery time has arrived to the real socket.

## Try it

```
make test         # runs sim_test among others
./bin/sim_test    # loopback send/receive + latency + statistical loss
```

## What to notice

- `receive()` returning 0 is normal, not an error — it just means "nothing yet".
- The simulator only delays/drops **outgoing** packets; the receiving side just
  reads its socket as usual.
