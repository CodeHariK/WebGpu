/**
 * @file tree_mesh.h
 * @brief FoliageTreeMesh: an Animal-Crossing-style tree — a dark canopy blob densely covered in leaf
 *        or blossom cards — with buttress roots and optional hanging cherries, as one ArrayMesh.
 */
#ifndef TREE_MESH_H
#define TREE_MESH_H

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <vector>

namespace godot {

/**
 * @class FoliageTreeMesh
 * @brief Stylized tree the way Animal Crossing builds one: a canopy volume (round lobes for deciduous,
 * a cone for pine) rendered as a dark inner shell so gaps read as shadow, then `leaf_count` small
 * double-sided cards scattered densely over its surface — leaf-shaped (`FOLIAGE_LEAVES`) or rounded
 * blossom puffs mixing `canopy` pinks with `blossom_color` white (`FOLIAGE_BLOSSOM`). Each card faces
 * roughly outward with a random tilt and roll, so the silhouette breaks into leaf edges. The trunk
 * flares into `root_count` buttress roots at the base; optional cherries hang on Y-stems. Colour comes
 * from the height gradient; UV.y = height / total so the prop shader glows the upper foliage. Only
 * parameters are saved; geometry rebuilds on change. `create_collision_shape()` = a trunk cylinder.
 */
class FoliageTreeMesh : public ArrayMesh {
	GDCLASS(FoliageTreeMesh,
			ArrayMesh)

public:
	enum Species { SPECIES_DECIDUOUS = 0, SPECIES_PINE = 1 };
	enum FoliageStyle { FOLIAGE_LEAVES = 0, FOLIAGE_BLOSSOM = 1 };

private:
	int seed = 5;
	Species species = SPECIES_DECIDUOUS;

	// Trunk
	float trunk_height = 1.4f;
	float trunk_radius = 0.2f;
	float trunk_flare = 1.5f; // Base radius / top radius
	int trunk_sides = 8;
	int root_count = 5; // Buttress roots splaying at the base (0 = plain cylinder)
	float root_spread = 0.5f; // How far roots reach out, × base radius
	Color trunk_color = Color(0.40f, 0.28f, 0.18f);

	// Canopy volume
	float canopy_radius = 1.4f;
	float canopy_height = 2.0f;
	float canopy_flatten = 0.9f; // Y scale of the canopy (< 1 = wider than tall, AC cap)
	int lobe_count = 7;
	float lobe_jitter = 0.4f;
	int canopy_detail = 1; // Inner-shell icosphere subdivisions

	// Foliage cards
	FoliageStyle foliage_style = FOLIAGE_LEAVES;
	int leaf_count = 550;
	float leaf_size = 0.32f;
	float leaf_tilt = 32.0f; // Degrees the cards tilt off the surface normal
	Color canopy_bottom = Color(0.14f, 0.40f, 0.17f);
	Color canopy_top = Color(0.44f, 0.74f, 0.32f);
	float color_variation = 0.10f;
	Color blossom_color = Color(1.0f, 0.97f, 0.98f); // BLOSSOM: the white highlight clusters
	float blossom_white_fraction = 0.30f; // BLOSSOM: share of cards that are white

	// Cherries
	int cherry_count = 0;
	float cherry_radius = 0.11f;
	Color cherry_color = Color(0.85f, 0.10f, 0.14f);

	bool _rebuild_queued = false;

	struct Builder {
		PackedVector3Array vertices, normals;
		PackedVector2Array uvs;
		PackedColorArray colors;
		PackedInt32Array indices;
	};

	void _build(Builder &b) const;
	void _queue_rebuild();

public:
	/// One batch of instances for a MultiMesh: per-instance transform and colour.
	struct Scatter {
		std::vector<Transform3D> xforms;
		PackedColorArray colors;
	};

private:
	struct Lobe {
		Vector3 c;
		float r;
	};
	void _canopy_lobes(
			std::vector<Lobe> &r_lobes,
			float &r_ymin,
			float &r_ymax
	) const;

protected:
	static void _bind_methods();

public:
	FoliageTreeMesh();
	~FoliageTreeMesh();

	void rebuild();
	Ref<Shape3D> create_collision_shape() const;

	/// Instance transforms + colours for the leaf cards over the canopy (any thread). p_size scales
	/// each instance, so a unit-ish leaf / quad mesh comes out at `leaf_size`.
	void compute_leaves(Scatter &r_out) const;
	/// Instance transforms + colours for the fruit hanging at the canopy edge.
	void compute_fruit(Scatter &r_out) const;
	float get_canopy_bottom_y() const { return trunk_height; }

	// clang-format off
#define FT_PROP(m_type, m_name, m_clamp)      \
	void set_##m_name(m_type p_value) {       \
		m_type v = m_clamp;                   \
		if (m_name != v) {                    \
			m_name = v;                       \
			_queue_rebuild();                 \
		}                                     \
	}                                         \
	m_type get_##m_name() const { return m_name; }

	FT_PROP(int, seed, p_value)
	FT_PROP(Species, species, p_value)
	FT_PROP(float, trunk_height, MAX(0.05f, p_value))
	FT_PROP(float, trunk_radius, MAX(0.01f, p_value))
	FT_PROP(float, trunk_flare, CLAMP(p_value, 1.0f, 4.0f))
	FT_PROP(int, trunk_sides, CLAMP(p_value, 3, 20))
	FT_PROP(int, root_count, CLAMP(p_value, 0, 12))
	FT_PROP(float, root_spread, CLAMP(p_value, 0.0f, 2.0f))
	FT_PROP(Color, trunk_color, p_value)
	FT_PROP(float, canopy_radius, MAX(0.05f, p_value))
	FT_PROP(float, canopy_height, MAX(0.1f, p_value))
	FT_PROP(float, canopy_flatten, CLAMP(p_value, 0.3f, 1.5f))
	FT_PROP(int, lobe_count, CLAMP(p_value, 1, 12))
	FT_PROP(float, lobe_jitter, CLAMP(p_value, 0.0f, 1.5f))
	FT_PROP(int, canopy_detail, CLAMP(p_value, 0, 2))
	FT_PROP(FoliageStyle, foliage_style, p_value)
	FT_PROP(int, leaf_count, CLAMP(p_value, 0, 4000))
	FT_PROP(float, leaf_size, MAX(0.02f, p_value))
	FT_PROP(float, leaf_tilt, CLAMP(p_value, 0.0f, 90.0f))
	FT_PROP(Color, canopy_bottom, p_value)
	FT_PROP(Color, canopy_top, p_value)
	FT_PROP(float, color_variation, CLAMP(p_value, 0.0f, 1.0f))
	FT_PROP(Color, blossom_color, p_value)
	FT_PROP(float, blossom_white_fraction, CLAMP(p_value, 0.0f, 1.0f))
	FT_PROP(int, cherry_count, CLAMP(p_value, 0, 60))
	FT_PROP(float, cherry_radius, MAX(0.01f, p_value))
	FT_PROP(Color, cherry_color, p_value)
#undef FT_PROP
	// clang-format on
};

} // namespace godot

VARIANT_ENUM_CAST(godot::FoliageTreeMesh::Species);
VARIANT_ENUM_CAST(godot::FoliageTreeMesh::FoliageStyle);

#endif // TREE_MESH_H
