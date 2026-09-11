/**
 * @file cliff_mesh.h
 * @brief CliffMesh: Procedural cliff rock with Voronoi-displaced walls, slope masking,
 * strata layering, and concave collision generation.
 */
#ifndef CLIFF_MESH_H
#define CLIFF_MESH_H

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {

/**
 * @class CliffMesh
 * @brief Generates a procedural cliff mesh by subdividing a base volumetric block and applying
 * 3D Voronoi / Worley noise displacement preferentially to its vertical walls.
 */
class CliffMesh : public ArrayMesh {
	GDCLASS(CliffMesh, ArrayMesh)

public:
	enum CliffStyle {
		STYLE_FACETED_BLOCKS = 0, // Broad planar rock slabs (Voronoi cell distance)
		STYLE_CREVASSED = 1,      // Deep fracture lines & fissures (F2 - F1)
		STYLE_COLUMNAR_BASALT = 2,// Vertical columns (stretched Voronoi in Y)
		STYLE_STRATIFIED = 3      // Layered horizontal sedimentary ledges
	};

private:
	int seed = 42;
	Vector3 extents = Vector3(10.0f, 12.0f, 6.0f); // Width (X), Height (Y), Depth (Z)
	int subdivisions_x = 24;
	int subdivisions_y = 32;
	int subdivisions_z = 16;
	float corner_radius = 0.35f; // Rounds horizontal wall corners (0 = sharp box, 1 = maximum cylinder/capsule rounding)
	float edge_radius = 0.2f;   // Rounds top/bottom horizontal edge transition into walls

	CliffStyle style = STYLE_FACETED_BLOCKS;
	float wall_noise_amp = 1.4f;       // Metres of displacement on vertical walls
	float wall_noise_freq = 0.35f;     // Frequency of Voronoi cells
	float warp_strength = 0.4f;        // Domain warp to break Voronoi grid regularity
	float top_noise_amp = 0.2f;        // Gentle bumpiness on the top walkable surface
	float slope_mask_steepness = 3.0f; // Higher = top stays flatter, walls get full noise
	bool flat_shaded = true;

	// Strata & Colors
	int strata_count = 8;
	float strata_depth = 0.3f;
	Color base_color = Color(0.38f, 0.35f, 0.32f);
	Color top_color = Color(0.55f, 0.53f, 0.48f);
	Color crevice_color = Color(0.18f, 0.16f, 0.15f);
	Color moss_color = Color(0.35f, 0.48f, 0.22f);
	float moss_amount = 0.6f; // Blends onto upward facing surfaces

	bool _rebuild_queued = false;
	PackedVector3Array _collision_faces;

	void _generate_subdivided_box(
			std::vector<Vector3> &r_verts,
			std::vector<Vector3> &r_normals,
			std::vector<Vector2> &r_uvs,
			std::vector<int> &r_indices
	) const;

	void _apply_voronoi_displacement(
			std::vector<Vector3> &r_verts,
			std::vector<Vector3> &r_normals,
			std::vector<Color> &r_colors
	) const;

	void _queue_rebuild();

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	CliffMesh();
	~CliffMesh();

	void rebuild();
	Ref<Shape3D> create_collision_shape() const;

	// clang-format off
#define CL_PROP(m_type, m_name, m_clamp)      \
	void set_##m_name(m_type p_value) {       \
		m_type v = m_clamp;                   \
		if (m_name != v) {                    \
			m_name = v;                       \
			_queue_rebuild();                 \
		}                                     \
	}                                         \
	m_type get_##m_name() const { return m_name; }

	CL_PROP(int, seed, p_value)
	CL_PROP(Vector3, extents, Vector3(MAX(0.5f, p_value.x), MAX(0.5f, p_value.y), MAX(0.5f, p_value.z)))
	CL_PROP(int, subdivisions_x, CLAMP(p_value, 2, 80))
	CL_PROP(int, subdivisions_y, CLAMP(p_value, 2, 80))
	CL_PROP(int, subdivisions_z, CLAMP(p_value, 2, 80))
	CL_PROP(float, corner_radius, CLAMP(p_value, 0.0f, 1.0f))
	CL_PROP(float, edge_radius, CLAMP(p_value, 0.0f, 1.0f))
	CL_PROP(CliffStyle, style, p_value)
	CL_PROP(float, wall_noise_amp, CLAMP(p_value, 0.0f, 10.0f))
	CL_PROP(float, wall_noise_freq, CLAMP(p_value, 0.01f, 5.0f))
	CL_PROP(float, warp_strength, CLAMP(p_value, 0.0f, 2.0f))
	CL_PROP(float, top_noise_amp, CLAMP(p_value, 0.0f, 5.0f))
	CL_PROP(float, slope_mask_steepness, CLAMP(p_value, 0.5f, 10.0f))
	CL_PROP(bool, flat_shaded, p_value)

	CL_PROP(int, strata_count, CLAMP(p_value, 0, 32))
	CL_PROP(float, strata_depth, CLAMP(p_value, 0.0f, 2.0f))
	CL_PROP(Color, base_color, p_value)
	CL_PROP(Color, top_color, p_value)
	CL_PROP(Color, crevice_color, p_value)
	CL_PROP(Color, moss_color, p_value)
	CL_PROP(float, moss_amount, CLAMP(p_value, 0.0f, 1.0f))
#undef CL_PROP
	// clang-format on
};

} // namespace godot

VARIANT_ENUM_CAST(godot::CliffMesh::CliffStyle);

#endif // CLIFF_MESH_H
