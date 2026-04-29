#ifndef GAME_CONSTANTS_H
#define GAME_CONSTANTS_H

#include <cstdint>

namespace godot {

/**
 * @brief Universal Collision Layers for the project.
 * Map to Godot's 32-bit collision layer system.
 * Usage: layer = LAYER_TERRAIN | LAYER_OBJECTS;
 */
enum CollisionLayer : uint32_t {
	LAYER_NONE = 0,
	LAYER_TERRAIN = 1 << 0,  // Layer 1
	LAYER_PLAYER = 1 << 1,   // Layer 2
	LAYER_ENEMY = 1 << 2,    // Layer 3
	LAYER_OBJECTS = 1 << 3,  // Layer 4 (Stations, Ingredients, Interactables)
	LAYER_CORNERS = 1 << 5,  // Layer 6 (MC Grid Debug)
	LAYER_CELLS = 1 << 6     // Layer 7 (MC Grid Dual)
};

/**
 * @brief Interaction Constants
 */
namespace InteractionDefaults {
const float RANGE = 4.0f;
const uint32_t MASK = LAYER_OBJECTS | LAYER_TERRAIN; // We might want to "hit" terrain to drop things
} // namespace InteractionDefaults

} // namespace godot

#endif // GAME_CONSTANTS_H
