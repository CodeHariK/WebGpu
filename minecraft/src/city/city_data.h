/**
 * @file city_data.h
 * @brief Reading the Map 26 JSON export (2d_map Map26Export.ts): points, numbers, colours.
 *
 * The export is in metres: x to the right, z down the 2D map, y up, origin the map's middle;
 * angles in radians from x towards z (so a node's rotation.y is −angle). A line is [[x, z], ...].
 */
#ifndef CITY_DATA_H
#define CITY_DATA_H

#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot::city {

// A line's points as (x, z) pairs.
PackedVector2Array read_line(const Array &p_points);

// p_dict[p_key] as a float, or p_fallback if it is missing.
float read_float(
		const Dictionary &p_dict,
		const String &p_key,
		float p_fallback
);

// A box or cylinder standing on the ground at (x, z), turned by angle, scaled to p_size: the
// transform for a unit mesh centred on the origin (so lifted by half its height).
Transform3D standing(
		float p_x,
		float p_z,
		float p_angle,
		const Vector3 &p_size,
		float p_base
);

// One triangle into p_st (and its corners into r_faces, for a collider, if given), facing p_normal:
// its corners put in Godot's front-face order (clockwise seen from the side p_normal points to),
// whatever order they come in.
void add_face(
		const Ref<SurfaceTool> &p_st,
		const Vector3 &p_a,
		const Vector3 &p_b,
		const Vector3 &p_c,
		const Vector3 &p_normal,
		const Color &p_colour,
		PackedVector3Array *r_faces
);

// A material that takes its colour from the vertices (or MultiMesh instances), given in sRGB.
Ref<StandardMaterial3D> vertex_colour_material(float p_roughness);

// Twice the signed area of a loop (x, z): negative for an island's coast, positive for a lake's
// shore (the export keeps land on the left).
float signed_area(const PackedVector2Array &p_loop);

} // namespace godot::city

#endif // CITY_DATA_H
