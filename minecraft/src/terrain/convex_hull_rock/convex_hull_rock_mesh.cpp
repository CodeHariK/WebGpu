/**
 * @file convex_hull_rock_mesh.cpp
 * @brief ConvexHullRockMesh: point generation, hull, surface build.
 */
#include "convex_hull_rock_mesh.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// Header-only hull; the implementation is compiled into this translation unit only.
#define CONVHULL_3D_ENABLE
#define CONVHULL_3D_USE_SINGLE_PRECISION
#include "utils/convhull_3d/convhull_3d.h"

#include <algorithm>
#include <cstdlib>

namespace godot {

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

/// PCG-style generator; the high bits are used, unlike an LCG's low bits which cycle visibly.
struct RockRNG {
	uint64_t state;
	explicit RockRNG(uint64_t p_seed) : state(p_seed * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL) {}
	uint32_t next() {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		return uint32_t(state >> 32);
	}
	float next_float() { return float(next()) / 4294967296.0f; }
	/// Uniformly distributed unit vector (rejection sampling in the cube).
	Vector3 next_direction() {
		while (true) {
			Vector3 v(next_float() * 2.0f - 1.0f, next_float() * 2.0f - 1.0f, next_float() * 2.0f - 1.0f);
			const float l2 = v.length_squared();
			if (l2 > 1e-4f && l2 <= 1.0f) {
				return v / Math::sqrt(l2);
			}
		}
	}
};

/// Planar UV of a point on a face: project onto the plane perpendicular to the face normal's dominant axis.
static Vector2 planar_uv(
		const Vector3 &p_point,
		const Vector3 &p_normal,
		float p_scale
) {
	const Vector3 a(Math::abs(p_normal.x), Math::abs(p_normal.y), Math::abs(p_normal.z));
	Vector2 uv;
	if (a.x >= a.y && a.x >= a.z) {
		uv = Vector2(p_point.z, p_point.y);
	} else if (a.y >= a.z) {
		uv = Vector2(p_point.x, p_point.z);
	} else {
		uv = Vector2(p_point.x, p_point.y);
	}
	return uv * p_scale;
}

/// Reorders (i1, i2) so the triangle winds clockwise seen from outside (Godot's front face) and returns
/// its outward unit normal.
static Vector3 orient_face(
		const std::vector<Vector3> &p_points,
		const Vector3 &p_centroid,
		int p_i0,
		int &r_i1,
		int &r_i2
) {
	const Vector3 &v0 = p_points[p_i0];
	Vector3 n = (p_points[r_i1] - v0).cross(p_points[r_i2] - v0);
	const Vector3 face_center = (v0 + p_points[r_i1] + p_points[r_i2]) / 3.0f;
	// Counter-clockwise cross product points along the outward direction; Godot wants clockwise.
	if (n.dot(face_center - p_centroid) > 0.0f) {
		std::swap(r_i1, r_i2);
		n = -n;
	}
	n = -n; // Clockwise cross points inwards; flip for lighting.
	return n.is_zero_approx() ? Vector3(0, 1, 0) : n.normalized();
}

// ---------------------------------------------------------------------------------------------
// Bindings
// ---------------------------------------------------------------------------------------------

void ConvexHullRockMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_rock_seed", "seed"), &ConvexHullRockMesh::set_rock_seed);
	ClassDB::bind_method(D_METHOD("get_rock_seed"), &ConvexHullRockMesh::get_rock_seed);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rock_seed"), "set_rock_seed", "get_rock_seed");

	ClassDB::bind_method(D_METHOD("set_num_points", "points"), &ConvexHullRockMesh::set_num_points);
	ClassDB::bind_method(D_METHOD("get_num_points"), &ConvexHullRockMesh::get_num_points);
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "num_points", PROPERTY_HINT_RANGE, "4,400,1"), "set_num_points", "get_num_points"
	);

	ClassDB::bind_method(D_METHOD("set_size_scale", "half_extents"), &ConvexHullRockMesh::set_size_scale);
	ClassDB::bind_method(D_METHOD("get_size_scale"), &ConvexHullRockMesh::get_size_scale);
	ADD_PROPERTY(
			PropertyInfo(Variant::VECTOR3, "size_scale", PROPERTY_HINT_NONE, "suffix:m"), "set_size_scale",
			"get_size_scale"
	);

	ClassDB::bind_method(D_METHOD("set_roughness", "roughness"), &ConvexHullRockMesh::set_roughness);
	ClassDB::bind_method(D_METHOD("get_roughness"), &ConvexHullRockMesh::get_roughness);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "roughness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_roughness", "get_roughness"
	);

	ClassDB::bind_method(D_METHOD("set_flatten_bottom", "fraction"), &ConvexHullRockMesh::set_flatten_bottom);
	ClassDB::bind_method(D_METHOD("get_flatten_bottom"), &ConvexHullRockMesh::get_flatten_bottom);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "flatten_bottom", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_flatten_bottom",
			"get_flatten_bottom"
	);

	ClassDB::bind_method(D_METHOD("set_flat_shaded", "flat_shaded"), &ConvexHullRockMesh::set_flat_shaded);
	ClassDB::bind_method(D_METHOD("get_flat_shaded"), &ConvexHullRockMesh::get_flat_shaded);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flat_shaded"), "set_flat_shaded", "get_flat_shaded");

	ClassDB::bind_method(D_METHOD("set_uv_scale", "repeats_per_metre"), &ConvexHullRockMesh::set_uv_scale);
	ClassDB::bind_method(D_METHOD("get_uv_scale"), &ConvexHullRockMesh::get_uv_scale);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "uv_scale", PROPERTY_HINT_RANGE, "0.01,10,0.01"), "set_uv_scale",
			"get_uv_scale"
	);

	ClassDB::bind_method(D_METHOD("rebuild"), &ConvexHullRockMesh::rebuild);
}

ConvexHullRockMesh::ConvexHullRockMesh() { _queue_rebuild(); }

ConvexHullRockMesh::~ConvexHullRockMesh() {}

// ---------------------------------------------------------------------------------------------
// Properties (each schedules one rebuild for the frame, so loading five properties builds once)
// ---------------------------------------------------------------------------------------------

void ConvexHullRockMesh::_queue_rebuild() {
	if (!_rebuild_queued) {
		_rebuild_queued = true;
		call_deferred("rebuild");
	}
}

void ConvexHullRockMesh::set_rock_seed(int p_seed) {
	if (rock_seed != p_seed) {
		rock_seed = p_seed;
		_queue_rebuild();
	}
}

void ConvexHullRockMesh::set_num_points(int p_points) {
	p_points = MAX(4, p_points);
	if (num_points != p_points) {
		num_points = p_points;
		_queue_rebuild();
	}
}

void ConvexHullRockMesh::set_size_scale(const Vector3 &p_scale) {
	if (size_scale != p_scale) {
		size_scale = p_scale;
		_queue_rebuild();
	}
}

void ConvexHullRockMesh::set_roughness(float p_roughness) {
	p_roughness = CLAMP(p_roughness, 0.0f, 1.0f);
	if (roughness != p_roughness) {
		roughness = p_roughness;
		_queue_rebuild();
	}
}

void ConvexHullRockMesh::set_flatten_bottom(float p_fraction) {
	p_fraction = CLAMP(p_fraction, 0.0f, 1.0f);
	if (flatten_bottom != p_fraction) {
		flatten_bottom = p_fraction;
		_queue_rebuild();
	}
}

void ConvexHullRockMesh::set_flat_shaded(bool p_flat) {
	if (flat_shaded != p_flat) {
		flat_shaded = p_flat;
		_queue_rebuild();
	}
}

void ConvexHullRockMesh::set_uv_scale(float p_scale) {
	p_scale = MAX(0.01f, p_scale);
	if (uv_scale != p_scale) {
		uv_scale = p_scale;
		_queue_rebuild();
	}
}

// ---------------------------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------------------------

/**
 * @brief Points on a unit sphere pulled inwards by up to `roughness`, scaled by the half extents, then
 * clamped to the bottom plane when flattening. Clamped points get a hair of jitter so the hull code
 * never sees an exactly coplanar cluster.
 */
void ConvexHullRockMesh::_generate_points(std::vector<Vector3> &r_points) const {
	RockRNG rng((uint64_t)(uint32_t)rock_seed);
	r_points.resize(num_points);
	const float floor_y = -size_scale.y * (1.0f - flatten_bottom);
	for (int i = 0; i < num_points; ++i) {
		const float r = 1.0f - roughness * rng.next_float();
		Vector3 p = rng.next_direction() * r * size_scale;
		if (flatten_bottom > 0.0f && p.y < floor_y) {
			p.y = floor_y + rng.next_float() * 1e-3f * size_scale.y;
		}
		r_points[i] = p;
	}
}

void ConvexHullRockMesh::rebuild() {
	_rebuild_queued = false;
	clear_surfaces();

	std::vector<Vector3> points;
	_generate_points(points);

	std::vector<ch_vertex> in(points.size());
	for (size_t i = 0; i < points.size(); ++i) {
		in[i].x = points[i].x;
		in[i].y = points[i].y;
		in[i].z = points[i].z;
	}
	int *faces = nullptr;
	int face_count = 0;
	convhull_3d_build(in.data(), (int)in.size(), &faces, &face_count);
	if (faces && face_count > 0) {
		_build_surface(points, faces, face_count);
	} else {
		UtilityFunctions::push_warning("[ConvexHullRockMesh] hull failed (degenerate points); mesh left empty.");
	}
	if (faces) {
		::free(faces);
	}
	emit_changed();
}

/// Flat: three unique vertices per face with the face normal. Smooth: shared vertices, area-weighted
/// vertex normals. Both use per-face planar UVs (smooth reuses the vertex's first face).
void ConvexHullRockMesh::_build_surface(
		const std::vector<Vector3> &p_points,
		const int *p_faces,
		int p_face_count
) {
	Vector3 centroid;
	for (const Vector3 &p : p_points) {
		centroid += p;
	}
	centroid /= (float)p_points.size();

	PackedVector3Array vertices, normals;
	PackedVector2Array uvs;
	PackedInt32Array indices;

	if (flat_shaded) {
		const int count = p_face_count * 3;
		vertices.resize(count);
		normals.resize(count);
		uvs.resize(count);
		indices.resize(count);
		for (int f = 0; f < p_face_count; ++f) {
			int i0 = p_faces[f * 3], i1 = p_faces[f * 3 + 1], i2 = p_faces[f * 3 + 2];
			const Vector3 n = orient_face(p_points, centroid, i0, i1, i2);
			const int idx[3] = { i0, i1, i2 };
			for (int k = 0; k < 3; ++k) {
				const int v = f * 3 + k;
				vertices[v] = p_points[idx[k]];
				normals[v] = n;
				uvs[v] = planar_uv(p_points[idx[k]], n, uv_scale);
				indices[v] = v;
			}
		}
	} else {
		const int n_pts = (int)p_points.size();
		vertices.resize(n_pts);
		normals.resize(n_pts);
		uvs.resize(n_pts);
		std::vector<bool> has_uv(n_pts, false);
		for (int i = 0; i < n_pts; ++i) {
			vertices[i] = p_points[i];
			normals[i] = Vector3();
		}
		indices.resize(p_face_count * 3);
		for (int f = 0; f < p_face_count; ++f) {
			int i0 = p_faces[f * 3], i1 = p_faces[f * 3 + 1], i2 = p_faces[f * 3 + 2];
			const Vector3 n = orient_face(p_points, centroid, i0, i1, i2);
			const Vector3 area_n = (p_points[i1] - p_points[i0]).cross(p_points[i2] - p_points[i0]); // |n| = 2*area
			const int idx[3] = { i0, i1, i2 };
			for (int k = 0; k < 3; ++k) {
				indices[f * 3 + k] = idx[k];
				normals[idx[k]] = normals[idx[k]] + n * area_n.length();
				if (!has_uv[idx[k]]) {
					uvs[idx[k]] = planar_uv(p_points[idx[k]], n, uv_scale);
					has_uv[idx[k]] = true;
				}
			}
		}
		for (int i = 0; i < n_pts; ++i) {
			normals[i] = normals[i].is_zero_approx() ? Vector3(0, 1, 0) : normals[i].normalized();
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;
	add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
}

} // namespace godot
