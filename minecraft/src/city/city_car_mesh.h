/**
 * @file city_car_mesh.h
 * @brief The traffic cars' one mesh: a toy car of boxes standing on y = 0, facing −z. Surface 0 is
 * the body, white, to be tinted by each MultiMesh instance's colour; surface 1 the windows; surface
 * 2 the wheels.
 */
#ifndef CITY_CAR_MESH_H
#define CITY_CAR_MESH_H

#include <godot_cpp/classes/array_mesh.hpp>

namespace godot::city {

// A car p_size big (width, height, length, metres), its three surfaces with their materials.
Ref<ArrayMesh> make_car_mesh(const Vector3 &p_size);

} // namespace godot::city

#endif // CITY_CAR_MESH_H
