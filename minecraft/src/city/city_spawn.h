/**
 * @file city_spawn.h
 * @brief Where to start in a Map 26 city: the longest piece of a big road (arterial first, then
 * highway, ring, main), as the transform that puts its middle at the origin, running along +x.
 */
#ifndef CITY_SPAWN_H
#define CITY_SPAWN_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

namespace godot::city {

// The transform for the city so its start road's middle is at the origin along +x (identity if
// there is no big road).
Transform3D start_on_road(const Array &p_roads);

} // namespace godot::city

#endif // CITY_SPAWN_H
