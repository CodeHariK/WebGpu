/**
 * @file city_props.h
 * @brief The Map 26 city's flat bits: car parks, dock yards and ponds just over the land, and the
 * piers out over the water (with colliders, so a car can drive onto them).
 */
#ifndef CITY_PROPS_H
#define CITY_PROPS_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot::city {

// Adds the car parks, dock yards, ponds and piers under p_parent from the export p_map.
void build_props(
		Node3D *p_parent,
		const Dictionary &p_map
);

} // namespace godot::city

#endif // CITY_PROPS_H
