/**
 * @file crystal_cluster_mesh.h
 * @brief CrystalClusterMesh: a deterministic cluster of faceted crystal prisms as an ArrayMesh.
 */
#ifndef CRYSTAL_CLUSTER_MESH_H
#define CRYSTAL_CLUSTER_MESH_H

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
 * @class CrystalClusterMesh
 * @brief One dominant crystal in the middle, `crystal_count` secondary ones leaning outward on a ring,
 * `small_count` shards between them and a skirt of `pebble_count` squat crystals at the base — the
 * classic amethyst-cluster silhouette. Every crystal is an n-sided tapered prism with a pyramid tip,
 * flat-shaded (faceted) or smooth, vertex-coloured from `base_color` at the ground to `tip_color` at
 * the tip (UV.y = the same 0..1 so the prop shader can glow the tips). Only the parameters are saved;
 * geometry is rebuilt on load and on every change. `create_collision_shape()` hulls the main crystal.
 */
class CrystalClusterMesh : public ArrayMesh {
	GDCLASS(CrystalClusterMesh,
			ArrayMesh)

private:
	struct Crystal {
		Transform3D frame; // Origin at the ground, Y along the crystal axis
		float height = 1.0f; // Ground to tip
		float radius = 0.3f; // Base radius
		float taper = 0.75f; // Top ring radius / base radius
		float tip = 0.3f; // Tip pyramid height as a fraction of height
		float tint = 0.0f; // Per-crystal colour variation, -1..1
	};

	int seed = 7;
	float height = 2.0f; // Main crystal, metres
	float radius = 0.32f; // Main crystal base radius
	int sides = 6;
	float taper = 0.8f;
	float tip_ratio = 0.3f;
	float sink = 0.1f; // Fraction of each crystal's height buried below the origin plane
	int crystal_count = 5; // Secondary ring
	int small_count = 6; // Shards
	int pebble_count = 22; // Base skirt
	float spread = 0.28f; // Ring radius as a fraction of height
	float lean = 22.0f; // Degrees the ring crystals lean outward (± variation)
	float height_variation = 0.3f;
	bool faceted = true;
	Color base_color = Color(0.33f, 0.12f, 0.62f);
	Color tip_color = Color(0.86f, 0.72f, 1.0f);
	float color_variation = 0.08f;
	float gradient_power = 1.4f; // >1 keeps the base dark longer

	bool _rebuild_queued = false;
	std::vector<Vector3> _main_points; // For the collision hull

	void _layout(std::vector<Crystal> &r_crystals) const;
	void _append_crystal(
			const Crystal &p_crystal,
			PackedVector3Array &r_vertices,
			PackedVector3Array &r_normals,
			PackedVector2Array &r_uvs,
			PackedColorArray &r_colors,
			PackedInt32Array &r_indices,
			std::vector<Vector3> *r_points
	) const;
	void _queue_rebuild();

protected:
	static void _bind_methods();

public:
	CrystalClusterMesh();
	~CrystalClusterMesh();

	/// Rebuilds the geometry from the current parameters (called automatically on change).
	void rebuild();
	/// Convex hull of the main crystal (local space), or null before the first build.
	Ref<Shape3D> create_collision_shape() const;

	// clang-format off
#define CC_PROP(m_type, m_name, m_clamp)      \
	void set_##m_name(m_type p_value) {       \
		m_type v = m_clamp;                   \
		if (m_name != v) {                    \
			m_name = v;                       \
			_queue_rebuild();                 \
		}                                     \
	}                                         \
	m_type get_##m_name() const { return m_name; }

	CC_PROP(int, seed, p_value)
	CC_PROP(float, height, MAX(0.05f, p_value))
	CC_PROP(float, radius, MAX(0.01f, p_value))
	CC_PROP(int, sides, CLAMP(p_value, 3, 16))
	CC_PROP(float, taper, CLAMP(p_value, 0.1f, 1.5f))
	CC_PROP(float, tip_ratio, CLAMP(p_value, 0.0f, 0.9f))
	CC_PROP(float, sink, CLAMP(p_value, 0.0f, 0.9f))
	CC_PROP(int, crystal_count, CLAMP(p_value, 0, 32))
	CC_PROP(int, small_count, CLAMP(p_value, 0, 64))
	CC_PROP(int, pebble_count, CLAMP(p_value, 0, 128))
	CC_PROP(float, spread, CLAMP(p_value, 0.0f, 3.0f))
	CC_PROP(float, lean, CLAMP(p_value, 0.0f, 80.0f))
	CC_PROP(float, height_variation, CLAMP(p_value, 0.0f, 1.0f))
	CC_PROP(bool, faceted, p_value)
	CC_PROP(Color, base_color, p_value)
	CC_PROP(Color, tip_color, p_value)
	CC_PROP(float, color_variation, CLAMP(p_value, 0.0f, 1.0f))
	CC_PROP(float, gradient_power, CLAMP(p_value, 0.2f, 5.0f))
#undef CC_PROP
	// clang-format on
};

} // namespace godot

#endif // CRYSTAL_CLUSTER_MESH_H
