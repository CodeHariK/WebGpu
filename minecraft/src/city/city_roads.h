/**
 * @file city_roads.h
 * @brief The Map 26 city's roads: every road a flat strip just over the land, its width and colour
 * by kind; the motorway a raised deck that climbs onto its bridge over the strait, on piers, with
 * a collider (the other roads are driven on the land's collider).
 */
#ifndef CITY_ROADS_H
#define CITY_ROADS_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/array.hpp>

namespace godot::city {

// Adds the roads under p_parent from the export's roads.
void build_roads(
		Node3D *p_parent,
		const Array &p_roads
);

} // namespace godot::city

#endif // CITY_ROADS_H
