# 06 — Bit packing & delta compression

**Goal:** make snapshots small, because bandwidth is the real budget in
networked games.

## The idea

Two techniques stack:

1. **Bit packing.** Instead of spending a full 32-bit float or int on every
   value, spend only the bits a value actually needs. A boolean is 1 bit. A
   number known to be in `[0, 1000]` is 10 bits. A world position is *quantized*
   to a fixed grid and packed in 16 bits. `BitWriter`/`BitReader` handle the
   bit-level plumbing.

2. **Delta compression.** Most of the world doesn't change between snapshots. So
   the server sends each entity as a **delta against a baseline** the client has
   already acknowledged: one "changed?" bit per field, and the value only if it
   changed. An entity's color (24 bits) almost never changes, so it collapses to
   a single bit. Entities that didn't move cost almost nothing.

The baseline is chosen by acknowledgement (Quake3 style): the client tells the
server the newest snapshot tick it decoded (`ack_server_tick`), and the server
deltas against exactly that. If the baseline has aged out, it sends an absolute
"keyframe" (baseline tick 0) that any client can decode.

## Files to read

- `src/net/BitStream.hpp` — `BitWriter`/`BitReader`: `write_bits`, `write_bool`,
  `write_ranged`, `write_float` (quantized), and safe over-read detection.
- `src/game/DeltaSnapshot.hpp` — `encode_delta_snapshot` / `decode_delta_snapshot`
  and the field quantization budgets.
- `src/apps/server.cpp` / `client.cpp` — how baselines are stored and acked.

## Try it

```
./bin/bitstream_test   # bit round-trips, ranged ints, quantized floats
./bin/delta_test       # delta round-trip; prints the compression ratio
```

## What to notice

- Quantization is **lossy** by design: positions snap to a fine grid. The grid is
  far finer than the reconciliation threshold, so it never causes false
  corrections.
- `delta_test` shows ~85% smaller snapshots on a busy scene — that's the whole
  point.
