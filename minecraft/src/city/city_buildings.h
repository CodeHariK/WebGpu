/**
 * @file city_buildings.h
 * @brief The Map 26 city's buildings and trees: every box building one instance of a unit box (all
 * of them one draw call, each its own colour), houses with a pyramid roof, round towers cylinders,
 * trees cones; each with a collider so cars hit them.
 */
#ifndef CITY_BUILDINGS_H
#define CITY_BUILDINGS_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/array.hpp>

namespace godot::city {

// Adds the buildings and trees under p_parent from the export's buildings and trees.
void build_buildings(
		Node3D *p_parent,
		const Array &p_buildings,
		const Array &p_trees
);

} // namespace godot::city

#endif // CITY_BUILDINGS_H
