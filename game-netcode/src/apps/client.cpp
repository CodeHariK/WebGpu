#include "core/Timer.hpp"
#include "game/DeltaSnapshot.hpp"
#include "game/GameTypes.hpp"
#include "game/InputHistory.hpp"
#include "game/SimConfig.hpp"
#include "game/Simulation.hpp"
#include "net/Connection.hpp"
#include "net/NetworkSimulator.hpp"
#include "net/ReliableOrderedChannel.hpp"
#include "net/Socket.hpp"
#include "net/UnreliableSequencedChannel.hpp"
#include "net/UnreliableUnorderedChannel.hpp"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
//  Game client.
//
//  main() owns the socket, the connection, and the loop; everything else lives
//  in ClientState plus a handful of named helpers (decode a snapshot, reconcile,
//  predict + send input, fire, print telemetry). Read the helpers, then main().
// ============================================================================

namespace {

std::atomic<bool> g_running{true};
void handle_signal(int) {
    g_running = false;
}

// --- Tuning constants --------------------------------------------------------
constexpr double TICK_RATE = 60.0;
constexpr double TICK_DT = 1.0 / TICK_RATE;
constexpr float SIM_DT = static_cast<float>(TICK_DT);
constexpr size_t REDUNDANT_INPUTS = 5;        // recent inputs resent each packet
constexpr size_t MAX_SNAPSHOT_HISTORY = 128;  // decoded snapshots kept for deltas
constexpr double FIRE_INTERVAL = 0.4;         // seconds between hitscan shots

// --- All mutable client-side state, in one place -----------------------------
struct ClientState {
    // World view / identity
    uint32_t local_entity_id{0};
    game::EntityState local_player{0, {0.0f, 0.0f}, {0.0f, 0.0f}, 0x00FF00};
    game::InputHistory input_history;
    std::map<uint32_t, std::vector<game::EntityState>> snapshot_history;
    uint32_t last_decoded_server_tick{0};

    // Prediction / reconciliation stats
    uint32_t client_tick{0};
    uint32_t total_reconciliations{0};
    uint64_t total_replayed_ticks{0};

    // Shooting demo
    bool target_known{false};
    game::Vec2 target_seen_pos{0.0f, 0.0f};  // where we currently SEE the target
    size_t visible_count{0};
    uint32_t shots_fired{0};
    uint32_t shots_hit{0};
};

// --- Free helpers ------------------------------------------------------------

// Deterministic, time-varying movement so the demo player traces a circle.
game::PlayerInput make_input(uint32_t tick) {
    const float t = static_cast<float>(tick) * SIM_DT;
    game::PlayerInput input{};
    input.tick = tick;
    input.move_x = std::cos(t);
    input.move_y = std::sin(t);
    input.buttons = 0;
    return input;
}

// Packs recent inputs (redundant against loss) plus the acked snapshot tick.
std::vector<uint8_t> pack_input_batch(const std::vector<game::PlayerInput>& inputs,
                                      uint32_t ack_server_tick) {
    game::InputBatchHeader header{};
    header.input_count = static_cast<uint32_t>(inputs.size());
    header.ack_server_tick = ack_server_tick;

    std::vector<uint8_t> payload(sizeof(header) + inputs.size() * sizeof(game::PlayerInput));
    std::memcpy(payload.data(), &header, sizeof(header));
    if (!inputs.empty()) {
        std::memcpy(payload.data() + sizeof(header),
                    inputs.data(),
                    inputs.size() * sizeof(game::PlayerInput));
    }
    return payload;
}

// Finds this client's own entity: the lowest id on first sight, then tracked.
const game::EntityState* find_local_entity(const std::vector<game::EntityState>& entities,
                                           uint32_t& local_entity_id) {
    const game::EntityState* found = nullptr;
    for (const auto& e : entities) {
        if (local_entity_id == 0) {
            if (!found || e.entity_id < found->entity_id) found = &e;
        } else if (e.entity_id == local_entity_id) {
            return &e;
        }
    }
    if (found) local_entity_id = found->entity_id;
    return found;
}

// Decodes one delta snapshot against our stored baseline, updates history and the
// acked tick, re-targets the nearest visible entity, and reconciles the local
// player against the server's authoritative state.
void handle_snapshot(ClientState& cs, const netcode::Message& msg) {
    // Resolve the baseline the server delta-encoded against (0 = absolute).
    const std::vector<game::EntityState>* baseline = nullptr;
    if (msg.payload.size() >= 12) {
        uint32_t baseline_tick = 0;
        std::memcpy(&baseline_tick, msg.payload.data() + 8, sizeof(baseline_tick));
        if (baseline_tick != 0) {
            auto hit = cs.snapshot_history.find(baseline_tick);
            if (hit == cs.snapshot_history.end()) return;  // baseline we never decoded
            baseline = &hit->second;
        }
    }

    static const std::vector<game::EntityState> kEmptyBaseline;
    game::SnapshotWireHeader hdr{};
    std::vector<game::EntityState> entities;
    if (!game::decode_delta_snapshot(msg.payload.data(),
                                     msg.payload.size(),
                                     baseline ? *baseline : kEmptyBaseline,
                                     hdr,
                                     entities)) {
        return;
    }

    cs.snapshot_history[hdr.server_tick] = entities;
    while (cs.snapshot_history.size() > MAX_SNAPSHOT_HISTORY) {
        cs.snapshot_history.erase(cs.snapshot_history.begin());
    }
    if (hdr.server_tick > cs.last_decoded_server_tick) {
        cs.last_decoded_server_tick = hdr.server_tick;
    }

    const game::EntityState* server_player = find_local_entity(entities, cs.local_entity_id);
    cs.visible_count = entities.size();

    // Target the nearest visible entity that isn't us (re-evaluated per snapshot
    // because AOI makes targets appear and vanish).
    cs.target_known = false;
    if (server_player) {
        float best_d2 = 0.0f;
        for (const auto& e : entities) {
            if (e.entity_id == cs.local_entity_id) continue;
            const float dx = e.position.x - server_player->position.x;
            const float dy = e.position.y - server_player->position.y;
            const float d2 = dx * dx + dy * dy;
            if (!cs.target_known || d2 < best_d2) {
                cs.target_known = true;
                best_d2 = d2;
                cs.target_seen_pos = e.position;
            }
        }
    }

    if (!server_player) return;

    // Reconcile: on divergence, snap to authoritative state and replay pending inputs.
    const uint32_t ack_tick = hdr.last_client_input_tick;
    if (cs.input_history.has_diverged(ack_tick, *server_player)) {
        game::EntityState reconciled;
        const size_t replayed =
            cs.input_history.reconcile(ack_tick, *server_player, SIM_DT, reconciled);
        cs.local_player = reconciled;
        ++cs.total_reconciliations;
        cs.total_replayed_ticks += replayed;
    } else {
        cs.input_history.discard_acknowledged(ack_tick);
    }
}

// Reads an authoritative hit result (reliable channel) and tallies it.
void handle_hit(ClientState& cs, const netcode::Message& msg) {
    if (msg.payload.size() != 1 + sizeof(game::HitNotification) ||
        msg.payload[0] != game::MSG_HIT) {
        return;
    }
    game::HitNotification note{};
    std::memcpy(&note, msg.payload.data() + 1, sizeof(note));
    if (note.hit) {
        ++cs.shots_hit;
        std::cout << "[Client] Shot (tick " << note.client_tick << ") HIT entity " << note.target_id
                  << " at (" << std::fixed << std::setprecision(2) << note.point_x << ", "
                  << note.point_y << ")\n";
    } else {
        std::cout << "[Client] Shot (tick " << note.client_tick << ") MISS\n";
    }
}

// One simulation tick: predict locally (zero latency), record for replay, and
// send the recent inputs to the server.
void predict_and_send(ClientState& cs, netcode::Connection& conn, double now) {
    const game::PlayerInput input = make_input(cs.client_tick);
    game::simulate_player(cs.local_player, input, SIM_DT);
    cs.input_history.record_input(input, cs.local_player);

    const std::vector<game::PlayerInput> batch =
        cs.input_history.get_recent_inputs(REDUNDANT_INPUTS);
    const std::vector<uint8_t> payload = pack_input_batch(batch, cs.last_decoded_server_tick);
    conn.send_message(game::CH_STATE, payload.data(), payload.size(), now);
}

// Periodically fires a hitscan shot at where we currently SEE the target. The
// server rewinds to view_server_tick to make the hit fair despite latency.
void maybe_fire(ClientState& cs, netcode::Connection& conn, double now, double& last_fire_time) {
    if (!cs.target_known || !conn.is_connected() || now - last_fire_time < FIRE_INTERVAL) return;
    last_fire_time = now;

    game::FireCommand fire{};
    fire.client_tick = cs.client_tick;
    fire.view_server_tick = cs.last_decoded_server_tick;
    fire.origin_x = cs.local_player.position.x;
    fire.origin_y = cs.local_player.position.y;
    fire.aim_x = cs.target_seen_pos.x - cs.local_player.position.x;
    fire.aim_y = cs.target_seen_pos.y - cs.local_player.position.y;

    std::vector<uint8_t> shot(1 + sizeof(fire));
    shot[0] = game::MSG_FIRE;
    std::memcpy(shot.data() + 1, &fire, sizeof(fire));
    conn.send_message(game::CH_RELIABLE, shot.data(), shot.size(), now);
    ++cs.shots_fired;
}

void print_telemetry(const ClientState& cs, const netcode::Connection& conn) {
    const uint32_t pct = cs.shots_fired ? (cs.shots_hit * 100 / cs.shots_fired) : 0;
    std::cout << "[Client] Tick: " << cs.client_tick
              << " | Ping: " << static_cast<int>(conn.rtt_ms()) << "ms | Pos: (" << std::fixed
              << std::setprecision(2) << cs.local_player.position.x << ", "
              << cs.local_player.position.y << ") | Reconciliations: " << cs.total_reconciliations
              << " (ReplayedTicks: " << cs.total_replayed_ticks << ") | Shots: " << cs.shots_fired
              << " Hits: " << cs.shots_hit << " (" << pct << "%) | Visible: " << cs.visible_count
              << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    uint16_t port = 40000;
    std::string ip = "127.0.0.1";
    if (argc > 1) port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (argc > 2) ip = argv[2];

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    netcode::Socket socket;
    if (!socket.open(0)) {  // ephemeral local port
        std::cerr << "[Client] Failed to open socket.\n";
        return 1;
    }

    netcode::Address server_addr(ip, port);
    netcode::Connection conn(socket, 5.0, 0.25);
    conn.create_channel<netcode::UnreliableUnorderedChannel>(game::CH_UNRELIABLE);
    conn.create_channel<netcode::UnreliableSequencedChannel>(game::CH_STATE);
    conn.create_channel<netcode::ReliableOrderedChannel>(game::CH_RELIABLE);

    const netcode::NetworkSimulatorConfig sim_cfg = game::read_sim_config_from_env();
    std::unique_ptr<netcode::NetworkSimulator> sim;
    if (game::sim_enabled(sim_cfg)) {
        sim = std::make_unique<netcode::NetworkSimulator>(socket, sim_cfg);
        conn.set_network_simulator(sim.get());
    }

    std::cout << "========================================================\n";
    std::cout << "  Game Netcode Client (Milestone 9)\n";
    std::cout << "  Connecting to " << server_addr.to_string() << " | Target Tickrate: 60Hz\n";
    std::cout << "  Features: Prediction, Delta Compression, Lag Comp, AOI & NetSim\n";
    std::cout << "========================================================\n";
    if (sim) {
        std::cout << "[Client] Network simulation ON (client->server): " << sim_cfg.latency_ms
                  << "ms +/-" << sim_cfg.jitter_ms << "ms jitter, loss "
                  << (sim_cfg.packet_loss_rate * 100.0f) << "%, dup "
                  << (sim_cfg.duplicate_rate * 100.0f) << "%\n";
    }

    double current_time = netcode::Timer::now_seconds();
    conn.connect(server_addr, current_time);

    ClientState cs;
    double accumulator = 0.0;
    double last_stat_print = current_time;
    double last_fire_time = current_time;
    uint8_t buffer[2048];

    while (g_running) {
        const double new_time = netcode::Timer::now_seconds();
        accumulator += (new_time - current_time);
        current_time = new_time;

        // 1. Drain the network: decode snapshots, read hit results.
        netcode::Address sender;
        int bytes = 0;
        while ((bytes = socket.receive(sender, buffer, sizeof(buffer))) > 0) {
            const uint8_t* raw_payload = nullptr;
            size_t raw_bytes = 0;
            if (!conn.process_packet(sender, buffer, bytes, raw_payload, raw_bytes, current_time)) {
                continue;
            }
            netcode::Message msg;
            while (conn.receive_message(msg)) {
                if (msg.channel_id == game::CH_STATE) {
                    handle_snapshot(cs, msg);
                } else if (msg.channel_id == game::CH_RELIABLE) {
                    handle_hit(cs, msg);
                }
            }
        }

        // 2. Fixed-timestep prediction + input transmission.
        while (accumulator >= TICK_DT) {
            ++cs.client_tick;
            accumulator -= TICK_DT;
            predict_and_send(cs, conn, current_time);
        }

        // 3. Shoot at the target we can see.
        maybe_fire(cs, conn, current_time, last_fire_time);

        // 4. Flush messages / simulator, detect disconnect.
        conn.update(current_time);
        if (sim) sim->update(current_time);
        if (conn.state() == netcode::ConnectionState::Disconnected) {
            std::cout << "[Client] Disconnected from server.\n";
            break;
        }

        // 5. Periodic telemetry.
        if (current_time - last_stat_print >= 1.0) {
            print_telemetry(cs, conn);
            last_stat_print = current_time;
        }

        netcode::Timer::sleep_ms(1.0);
    }

    conn.disconnect(current_time);
    socket.close();
    return 0;
}
