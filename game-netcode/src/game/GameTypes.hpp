#pragma once

#include <cstdint>

namespace game {

/**
 * Common identifiers for our core channel layout.
 * Best practice is to statically assign channel slots.
 */
enum ReliableMsgType : uint8_t {
    MSG_FIRE = 1,  // Client -> Server FireCommand
    MSG_HIT = 2,   // Server -> Client HitNotification
};

enum Channels : uint8_t {
    CH_UNRELIABLE = 0,  // Unordered (e.g. effects, audio)
    CH_STATE = 1,       // Sequenced (e.g. Server->Client world snapshots, Client->Server input)
    CH_RELIABLE = 2,    // Ordered (e.g. chat, spawn/despawn events, RPCs)
};

/**
 * 2D Vector for basic physics / movement simulation
 */
struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }
    Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
    Vec2 operator*(float scalar) const { return {x * scalar, y * scalar}; }

    static Vec2 lerp(const Vec2& a, const Vec2& b, float t) { return a + (b - a) * t; }
};

/**
 * Represents a single entity (e.g. a player) in the world.
 */
#pragma pack(push, 1)
struct EntityState {
    uint32_t entity_id{0};
    Vec2 position{0.0f, 0.0f};
    Vec2 velocity{0.0f, 0.0f};
    uint32_t color_rgb{0xFFFFFF};
};

/**
 * Client -> Server: Player Input
 * Sent every client frame or tick.
 */
struct PlayerInput {
    uint32_t tick{0};
    float move_x{0.0f};
    float move_y{0.0f};
    uint32_t buttons{0};
};

/**
 * Header preceding an array of redundant PlayerInputs in a datagram.
 * ack_server_tick carries the newest server snapshot tick the client has fully
 * decoded, so the server knows which baseline to delta-compress against
 * (Milestone 6). 0 means the client has no baseline yet.
 */
struct InputBatchHeader {
    uint32_t input_count{0};
    uint32_t ack_server_tick{0};
};

/**
 * Server -> Client: World Snapshot Header
 * Precedes an array of EntityState structs payload.
 * Includes last_client_input_tick so the client knows which input was acknowledged
 * by the server for prediction reconciliation.
 */
struct SnapshotHeader {
    uint32_t server_tick{0};
    uint32_t last_client_input_tick{0};
    uint32_t entity_count{0};
};

/**
 * Milestone 7 (Lag Compensation) wire messages, sent on the ReliableOrdered
 * channel. Reliable payloads are prefixed with a 1-byte ReliableMsgType tag so a
 * single channel can carry more than one message kind.
 *
 * Client -> Server: the player fired a hitscan shot. origin is the shooter's
 * authoritative position at fire time; aim is the (un-normalized) direction the
 * player was aiming. view_server_tick is the snapshot tick the client was looking
 * at, so the server can rewind the world to exactly what the shooter saw.
 */
struct FireCommand {
    uint32_t client_tick{0};
    uint32_t view_server_tick{0};
    float origin_x{0.0f};
    float origin_y{0.0f};
    float aim_x{0.0f};
    float aim_y{0.0f};
};

/**
 * Server -> Client: authoritative result of a FireCommand after rewinding the
 * world. hit is 1 when a target was struck; target_id / point describe it.
 */
struct HitNotification {
    uint32_t client_tick{0};
    uint8_t hit{0};
    uint32_t target_id{0};
    float point_x{0.0f};
    float point_y{0.0f};
};
#pragma pack(pop)

}  // namespace game
