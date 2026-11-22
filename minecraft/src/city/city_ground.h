/**
 * @file city_ground.h
 * @brief The Map 26 city's ground: the islands (flat land with a sandy edge dropping to the sea),
 * the lakes, the sea round them, and the colliders (the land, and a sea floor to stop a car that
 * drives off the edge).
 */
#ifndef CITY_GROUND_H
#define CITY_GROUND_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/array.hpp>

namespace godot::city {

constexpr float SEA_LEVEL = -0.6f; // metres: the sea's surface, below the land at 0
constexpr float SEA_FLOOR = -4.0f; // metres: where a car that leaves the land comes to rest
constexpr float EDGE_DEPTH = 2.0f; // metres the land's edge drops under the sea

// Adds the ground under p_parent from the export's coastlines; p_size: the map's width (metres).
void build_ground(
		Node3D *p_parent,
		const Array &p_coastlines,
		float p_size
);

} // namespace godot::city

#endif // CITY_GROUND_H
