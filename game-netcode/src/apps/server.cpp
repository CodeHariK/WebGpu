#include "core/Timer.hpp"
#include "game/DeltaSnapshot.hpp"
#include "game/GameTypes.hpp"
#include "game/InterestManagement.hpp"
#include "game/LagCompensation.hpp"
#include "game/SimConfig.hpp"
#include "game/Simulation.hpp"
#include "net/Connection.hpp"
#include "net/NetworkSimulator.hpp"
#include "net/ReliableOrderedChannel.hpp"
#include "net/Socket.hpp"
#include "net/UnreliableSequencedChannel.hpp"
#include "net/UnreliableUnorderedChannel.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

// ============================================================================
//  Authoritative game server.
//
//  main() is deliberately thin: it owns the loop and the world state, and hands
//  each concern to a small named helper below (accept, input, fire, bots,
//  snapshot build, broadcast). Read the helpers first, then main() reads like a
//  table of contents.
// ============================================================================

namespace {

std::atomic<bool> g_running{true};
void handle_signal(int) {
    g_running = false;
}

// --- Tuning constants --------------------------------------------------------
constexpr double TICK_RATE = 60.0;
constexpr double TICK_DT = 1.0 / TICK_RATE;
constexpr uint32_t SNAPSHOT_RATE = 3;         // broadcast every 3rd tick (20Hz)
constexpr size_t MAX_SNAPSHOT_HISTORY = 128;  // per-client delta baselines kept
constexpr size_t WORLD_HISTORY_FRAMES = 128;  // full-world rewind buffer depth

constexpr float HIT_RADIUS = 3.0f;
constexpr float AOI_RADIUS = 35.0f;      // clients only receive entities within range
constexpr size_t AOI_MAX_ENTITIES = 16;  // hard per-snapshot ceiling
constexpr double INTERP_DELAY = 0.100;   // client render delay (fallback rewind)

// Server-controlled moving target bots (lag-comp + AOI demo). Each sweeps a sine
// path in its own lane/phase.
constexpr uint32_t BOT_BASE_ID = 1000;
constexpr int NUM_BOTS = 6;
struct BotSpec {
    float amplitude;
    float frequency;
    float phase;
    float lane_y;
};
constexpr std::array<BotSpec, NUM_BOTS> kBots = {{
    {60.0f, 2.0f, 0.0f, 0.0f},
    {50.0f, 1.5f, 1.0f, 22.0f},
    {40.0f, 2.5f, 2.0f, -22.0f},
    {55.0f, 1.2f, 3.0f, 45.0f},
    {45.0f, 1.8f, 4.0f, -45.0f},
    {35.0f, 2.2f, 5.0f, 15.0f},
}};

// --- Per-client state --------------------------------------------------------
struct ClientSession {
    std::unique_ptr<netcode::Connection> connection;
    uint32_t entity_id{0};
    uint32_t last_processed_input_tick{0};
    uint32_t baseline_tick{0};  // newest snapshot tick this client acknowledged
    // Exactly what we sent this client at each broadcast tick (its AOI subset),
    // used as the delta baseline it reconstructs against.
    std::map<uint32_t, std::vector<game::EntityState>> sent_history;
    std::unique_ptr<netcode::NetworkSimulator> sim;  // optional outgoing impairment
};

using World = std::unordered_map<uint32_t, game::EntityState>;
using ClientMap = std::unordered_map<netcode::Address, ClientSession>;

struct BroadcastStats {
    uint64_t bytes{0};
    uint64_t count{0};
    uint64_t visible{0};
};

// --- Helpers -----------------------------------------------------------------

// Creates a fresh session for a newly-seen client: connection, channels, its
// world entity, and (optionally) an outgoing network simulator.
ClientSession make_client_session(netcode::Socket& socket,
                                  const netcode::Address& sender,
                                  World& world,
                                  uint32_t& next_entity_id,
                                  double current_time,
                                  const netcode::NetworkSimulatorConfig& sim_cfg,
                                  bool use_sim) {
    auto conn = std::make_unique<netcode::Connection>(socket, 5.0, 0.25);
    conn->create_channel<netcode::UnreliableUnorderedChannel>(game::CH_UNRELIABLE);
    conn->create_channel<netcode::UnreliableSequencedChannel>(game::CH_STATE);
    conn->create_channel<netcode::ReliableOrderedChannel>(game::CH_RELIABLE);
    conn->accept(sender, current_time);

    const uint32_t eid = next_entity_id++;
    world[eid] = game::EntityState{eid, {0.0f, 0.0f}, {0.0f, 0.0f}, 0x00FF00};

    ClientSession session;
    session.connection = std::move(conn);
    session.entity_id = eid;
    if (use_sim) {
        session.sim = std::make_unique<netcode::NetworkSimulator>(socket, sim_cfg);
        session.connection->set_network_simulator(session.sim.get());
    }
    return session;
}

// Applies a batch of client inputs (sequenced state channel) to the client's
// authoritative entity, and advances its acknowledged delta baseline.
void apply_input_batch(ClientSession& session, const netcode::Message& msg, World& world) {
    if (msg.payload.size() < sizeof(game::InputBatchHeader)) return;

    const auto* hdr = reinterpret_cast<const game::InputBatchHeader*>(msg.payload.data());
    const size_t expected =
        sizeof(game::InputBatchHeader) + hdr->input_count * sizeof(game::PlayerInput);
    if (msg.payload.size() != expected) return;

    if (hdr->ack_server_tick > session.baseline_tick) {
        session.baseline_tick = hdr->ack_server_tick;
    }

    const auto* inputs = reinterpret_cast<const game::PlayerInput*>(msg.payload.data() +
                                                                    sizeof(game::InputBatchHeader));

    auto& entity = world[session.entity_id];
    for (uint32_t i = 0; i < hdr->input_count; ++i) {
        const auto& in = inputs[i];
        if (in.tick > session.last_processed_input_tick) {
            game::simulate_player(entity, in, static_cast<float>(TICK_DT));
            session.last_processed_input_tick = in.tick;
        }
    }
}

// Handles a fire command (reliable channel) with lag compensation: rewind the
// world to what the shooter saw, hit-test, and reply with the result.
void handle_fire_command(ClientSession& session,
                         const netcode::Message& msg,
                         const game::WorldHistory& world_history,
                         uint32_t server_tick,
                         double current_time) {
    if (msg.payload.size() != 1 + sizeof(game::FireCommand) || msg.payload[0] != game::MSG_FIRE) {
        return;
    }

    game::FireCommand fire{};
    std::memcpy(&fire, msg.payload.data() + 1, sizeof(fire));

    // Prefer the exact tick the client was viewing; fall back to an RTT estimate.
    std::vector<game::EntityState> interp;
    const std::vector<game::EntityState>* rewound =
        world_history.get_at_tick(fire.view_server_tick);
    if (!rewound) {
        const float rtt_s = session.connection->rtt_ms() / 1000.0f;
        if (world_history.sample(current_time - (rtt_s * 0.5) - INTERP_DELAY, interp)) {
            rewound = &interp;
        }
    }

    game::HitResult result{};
    if (rewound) {
        result = game::hitscan({fire.origin_x, fire.origin_y},
                               {fire.aim_x, fire.aim_y},
                               *rewound,
                               session.entity_id,
                               HIT_RADIUS);
    }

    const uint32_t rewind_ticks =
        (server_tick > fire.view_server_tick) ? (server_tick - fire.view_server_tick) : 0;
    std::cout << "[Server] Fire (client tick " << fire.client_tick << ", rewound " << std::fixed
              << std::setprecision(0) << (rewind_ticks * TICK_DT * 1000.0)
              << "ms): " << (result.hit ? "HIT" : "MISS");
    if (result.hit) {
        std::cout << " entity " << result.target_id << " at (" << std::setprecision(2)
                  << result.point.x << ", " << result.point.y << ")";
    }
    std::cout << "\n";

    game::HitNotification note{};
    note.client_tick = fire.client_tick;
    note.hit = result.hit ? 1 : 0;
    note.target_id = result.target_id;
    note.point_x = result.point.x;
    note.point_y = result.point.y;

    std::vector<uint8_t> reply(1 + sizeof(note));
    reply[0] = game::MSG_HIT;
    std::memcpy(reply.data() + 1, &note, sizeof(note));
    session.connection->send_message(game::CH_RELIABLE, reply.data(), reply.size(), current_time);
}

// Moves the scripted target bots along their sine sweeps. Wire velocity stays
// zero (position is scripted, not integrated) to keep within quantization range.
void update_bots(World& world, float elapsed) {
    for (int i = 0; i < NUM_BOTS; ++i) {
        const uint32_t id = BOT_BASE_ID + static_cast<uint32_t>(i);
        world[id].position = {
            kBots[i].amplitude * std::sin(elapsed * kBots[i].frequency + kBots[i].phase),
            kBots[i].lane_y};
    }
}

// Flattens the authoritative world map into an ordered vector for this tick.
std::vector<game::EntityState> snapshot_world(const World& world) {
    std::vector<game::EntityState> out;
    out.reserve(world.size());
    for (const auto& [id, entity] : world) out.push_back(entity);
    return out;
}

// Sends each connected client an AOI-culled, delta-compressed snapshot, and
// records what it was sent as a future baseline. Accumulates bandwidth stats.
void broadcast_snapshots(ClientMap& clients,
                         const std::vector<game::EntityState>& current_snapshot,
                         uint32_t server_tick,
                         double current_time,
                         BroadcastStats& stats) {
    static const std::vector<game::EntityState> kEmptyBaseline;

    for (auto& [addr, session] : clients) {
        if (!session.connection->is_connected()) continue;

        std::vector<game::EntityState> visible = game::compute_visible_set(session.entity_id,
                                                                           current_snapshot,
                                                                           AOI_RADIUS,
                                                                           AOI_MAX_ENTITIES);

        const std::vector<game::EntityState>* baseline = nullptr;
        uint32_t baseline_tick = 0;
        if (session.baseline_tick != 0) {
            auto hit = session.sent_history.find(session.baseline_tick);
            if (hit != session.sent_history.end()) {
                baseline = &hit->second;
                baseline_tick = session.baseline_tick;
            }
        }

        game::SnapshotWireHeader hdr{};
        hdr.server_tick = server_tick;
        hdr.last_client_input_tick = session.last_processed_input_tick;
        hdr.baseline_tick = baseline_tick;

        std::vector<uint8_t> snap =
            game::encode_delta_snapshot(hdr, visible, baseline ? *baseline : kEmptyBaseline);

        session.connection->send_message(game::CH_STATE, snap.data(), snap.size(), current_time);

        stats.bytes += snap.size();
        stats.visible += visible.size();
        ++stats.count;

        session.sent_history[server_tick] = std::move(visible);
        while (session.sent_history.size() > MAX_SNAPSHOT_HISTORY) {
            session.sent_history.erase(session.sent_history.begin());
        }
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    uint16_t port = 40000;
    if (argc > 1) port = static_cast<uint16_t>(std::atoi(argv[1]));

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    netcode::Socket socket;
    if (!socket.open(port)) return 1;

    std::cout << "========================================================\n";
    std::cout << "  Game Netcode Auth Server (Milestone 9)\n";
    std::cout << "  Listening on port: " << port << " | Target Tickrate: 60Hz\n";
    std::cout << "  Features: Delta Compression, Lag Comp & Interest Mgmt\n";
    std::cout << "========================================================\n";

    const netcode::NetworkSimulatorConfig sim_cfg = game::read_sim_config_from_env();
    const bool use_sim = game::sim_enabled(sim_cfg);
    if (use_sim) {
        std::cout << "[Server] Network simulation ON (server->client): " << sim_cfg.latency_ms
                  << "ms +/-" << sim_cfg.jitter_ms << "ms jitter, loss "
                  << (sim_cfg.packet_loss_rate * 100.0f) << "%, dup "
                  << (sim_cfg.duplicate_rate * 100.0f) << "%\n";
    }

    ClientMap clients;
    World world_entities;
    uint32_t next_entity_id = 1;

    // Spawn the target bots.
    for (int i = 0; i < NUM_BOTS; ++i) {
        const uint32_t id = BOT_BASE_ID + static_cast<uint32_t>(i);
        world_entities[id] = game::EntityState{id, {0.0f, kBots[i].lane_y}, {0.0f, 0.0f}, 0xFF00FF};
    }

    game::WorldHistory world_history(WORLD_HISTORY_FRAMES);  // full-world rewind buffer

    uint32_t server_tick = 0;
    double current_time = netcode::Timer::now_seconds();
    const double start_time = current_time;
    double accumulator = 0.0;
    double last_stat_time = current_time;
    BroadcastStats stats;

    while (g_running) {
        const double new_time = netcode::Timer::now_seconds();
        accumulator += (new_time - current_time);
        current_time = new_time;

        // 1. Drain the network: accept clients, apply inputs, resolve shots.
        netcode::Address sender;
        uint8_t buffer[2048];
        int bytes = 0;
        while ((bytes = socket.receive(sender, buffer, sizeof(buffer))) > 0) {
            auto it = clients.find(sender);
            if (it == clients.end()) {
                std::cout << "[Server] Client connected: " << sender.to_string() << "\n";
                it = clients
                         .emplace(sender,
                                  make_client_session(socket,
                                                      sender,
                                                      world_entities,
                                                      next_entity_id,
                                                      current_time,
                                                      sim_cfg,
                                                      use_sim))
                         .first;
            }

            const uint8_t* raw_payload = nullptr;
            size_t raw_bytes = 0;
            if (!it->second.connection->process_packet(sender,
                                                       buffer,
                                                       bytes,
                                                       raw_payload,
                                                       raw_bytes,
                                                       current_time)) {
                continue;
            }

            netcode::Message msg;
            while (it->second.connection->receive_message(msg)) {
                if (msg.channel_id == game::CH_STATE) {
                    apply_input_batch(it->second, msg, world_entities);
                } else if (msg.channel_id == game::CH_RELIABLE) {
                    handle_fire_command(it->second, msg, world_history, server_tick, current_time);
                }
            }
        }

        // 2. Fixed-timestep simulation: move bots, record history, broadcast.
        while (accumulator >= TICK_DT) {
            ++server_tick;
            accumulator -= TICK_DT;

            update_bots(world_entities, static_cast<float>(current_time - start_time));

            std::vector<game::EntityState> current_snapshot = snapshot_world(world_entities);
            world_history.record(current_time, server_tick, current_snapshot);

            if (server_tick % SNAPSHOT_RATE == 0) {
                broadcast_snapshots(clients, current_snapshot, server_tick, current_time, stats);
            }
        }

        // 3. Update connections, flush simulators, drop timed-out clients.
        for (auto it = clients.begin(); it != clients.end();) {
            it->second.connection->update(current_time);
            if (it->second.sim) it->second.sim->update(current_time);
            if (it->second.connection->state() == netcode::ConnectionState::Disconnected) {
                std::cout << "[Server] Client timed out: " << it->first.to_string() << "\n";
                world_entities.erase(it->second.entity_id);
                it = clients.erase(it);
            } else {
                ++it;
            }
        }

        // 4. Periodic telemetry.
        if (current_time - last_stat_time >= 2.0) {
            const double avg_snap = stats.count ? (double(stats.bytes) / double(stats.count)) : 0.0;
            const double avg_vis =
                stats.count ? (double(stats.visible) / double(stats.count)) : 0.0;
            std::cout << "[Server] Tick: " << server_tick
                      << " | World entities: " << world_entities.size()
                      << " | Avg visible/client: " << std::fixed << std::setprecision(1) << avg_vis
                      << " | Avg snapshot: " << std::setprecision(1) << avg_snap << "B\n";
            stats = BroadcastStats{};
            last_stat_time = current_time;
        }

        netcode::Timer::sleep_ms(1.0);
    }

    socket.close();
    return 0;
}
