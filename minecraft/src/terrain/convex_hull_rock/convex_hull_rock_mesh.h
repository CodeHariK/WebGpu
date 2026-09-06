/**
 * @file convex_hull_rock_mesh.h
 * @brief ConvexHullRockMesh: a procedural low-poly rock as an ArrayMesh resource.
 */
#ifndef CONVEX_HULL_ROCK_MESH_H
#define CONVEX_HULL_ROCK_MESH_H

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {

/**
 * @class ConvexHullRockMesh
 * @brief Deterministic rock: jittered points on a (scaled, optionally bottom-flattened) sphere, wrapped
 * in a convex hull, flat- or smooth-shaded, with per-face planar UVs.
 *
 * Being a Mesh resource it can be assigned to any MeshInstance3D, a MultiMesh, or a
 * TerrainSplineScatter, and `create_convex_shape()` gives the matching collision shape. Only the
 * parameters are saved; the geometry is rebuilt on load and whenever a parameter changes.
 */
class ConvexHullRockMesh : public ArrayMesh {
	GDCLASS(ConvexHullRockMesh,
			ArrayMesh)

private:
	int rock_seed = 12345;
	int num_points = 24; // Hull input points; fewer = chunkier
	Vector3 size_scale = Vector3(1.0f, 1.0f, 1.0f); // Half extents in metres
	float roughness = 0.35f; // 0 = points on the sphere (round), 1 = anywhere down to the centre (jagged)
	float flatten_bottom = 0.0f; // 0 = none, 1 = cut at the centre plane; sits on terrain without floating
	bool flat_shaded = true;
	float uv_scale = 1.0f; // Texture repeats per metre

	bool _rebuild_queued = false;

	void _generate_points(std::vector<Vector3> &r_points) const;
	void _build_surface(
			const std::vector<Vector3> &p_points,
			const int *p_faces,
			int p_face_count
	);
	void _queue_rebuild();

protected:
	static void _bind_methods();

public:
	ConvexHullRockMesh();
	~ConvexHullRockMesh();

	/// Rebuilds the geometry from the current parameters (called automatically on change).
	void rebuild();

	void set_rock_seed(int p_seed);
	int get_rock_seed() const { return rock_seed; }
	void set_num_points(int p_points);
	int get_num_points() const { return num_points; }
	void set_size_scale(const Vector3 &p_scale);
	Vector3 get_size_scale() const { return size_scale; }
	void set_roughness(float p_roughness);
	float get_roughness() const { return roughness; }
	void set_flatten_bottom(float p_fraction);
	float get_flatten_bottom() const { return flatten_bottom; }
	void set_flat_shaded(bool p_flat);
	bool get_flat_shaded() const { return flat_shaded; }
	void set_uv_scale(float p_scale);
	float get_uv_scale() const { return uv_scale; }
};

} // namespace godot

#endif // CONVEX_HULL_ROCK_MESH_H
