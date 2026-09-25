# Game Netcode from Scratch (C++20)

A high-performance game networking and state synchronization stack built from first principles, following Glenn Fiedler's architecture, ENet, and Valve's GameNetworkingSockets patterns.

---

## Project layout

```
src/net/     reusable networking library (sockets, connection, channels,
             reliability, bitstream, network simulator)
src/game/    game logic, header-only (types, simulation, prediction, delta,
             lag compensation, interest management)
src/apps/    demo programs: server.cpp, client.cpp
tests/       one test program per concept
docs/        step-by-step learning guides (start with docs/00-overview.md)
```

**New here / learning?** Read [`docs/00-overview.md`](docs/00-overview.md) — it
walks the concepts in order and maps each one to the files. See also
[`docs/CODE_REVIEW.md`](docs/CODE_REVIEW.md).

## Build & run

```
make          # build apps + tests
make test     # build and run every test
make run      # server + client (clean network)
make run-sim  # server + client under a simulated bad network
make format   # apply .clang-format to all sources
```

Code style is defined in [`.clang-format`](.clang-format) (4-space indent,
one parameter per line when signatures wrap).

## Milestone 1: Sockets & Network Impairment Simulator
- Non-blocking UDP sockets via `fcntl(O_NONBLOCK)`
- Custom network condition simulator (latency, jitter, packet loss, duplicates)
- High-resolution monotonic timer

## Milestone 2: Virtual Connection & Sliding ACK Bitfield
- **PacketHeader**: 12-byte wire header (`protocol_id`, `sequence`, `ack`, `ack_bits`) with network byte order serialization and 16-bit wraparound handling.
- **ReliabilitySystem**:
  - Glenn Fiedler’s 32-packet sliding ACK bitfield algorithm.
  - Round-Trip Time (RTT) estimation via Exponential Moving Average (EMA).
  - Statistical packet loss calculation based on unacknowledged aged packets.
- **Connection**:
  - Virtual session state machine (`Disconnected` -> `Connecting` -> `Connected` -> `Disconnecting`).
  - Automatic keep-alive heartbeats and inactivity timeout detection.

## Milestone 3: Channel Multiplexing
- **UnreliableUnordered** (`Channel 0`): Fire-and-forget for transient data (audio/effects).
- **UnreliableSequenced** (`Channel 1`): For state snapshots. Older out-of-order packets are dropped natively at the channel layer.
- **ReliableOrdered** (`Channel 2`): For chat, RPCs, and entity creation. Implements an active unacknowledged sliding window, RTT-based retransmission timers, and Head-of-Line isolation logic buffering out-of-order messages sequentially.
- **Multiplexer**: `Connection::flush_channels()` dynamically distributes the remaining MTU budget across available channel queues.

## Milestone 4: Snapshot Interpolation (Remote Entities)
- Authoritative 60Hz physics loop (`EntityState`: velocity integration, damping).
- Server broadcasts `WorldSnapshot` at 20Hz over the UnreliableSequenced channel.
- Clients maintain a jitter/history `SnapshotBuffer` delaying remote entity rendering ($T_{render} = T_{now} - 100\text{ms}$) to interpolate smoothly across packet arrival jitter.

## Milestone 5: Client-Side Prediction & Server Reconciliation
- **Deterministic Shared Simulation ([`Simulation.hpp`](src/game/Simulation.hpp))**: Bit-identical physics run across client and server.
- **Client Prediction ([`InputHistory.hpp`](src/game/InputHistory.hpp))**:
  - Local client applies inputs immediately at 60Hz with zero latency.
  - Records inputs and resulting predicted states in a circular history queue.
  - Redundantly batches recent inputs (up to 5) into each datagram to survive packet drops.
- **Server Authoritative State & Acks**:
  - Server processes inputs, stamps each snapshot with `last_client_input_tick`.
- **Reconciliation / Rollback**:
  - When the client receives a server snapshot, it discards acknowledged inputs.
  - If server state diverges from predicted state at that tick, the client snaps back to the server's state and replays all pending unacknowledged inputs forward to the current tick.

## Milestone 6: Bitstream Packing & Delta Compression
- **BitStream ([`BitStream.hpp`](src/net/BitStream.hpp))**:
  - `BitWriter` / `BitReader` pack values at arbitrary bit widths (LSB-first) into a byte buffer.
  - Ranged integers cost only `bits_required(range)` bits; booleans cost 1 bit.
  - Floats are quantized to a fixed bit budget over a `[min, max]` range.
  - Over-reads set an `overflowed()` flag so truncated datagrams fail safely.
- **Delta Snapshots ([`DeltaSnapshot.hpp`](src/game/DeltaSnapshot.hpp))**:
  - Each entity field carries a 1-bit "changed" flag; only changed, quantized fields are written.
  - The near-constant 24-bit color collapses to a single bit when unchanged.
  - Encodes against a *baseline* snapshot the client has acknowledged (Quake3 / Source style); `baseline_tick == 0` is an absolute keyframe any client can decode.
- **Acknowledged Baselines (server & client)**:
  - The client stamps each input batch with `ack_server_tick` (the newest snapshot it fully decoded).
  - The server keeps a per-tick snapshot history and delta-encodes against each client's acknowledged baseline, falling back to a keyframe when that baseline has aged out.
  - The client keeps its own decoded-snapshot history and reconstructs each delta against the referenced baseline, dropping snapshots whose baseline it never received.
  - Result: ~85% smaller snapshots for many-entity scenes; the 32-entity unit test packs 780 raw bytes into 114.

## Milestone 7: Lag Compensation (Server-Side Hit Rewind)
- **Rewind buffer ([`LagCompensation.hpp`](src/game/LagCompensation.hpp))**:
  - `WorldHistory` records one time-stamped world frame per server tick (~2s ring).
  - Rewind by exact tick (`get_at_tick`) or by interpolated wall-clock time (`sample`).
- **Hitscan**:
  - `ray_circle_intersect` / `hitscan` resolve the nearest circular hitbox struck by a ray, skipping the shooter.
- **"What you see is what you get"**:
  - The client fires at where it *sees* the target (a position it viewed in the past) and stamps the shot with `view_server_tick`.
  - The server rewinds every entity to that tick before the hit test, so shots that look like hits on the client are hits on the server despite latency.
  - A `FireCommand` (reliable channel) is answered with an authoritative `HitNotification`.
- **Demo**: the server spawns a fast sine-sweeping target bot; the client auto-fires at it and reports its hit rate, which stays high only because of the rewind.

## Milestone 8: Interest Management (Area of Interest)
- **AOI culling ([`InterestManagement.hpp`](src/game/InterestManagement.hpp))**:
  - `compute_visible_set` returns, per client, the viewer plus every entity within an interest radius; an optional `max_count` caps it to the nearest N (a hard per-snapshot ceiling).
  - Snapshot bandwidth then scales with what a player can *see*, not with total world size — the natural way to scale toward many entities/players.
- **Per-client delta baselines (server)**:
  - Each client's delta baseline is now the exact AOI subset it last acknowledged (stored per session), so entities entering range encode as new and entities leaving range simply drop out.
  - The full-world rewind buffer for lag compensation is kept separately, so hit detection is never affected by what a client happened to be sent.
- **Demo**: the server spawns several bots sweeping different lanes; each client receives only nearby ones (telemetry reports avg visible/client below the world total), while lag-compensated shots still land on visible targets.

## Milestone 9: Network Simulator in the Live Loop
- **Optional impairment on the wire ([`NetworkSimulator`](src/net/NetworkSimulator.hpp) + `Connection`)**:
  - `Connection::set_network_simulator` routes all outgoing packets through the simulator (latency, jitter, packet loss, duplication) instead of straight to the socket; the app flushes due packets with `update()` each frame. With no simulator attached, behavior is unchanged.
  - Both the server (server->client) and the client (client->server) can be impaired independently.
- **Env-driven config ([`SimConfig.hpp`](src/game/SimConfig.hpp))**: `NETSIM_LATENCY_MS`, `NETSIM_JITTER_MS`, `NETSIM_LOSS`, `NETSIM_DUP` — no CLI changes needed.
- **Why it matters**: under real latency/jitter/loss the earlier milestones finally earn their keep — prediction/reconciliation hide the delay, delta compression keeps snapshots small, and lag compensation still lands hits. `make run-sim` launches both sides under adverse conditions; watch ping and reconciliations climb while hits keep resolving.

---

## Roadmap

- [x] **Milestone 1**: Sockets & Artificial Impairment Simulator
- [x] **Milestone 2**: Virtual Connection & Sliding Ack Bitfield
- [x] **Milestone 3**: Channel Multiplexing
- [x] **Milestone 4**: Authoritative Server, Snapshot History Buffer & Interpolation
- [x] **Milestone 5**: Client-Side Prediction, Input History Buffer & Reconciliation
- [x] **Milestone 6**: Bitstream Packing, Quantization & Delta Compression
- [x] **Milestone 7**: Lag Compensation (server-side hit rewind)
- [x] **Milestone 8**: Interest Management (Area-of-Interest culling)
- [x] **Milestone 9**: Network Simulator in the live loop (latency/jitter/loss/duplication)
