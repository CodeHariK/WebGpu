# 03 — Channels (delivery guarantees)

**Goal:** send different kinds of data with different guarantees over the same
connection.

## The idea

Not all data is equal:

- A **sound effect** can be lost — fire and forget. *(UnreliableUnordered)*
- A **position snapshot** should never apply an old one after a newer one, but a
  dropped one doesn't matter — just keep the newest. *(UnreliableSequenced)*
- A **chat message or "player joined"** must arrive, exactly once, in order.
  *(ReliableOrdered)*

So the connection has three **channels**, each with its own policy. Multiple
channel messages are **multiplexed** (packed) into a single UDP datagram, each
prefixed with a 5-byte message header (`channel_id`, `message_id`,
`payload_size`). `Connection::flush_channels()` fills the remaining space in the
packet from each channel's queue.

## Files to read

- `src/net/ChannelTypes.hpp` — the `Message` type and the per-message wire header.
- `src/net/Channel.hpp` — the abstract channel interface.
- `src/net/UnreliableUnorderedChannel.hpp` — no tracking; just send.
- `src/net/UnreliableSequencedChannel.hpp` — drops anything older than the newest
  seen.
- `src/net/ReliableOrderedChannel.hpp` — retransmits until acked, and buffers
  out-of-order arrivals so it can deliver them in order (head-of-line handling).
- `src/net/Connection.cpp` — `flush_channels()` does the multiplexing.

## Try it

```
./bin/channel_test    # verifies each channel's ordering/reliability behavior
```

## What to notice

- The game picks a channel per message by its `channel_id` (see
  `game::Channels` in `src/game/GameTypes.hpp`): `CH_UNRELIABLE`, `CH_STATE`,
  `CH_RELIABLE`.
- Reliability lives at the channel layer; the reliability *acks* from step 2 tell
  channels which packets got through.
