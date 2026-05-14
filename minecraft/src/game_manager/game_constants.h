#ifndef GAME_CONSTANTS_H
#define GAME_CONSTANTS_H

#include "godot_cpp/classes/character_body3d.hpp"
#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/rigid_body3d.hpp"
#include <cstdint>

namespace godot {

/**
 * @brief Universal Collision Layers for the project.
 * Map to Godot's 32-bit collision layer system.
 * Usage: layer = LAYER_TERRAIN | LAYER_OBJECTS;
 */
enum CollisionLayer : uint8_t {
	LAYER_TERRAIN = 1, //
	LAYER_PLAYER = 2, //
	LAYER_ENEMY = 3, //
	LAYER_OBJECTS = 4, // Layer 4 (Stations, Ingredients, Interactables)
	LAYER_CORNERS = 5, // Layer 6 (MC Grid Debug)
	LAYER_CELLS = 6, // Layer 7 (MC Grid Dual)
	LAYER_MOVING_OBJECTS = 7
};

inline uint32_t toLayer(CollisionLayer layer) {
	return 1 << (static_cast<uint32_t>(layer) - 1);
}

/**
 * @brief Interaction Constants
 */
namespace InteractionDefaults {
const float RANGE = 4.0f;
inline const uint32_t MASK = toLayer(LAYER_OBJECTS) | toLayer(LAYER_TERRAIN); // We might want to "hit" terrain to drop things
} // namespace InteractionDefaults

inline void applyCollisionLayerMaskPlayer(CharacterBody3D *p_node) {
	p_node->set_collision_layer(toLayer(LAYER_PLAYER));
	p_node->set_collision_mask(toLayer(LAYER_TERRAIN) | toLayer(LAYER_PLAYER) | toLayer(LAYER_ENEMY) | toLayer(LAYER_OBJECTS));
}

inline void applyCollisionLayerMaskMovingObjects(RigidBody3D *p_node) {
	p_node->set_collision_layer(toLayer(LAYER_MOVING_OBJECTS));
	p_node->set_collision_mask(toLayer(LAYER_TERRAIN) | toLayer(LAYER_PLAYER) | toLayer(LAYER_MOVING_OBJECTS));
}

} // namespace godot

#endif // GAME_CONSTANTS_H
