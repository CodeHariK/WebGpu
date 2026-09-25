# 02 — Virtual connection & reliability

**Goal:** turn raw UDP into a "connection" that knows if the other side is still
there, measures ping, and can tell which packets were received.

## The idea

UDP has no connection, so we invent one. Every packet gets a small **12-byte
header**:

```
protocol_id | sequence | ack | ack_bits
```

- `sequence` — our own increasing packet number.
- `ack` — the latest sequence number we've received from them.
- `ack_bits` — a 32-bit field: bit *n* set means "I also got packet `ack - n`".

This is Glenn Fiedler's classic scheme. With one `ack` + 32 `ack_bits`, a single
packet acknowledges up to 33 of the peer's recent packets, so acknowledgements
survive packet loss well. From acks we derive **round-trip time** (a smoothed
average) and an estimated **packet-loss %**.

On top of that sits a small **state machine**:
`Disconnected → Connecting → Connected → Disconnecting`, plus keep-alive
heartbeats and an inactivity timeout so a silent peer is eventually dropped.

## Files to read

- `src/net/PacketHeader.hpp` — the 12-byte header, with big-/little-endian-safe
  serialization and 16-bit sequence wraparound handling.
- `src/net/ReliabilitySystem.hpp/.cpp` — the ack bitfield, RTT (EMA), and loss
  estimation.
- `src/net/Connection.hpp/.cpp` — ties it together: `connect`, `accept`,
  `process_packet`, `update` (heartbeats + timeouts). Later steps add channels.

## Try it

```
./bin/protocol_test   # sequence wraparound, header (de)serialization, ack + RTT
```

## What to notice

- Sequence numbers are 16-bit and **wrap around**; comparing them correctly
  (is A newer than B across the wrap?) is a classic netcode gotcha — see the
  `sequence_greater_than` logic.
- `Connection::update()` must be called every frame; it flushes pending data,
  sends heartbeats, and detects timeouts.
