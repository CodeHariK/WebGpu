#include "convex_hull_rock.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/convex_polygon_shape3d.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#define CONVHULL_3D_ENABLE
#define CONVHULL_3D_USE_SINGLE_PRECISION
#include "utils/convhull_3d/convhull_3d.h"

#include <vector>

namespace godot {

struct SeededRand {
	uint32_t state;
	SeededRand(uint32_t s) : state(s) {}
	float rand_f() {
		state = state * 1664525 + 1013904223;
		return (float)(state & 0xFFFFFF) / 16777216.0f; // 0.0 to 1.0
	}
};

static Vector2 get_uv(const Vector3 &p) {
	Vector3 n = p.is_zero_approx() ? Vector3(0, 0, 1) : p.normalized();
	float u = 0.5f + Math::atan2(n.z, n.x) / (2.0f * Math_PI);
	float v = 0.5f - Math::asin(n.y) / Math_PI;
	return Vector2(u, v);
}

void ConvexHullRock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_collision_type", "type"), &ConvexHullRock::set_collision_type);
	ClassDB::bind_method(D_METHOD("get_collision_type"), &ConvexHullRock::get_collision_type);

	ClassDB::bind_method(D_METHOD("set_rock_seed", "seed"), &ConvexHullRock::set_rock_seed);
	ClassDB::bind_method(D_METHOD("get_rock_seed"), &ConvexHullRock::get_rock_seed);

	ClassDB::bind_method(D_METHOD("set_num_points", "points"), &ConvexHullRock::set_num_points);
	ClassDB::bind_method(D_METHOD("get_num_points"), &ConvexHullRock::get_num_points);

	ClassDB::bind_method(D_METHOD("set_size_scale", "scale"), &ConvexHullRock::set_size_scale);
	ClassDB::bind_method(D_METHOD("get_size_scale"), &ConvexHullRock::get_size_scale);

	ClassDB::bind_method(D_METHOD("set_flat_shaded", "flat_shaded"), &ConvexHullRock::set_flat_shaded);
	ClassDB::bind_method(D_METHOD("get_flat_shaded"), &ConvexHullRock::get_flat_shaded);

	ClassDB::bind_method(D_METHOD("generate_rock"), &ConvexHullRock::generate_rock);
	ClassDB::bind_method(D_METHOD("update_collision"), &ConvexHullRock::update_collision);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_type", PROPERTY_HINT_ENUM, "None,Convex,Concave"), "set_collision_type", "get_collision_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rock_seed"), "set_rock_seed", "get_rock_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "num_points"), "set_num_points", "get_num_points");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size_scale"), "set_size_scale", "get_size_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flat_shaded"), "set_flat_shaded", "get_flat_shaded");

	BIND_ENUM_CONSTANT(COLLISION_NONE);
	BIND_ENUM_CONSTANT(COLLISION_CONVEX);
	BIND_ENUM_CONSTANT(COLLISION_CONCAVE);
}

ConvexHullRock::ConvexHullRock() {
	collision_type = COLLISION_CONVEX;
	rock_seed = 12345;
	num_points = 30;
	size_scale = Vector3(1.0f, 1.0f, 1.0f);
	flat_shaded = true;
}

ConvexHullRock::~ConvexHullRock() {
}

void ConvexHullRock::_ready() {
	static_body = Object::cast_to<StaticBody3D>(get_node_or_null("StaticBody3D"));
	if (static_body) {
		collision_shape = Object::cast_to<CollisionShape3D>(static_body->get_node_or_null("CollisionShape3D"));
	}

	generate_rock();
}

void ConvexHullRock::set_collision_type(CollisionType p_type) {
	if (collision_type != p_type) {
		collision_type = p_type;
		if (is_inside_tree()) {
			update_collision();
		}
	}
}

ConvexHullRock::CollisionType ConvexHullRock::get_collision_type() const {
	return collision_type;
}

void ConvexHullRock::set_rock_seed(int p_seed) {
	if (rock_seed != p_seed) {
		rock_seed = p_seed;
		if (is_inside_tree()) {
			generate_rock();
		}
	}
}

int ConvexHullRock::get_rock_seed() const {
	return rock_seed;
}

void ConvexHullRock::set_num_points(int p_points) {
	int clamped_points = p_points < 4 ? 4 : p_points;
	if (num_points != clamped_points) {
		num_points = clamped_points;
		if (is_inside_tree()) {
			generate_rock();
		}
	}
}

int ConvexHullRock::get_num_points() const {
	return num_points;
}

void ConvexHullRock::set_size_scale(const Vector3 &p_scale) {
	if (size_scale != p_scale) {
		size_scale = p_scale;
		if (is_inside_tree()) {
			generate_rock();
		}
	}
}

Vector3 ConvexHullRock::get_size_scale() const {
	return size_scale;
}

void ConvexHullRock::set_flat_shaded(bool p_flat) {
	if (flat_shaded != p_flat) {
		flat_shaded = p_flat;
		if (is_inside_tree()) {
			generate_rock();
		}
	}
}

bool ConvexHullRock::get_flat_shaded() const {
	return flat_shaded;
}

void ConvexHullRock::generate_rock() {
	if (num_points < 4) {
		num_points = 4;
	}

	std::vector<Vector3> points;
	SeededRand rng(rock_seed);
	for (int i = 0; i < num_points; ++i) {
		Vector3 p;
		while (true) {
			float x = rng.rand_f() * 2.0f - 1.0f;
			float y = rng.rand_f() * 2.0f - 1.0f;
			float z = rng.rand_f() * 2.0f - 1.0f;
			Vector3 v(x, y, z);
			if (v.length_squared() <= 1.0f) {
				p = v;
				break;
			}
		}
		points.push_back(p * size_scale);
	}

	// Build the convex hull
	int *faces = nullptr;
	int n_faces = 0;

	std::vector<ch_vertex> ch_vertices(points.size());
	for (size_t i = 0; i < points.size(); ++i) {
		ch_vertices[i].x = points[i].x;
		ch_vertices[i].y = points[i].y;
		ch_vertices[i].z = points[i].z;
	}

	convhull_3d_build(ch_vertices.data(), (int)ch_vertices.size(), &faces, &n_faces);

	if (!faces || n_faces == 0) {
		if (faces) {
			::free(faces);
		}
		set_mesh(Ref<Mesh>());
		update_collision();
		return;
	}

	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	PackedInt32Array indices;

	if (flat_shaded) {
		int vertex_count = n_faces * 3;
		vertices.resize(vertex_count);
		normals.resize(vertex_count);
		uvs.resize(vertex_count);
		indices.resize(vertex_count);

		for (int i = 0; i < n_faces; ++i) {
			int idx0 = faces[i * 3 + 0];
			int idx1 = faces[i * 3 + 1];
			int idx2 = faces[i * 3 + 2];

			Vector3 v0 = points[idx0];
			Vector3 v1 = points[idx1];
			Vector3 v2 = points[idx2];

			Vector3 center = (v0 + v1 + v2) / 3.0f;
			Vector3 normal = (v1 - v0).cross(v2 - v0);

			// We want the winding order to be CW (facing outwards in this project).
			// If normal.dot(center) > 0.0f, the cross product points outwards (CCW).
			// We swap v1 and v2 to make it CW.
			if (normal.dot(center) > 0.0f) {
				std::swap(v1, v2);
			}

			// Compute the outward pointing normal vector for lighting
			normal = (v1 - v0).cross(v2 - v0); // points inwards (CW)
			normal = -normal; // points outwards

			if (!normal.is_zero_approx()) {
				normal = normal.normalized();
			} else {
				normal = Vector3(0, 1, 0);
			}

			int v_idx = i * 3;
			vertices[v_idx + 0] = v0;
			vertices[v_idx + 1] = v1;
			vertices[v_idx + 2] = v2;

			normals[v_idx + 0] = normal;
			normals[v_idx + 1] = normal;
			normals[v_idx + 2] = normal;

			uvs[v_idx + 0] = get_uv(v0);
			uvs[v_idx + 1] = get_uv(v1);
			uvs[v_idx + 2] = get_uv(v2);

			indices[v_idx + 0] = v_idx + 0;
			indices[v_idx + 1] = v_idx + 1;
			indices[v_idx + 2] = v_idx + 2;
		}
	} else {
		// Smooth shaded
		vertices.resize(points.size());
		uvs.resize(points.size());
		normals.resize(points.size());
		for (size_t i = 0; i < points.size(); ++i) {
			vertices[i] = points[i];
			uvs[i] = get_uv(points[i]);
			normals[i] = Vector3(0, 0, 0);
		}

		indices.resize(n_faces * 3);
		for (int i = 0; i < n_faces; ++i) {
			int idx0 = faces[i * 3 + 0];
			int idx1 = faces[i * 3 + 1];
			int idx2 = faces[i * 3 + 2];

			Vector3 v0 = points[idx0];
			Vector3 v1 = points[idx1];
			Vector3 v2 = points[idx2];

			Vector3 center = (v0 + v1 + v2) / 3.0f;
			Vector3 normal = (v1 - v0).cross(v2 - v0);

			// We want the winding order to be CW (facing outwards in this project).
			// If normal.dot(center) > 0.0f, the cross product points outwards (CCW).
			// We swap idx1 and idx2 to make it CW.
			if (normal.dot(center) > 0.0f) {
				std::swap(idx1, idx2);
			}

			// Compute the outward pointing normal vector for lighting
			normal = (points[idx1] - points[idx0]).cross(points[idx2] - points[idx0]); // points inwards (CW)
			normal = -normal; // points outwards

			if (!normal.is_zero_approx()) {
				normal = normal.normalized();
			} else {
				normal = Vector3(0, 1, 0);
			}

			int i_idx = i * 3;
			indices[i_idx + 0] = idx0;
			indices[i_idx + 1] = idx1;
			indices[i_idx + 2] = idx2;

			normals[idx0] += normal;
			normals[idx1] += normal;
			normals[idx2] += normal;
		}

		for (int i = 0; i < normals.size(); ++i) {
			if (!normals[i].is_zero_approx()) {
				normals[i] = normals[i].normalized();
			} else {
				normals[i] = Vector3(0, 1, 0);
			}
		}
	}

	if (faces) {
		::free(faces);
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> am;
	am.instantiate();
	if (vertices.size() > 0 && indices.size() > 0) {
		am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	}
	set_mesh(am);

	update_collision();
}

void ConvexHullRock::update_collision() {
	if (collision_type == COLLISION_NONE) {
		if (static_body) {
			static_body->queue_free();
			static_body = nullptr;
			collision_shape = nullptr;
		}
		return;
	}

	if (!static_body) {
		static_body = Object::cast_to<StaticBody3D>(get_node_or_null("StaticBody3D"));
		if (!static_body) {
			static_body = memnew(StaticBody3D);
			static_body->set_name("StaticBody3D");
			add_child(static_body);
			if (get_owner()) {
				static_body->set_owner(get_owner());
			}
		}
	}

	if (!collision_shape) {
		collision_shape = Object::cast_to<CollisionShape3D>(static_body->get_node_or_null("CollisionShape3D"));
		if (!collision_shape) {
			collision_shape = memnew(CollisionShape3D);
			collision_shape->set_name("CollisionShape3D");
			static_body->add_child(collision_shape);
			if (get_owner()) {
				collision_shape->set_owner(get_owner());
			}
		}
	}

	Ref<Mesh> mesh = get_mesh();
	if (mesh.is_null()) {
		collision_shape->set_shape(Ref<Shape3D>());
		return;
	}

	if (collision_type == COLLISION_CONVEX) {
		Ref<ConvexPolygonShape3D> convex_shape = mesh->create_convex_shape(true, false);
		collision_shape->set_shape(convex_shape);
	} else if (collision_type == COLLISION_CONCAVE) {
		Ref<ConcavePolygonShape3D> concave_shape = mesh->create_trimesh_shape();
		collision_shape->set_shape(concave_shape);
	}
}

} // namespace godot
