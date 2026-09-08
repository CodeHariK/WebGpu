/**
 * @file rock_mesh.h
 * @brief RockMesh: noise-displaced, faceted, clustered boulders as a deterministic ArrayMesh.
 */
#ifndef ROCK_MESH_H
#define ROCK_MESH_H

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
 * @class RockMesh
 * @brief A rock is a displaced sphere, not a convex hull: an icosphere pushed along its normals by
 * fractal value noise (`noise_*`, `ridged` for creases), so it has real dents and bulges; `facet_snap`
 * projects groups of vertices onto `facet_count` planes for broad split-stone faces; `flatten_bottom`
 * cuts the underside so it sits in the ground. `blob_count` > 1 makes a cluster — a PILE (heap with
 * smaller stones on top) or an OUTCROP (a line of boulders `spread` metres long for cliff bases and
 * mountain rubble). Vertex colours do the realism: height gradient `base_color` → `top_color`,
 * concavity darkening (`crevice_*`, from each vertex's radius against its neighbours), optional
 * sedimentary `strata` bands, and `moss_color` on up-facing surfaces. Flat or smooth shaded. Only
 * parameters are saved; geometry rebuilds on change. One convex collision shape per blob.
 */
class RockMesh : public ArrayMesh {
	GDCLASS(RockMesh,
			ArrayMesh)

public:
	enum Arrangement {
		ARRANGE_SINGLE = 0,
		ARRANGE_PILE = 1,
		ARRANGE_OUTCROP = 2,
		ARRANGE_STACK = 3 // Slabs stacked on top of each other (jointed / layered rock)
	};
	enum BaseShape {
		BASE_SPHERE = 0,
		BASE_CUBE = 1 // Blocky boulders: the icosphere projected onto a cube, `roundness` softens it
	};

private:
	int seed = 11;
	Vector3 size = Vector3(1.0f, 0.8f, 1.0f); // Half extents of the main blob, metres
	int detail = 2; // Icosphere subdivisions (0..3)

	// Shape
	BaseShape base_shape = BASE_SPHERE;
	float roundness = 0.35f; // CUBE: 0 = sharp block, 1 = back to a sphere
	Vector2 shear = Vector2(0.0f, 0.0f); // X/Z offset per unit of height (leaning slabs)
	float tilt = 0.0f; // Degrees each blob may lean off vertical (random axis), before the bottom cut
	float noise_amplitude = 0.32f; // Displacement as a fraction of the radius
	float noise_frequency = 1.4f;
	int noise_octaves = 3;
	bool ridged = false; // Creased, split-rock look
	float facet_snap = 0.55f; // 0 = organic, 1 = fully planar facets
	int facet_count = 9;
	float flatten_bottom = 0.3f; // Fraction of the height cut off underneath
	float sink = 0.05f; // Metres the cut plane sits below the origin
	bool flat_shaded = true;

	// Cluster
	Arrangement arrangement = ARRANGE_SINGLE;
	int blob_count = 1;
	float spread = 3.0f; // OUTCROP: length of the line, metres
	float blob_size_variation = 0.35f;

	// Colour
	Color base_color = Color(0.40f, 0.37f, 0.34f);
	Color top_color = Color(0.62f, 0.60f, 0.56f);
	Color crevice_color = Color(0.20f, 0.18f, 0.17f);
	float crevice_strength = 0.8f;
	int strata = 0; // 0 = off; else number of bands over the height
	float strata_strength = 0.25f;
	Color moss_color = Color(0.36f, 0.52f, 0.24f);
	float moss_amount = 0.0f; // 0 = none; blends onto up-facing surfaces
	float color_variation = 0.08f;

	bool _rebuild_queued = false;
	std::vector<PackedVector3Array> _blob_points; // Per blob, for collision hulls

	struct Blob {
		Vector3 centre;
		Vector3 half; // Half extents
		float yaw;
		float tint;
		uint32_t noise_seed;
		Vector3 tilt_axis;
		float tilt_rad = 0.0f;
	};
	void _layout(std::vector<Blob> &r_blobs) const;
	void _append_blob(
			const Blob &p_blob,
			PackedVector3Array &r_vertices,
			PackedVector3Array &r_normals,
			PackedVector2Array &r_uvs,
			PackedColorArray &r_colors,
			PackedInt32Array &r_indices,
			PackedVector3Array &r_points
	) const;
	void _queue_rebuild();

protected:
	static void _bind_methods();

public:
	RockMesh();
	~RockMesh();

	void rebuild();
	int get_collision_shape_count() const { return (int)_blob_points.size(); }
	/// Convex hull (Godot builds it from the point cloud) of blob p_index, local space.
	Ref<Shape3D> create_collision_shape(int p_index) const;

	// clang-format off
#define RK_PROP(m_type, m_name, m_clamp)      \
	void set_##m_name(m_type p_value) {       \
		m_type v = m_clamp;                   \
		if (m_name != v) {                    \
			m_name = v;                       \
			_queue_rebuild();                 \
		}                                     \
	}                                         \
	m_type get_##m_name() const { return m_name; }

	RK_PROP(int, seed, p_value)
	RK_PROP(Vector3, size, Vector3(MAX(0.02f, p_value.x), MAX(0.02f, p_value.y), MAX(0.02f, p_value.z)))
	RK_PROP(int, detail, CLAMP(p_value, 0, 3))
	RK_PROP(BaseShape, base_shape, p_value)
	RK_PROP(float, roundness, CLAMP(p_value, 0.0f, 1.0f))
	RK_PROP(Vector2, shear, Vector2(CLAMP(p_value.x, -2.0f, 2.0f), CLAMP(p_value.y, -2.0f, 2.0f)))
	RK_PROP(float, tilt, CLAMP(p_value, 0.0f, 60.0f))
	RK_PROP(float, noise_amplitude, CLAMP(p_value, 0.0f, 1.0f))
	RK_PROP(float, noise_frequency, CLAMP(p_value, 0.1f, 10.0f))
	RK_PROP(int, noise_octaves, CLAMP(p_value, 1, 6))
	RK_PROP(bool, ridged, p_value)
	RK_PROP(float, facet_snap, CLAMP(p_value, 0.0f, 1.0f))
	RK_PROP(int, facet_count, CLAMP(p_value, 3, 40))
	RK_PROP(float, flatten_bottom, CLAMP(p_value, 0.0f, 0.9f))
	RK_PROP(float, sink, CLAMP(p_value, -2.0f, 5.0f))
	RK_PROP(bool, flat_shaded, p_value)
	RK_PROP(Arrangement, arrangement, p_value)
	RK_PROP(int, blob_count, CLAMP(p_value, 1, 24))
	RK_PROP(float, spread, MAX(0.0f, p_value))
	RK_PROP(float, blob_size_variation, CLAMP(p_value, 0.0f, 1.0f))
	RK_PROP(Color, base_color, p_value)
	RK_PROP(Color, top_color, p_value)
	RK_PROP(Color, crevice_color, p_value)
	RK_PROP(float, crevice_strength, CLAMP(p_value, 0.0f, 3.0f))
	RK_PROP(int, strata, CLAMP(p_value, 0, 32))
	RK_PROP(float, strata_strength, CLAMP(p_value, 0.0f, 1.0f))
	RK_PROP(Color, moss_color, p_value)
	RK_PROP(float, moss_amount, CLAMP(p_value, 0.0f, 1.0f))
	RK_PROP(float, color_variation, CLAMP(p_value, 0.0f, 1.0f))
#undef RK_PROP
	// clang-format on
};

} // namespace godot

VARIANT_ENUM_CAST(godot::RockMesh::Arrangement);
VARIANT_ENUM_CAST(godot::RockMesh::BaseShape);

#endif // ROCK_MESH_H
