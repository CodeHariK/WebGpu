/**
 * @file crystal_cluster_mesh.cpp
 * @brief CrystalClusterMesh: cluster layout and prism geometry.
 */
#include "crystal_cluster_mesh.h"
#include <algorithm>
#include <cstdint>
#include <godot_cpp/classes/convex_polygon_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

namespace {

/// PCG-style generator (high bits), deterministic per seed.
struct CrystalRNG {
	uint64_t state;
	explicit CrystalRNG(uint64_t p_seed) : state(p_seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
	uint32_t next() {
		uint64_t old = state;
		state = old * 6364136223846793005ULL + 1442695040888963407ULL;
		uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
		uint32_t rot = (uint32_t)(old >> 59u);
		return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
	}
	float unit() { return (next() >> 8) * (1.0f / 16777216.0f); }
	float
	range(float p_min,
		  float p_max) {
		return p_min + (p_max - p_min) * unit();
	}
};

/// Frame at p_origin whose Y axis is tilted `p_lean_rad` away from vertical towards p_dir_xz, yawed by p_yaw.
Transform3D crystal_frame(
		const Vector3 &p_origin,
		const Vector2 &p_dir_xz,
		float p_lean_rad,
		float p_yaw
) {
	Vector3 axis(0, 1, 0);
	Vector3 lean_dir(p_dir_xz.x, 0, p_dir_xz.y);
	if (lean_dir.length_squared() > 1e-8f && p_lean_rad > 1e-4f) {
		lean_dir.normalize();
		axis = (Vector3(0, 1, 0) * Math::cos(p_lean_rad) + lean_dir * Math::sin(p_lean_rad)).normalized();
	}
	Vector3 x = Vector3(0, 0, 1).cross(axis);
	if (x.length_squared() < 1e-6f) {
		x = Vector3(1, 0, 0);
	}
	x.normalize();
	Vector3 z = axis.cross(x).normalized();
	Basis b(x, axis, z);
	b = b * Basis(Vector3(0, 1, 0), p_yaw);
	return Transform3D(b, p_origin);
}

} // namespace

// clang-format off
#define CC_BIND(m_variant, m_name, m_hint, m_hint_str)                                                                 \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &CrystalClusterMesh::set_##m_name);                        \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &CrystalClusterMesh::get_##m_name);                                 \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_str), "set_" #m_name, "get_" #m_name)

void CrystalClusterMesh::_bind_methods() {
	ADD_GROUP("Main Crystal", "");
	CC_BIND(INT, seed, PROPERTY_HINT_NONE, "");
	CC_BIND(FLOAT, height, PROPERTY_HINT_RANGE, "0.05,50,0.05,suffix:m");
	CC_BIND(FLOAT, radius, PROPERTY_HINT_RANGE, "0.01,10,0.01,suffix:m");
	CC_BIND(INT, sides, PROPERTY_HINT_RANGE, "3,16,1");
	CC_BIND(FLOAT, taper, PROPERTY_HINT_RANGE, "0.1,1.5,0.01");
	CC_BIND(FLOAT, tip_ratio, PROPERTY_HINT_RANGE, "0,0.9,0.01");
	CC_BIND(FLOAT, sink, PROPERTY_HINT_RANGE, "0,0.9,0.01");
	ADD_GROUP("Cluster", "");
	CC_BIND(INT, crystal_count, PROPERTY_HINT_RANGE, "0,32,1");
	CC_BIND(INT, small_count, PROPERTY_HINT_RANGE, "0,64,1");
	CC_BIND(INT, pebble_count, PROPERTY_HINT_RANGE, "0,128,1");
	CC_BIND(FLOAT, spread, PROPERTY_HINT_RANGE, "0,3,0.01");
	CC_BIND(FLOAT, lean, PROPERTY_HINT_RANGE, "0,80,0.5,suffix:°");
	CC_BIND(FLOAT, height_variation, PROPERTY_HINT_RANGE, "0,1,0.01");
	ADD_GROUP("Look", "");
	CC_BIND(BOOL, faceted, PROPERTY_HINT_NONE, "");
	CC_BIND(COLOR, base_color, PROPERTY_HINT_NONE, "");
	CC_BIND(COLOR, tip_color, PROPERTY_HINT_NONE, "");
	CC_BIND(FLOAT, color_variation, PROPERTY_HINT_RANGE, "0,1,0.01");
	CC_BIND(FLOAT, gradient_power, PROPERTY_HINT_RANGE, "0.2,5,0.05");
	ClassDB::bind_method(D_METHOD("rebuild"), &CrystalClusterMesh::rebuild);
	ClassDB::bind_method(D_METHOD("create_collision_shape"), &CrystalClusterMesh::create_collision_shape);
}
#undef CC_BIND
// clang-format on

CrystalClusterMesh::CrystalClusterMesh() { _queue_rebuild(); }
CrystalClusterMesh::~CrystalClusterMesh() {}

void CrystalClusterMesh::_queue_rebuild() {
	if (!_rebuild_queued) {
		_rebuild_queued = true;
		call_deferred("rebuild");
	}
}

// ---------------------------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------------------------

/**
 * @brief Main crystal upright at the centre; the ring leans outward with jittered spacing, heights
 * 0.45..0.75 of the main; shards 0.2..0.4 between ring members, leaning more; pebbles are squat wide
 * prisms (taper ~1, big tip) scattered just outside the ring.
 */
void CrystalClusterMesh::_layout(std::vector<Crystal> &r_crystals) const {
	CrystalRNG rng((uint64_t)(uint32_t)seed);
	r_crystals.clear();

	Crystal main;
	main.frame = crystal_frame(Vector3(), Vector2(), 0.0f, rng.range(0.0f, (float)Math::TAU));
	main.height = height;
	main.radius = radius;
	main.taper = taper;
	main.tip = tip_ratio;
	main.tint = 0.0f;
	r_crystals.push_back(main);

	const float ring = height * spread;
	const float lean_rad = Math::deg_to_rad(lean);
	for (int i = 0; i < crystal_count; ++i) {
		const float a = ((float)Math::TAU * i) / MAX(1, crystal_count) + rng.range(-0.25f, 0.25f);
		const Vector2 dir(Math::cos(a), Math::sin(a));
		const float d = ring * rng.range(0.7f, 1.1f);
		const float hv = 1.0f + height_variation * rng.range(-1.0f, 1.0f);
		Crystal c;
		c.frame = crystal_frame(
				Vector3(dir.x * d, 0.0f, dir.y * d), dir, lean_rad * rng.range(0.6f, 1.4f),
				rng.range(0.0f, (float)Math::TAU)
		);
		c.height = height * 0.6f * hv;
		c.radius = radius * rng.range(0.6f, 0.9f);
		c.taper = taper * rng.range(0.9f, 1.05f);
		c.tip = tip_ratio * rng.range(0.8f, 1.3f);
		c.tint = rng.range(-1.0f, 1.0f);
		r_crystals.push_back(c);
	}
	for (int i = 0; i < small_count; ++i) {
		const float a = rng.range(0.0f, (float)Math::TAU);
		const Vector2 dir(Math::cos(a), Math::sin(a));
		const float d = ring * rng.range(0.6f, 1.4f);
		Crystal c;
		c.frame = crystal_frame(
				Vector3(dir.x * d, 0.0f, dir.y * d), dir, lean_rad * rng.range(1.2f, 2.2f),
				rng.range(0.0f, (float)Math::TAU)
		);
		c.height = height * rng.range(0.2f, 0.4f);
		c.radius = radius * rng.range(0.35f, 0.6f);
		c.taper = taper;
		c.tip = tip_ratio * rng.range(1.0f, 1.6f);
		c.tint = rng.range(-1.0f, 1.0f);
		r_crystals.push_back(c);
	}
	for (int i = 0; i < pebble_count; ++i) {
		const float a = rng.range(0.0f, (float)Math::TAU);
		const Vector2 dir(Math::cos(a), Math::sin(a));
		const float d = ring * rng.range(0.9f, 1.7f);
		Crystal c;
		c.frame = crystal_frame(
				Vector3(dir.x * d, 0.0f, dir.y * d), dir, Math::deg_to_rad(rng.range(0.0f, 45.0f)),
				rng.range(0.0f, (float)Math::TAU)
		);
		c.height = height * rng.range(0.05f, 0.12f);
		c.radius = radius * rng.range(0.25f, 0.5f);
		c.taper = 1.0f;
		c.tip = 0.6f;
		c.tint = rng.range(-1.0f, 1.0f);
		r_crystals.push_back(c);
	}
}

// ---------------------------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------------------------

/**
 * @brief One n-sided prism: base ring (buried `sink` below the frame's origin), top ring at
 * height·(1−tip), apex at height; bottom cap. Faceted: every face gets its own vertices and flat
 * normal. Smooth: shared ring vertices with radial normals. Colour and UV.y follow the height fraction.
 */
void CrystalClusterMesh::_append_crystal(
		const Crystal &p_c,
		PackedVector3Array &r_vertices,
		PackedVector3Array &r_normals,
		PackedVector2Array &r_uvs,
		PackedColorArray &r_colors,
		PackedInt32Array &r_indices,
		std::vector<Vector3> *r_points
) const {
	const int n = sides;
	const float y0 = -p_c.height * sink;
	const float y1 = p_c.height * (1.0f - p_c.tip);
	const float y2 = p_c.height;
	const float r0 = p_c.radius;
	const float r1 = p_c.radius * p_c.taper;

	auto colour_at = [&](float p_y) -> Color {
		float t = CLAMP((p_y - y0) / (y2 - y0), 0.0f, 1.0f);
		t = Math::pow(t, gradient_power);
		Color c = base_color.lerp(tip_color, t);
		const float v = 1.0f + p_c.tint * color_variation;
		return Color(CLAMP(c.r * v, 0.0f, 1.0f), CLAMP(c.g * v, 0.0f, 1.0f), CLAMP(c.b * v, 0.0f, 1.0f), 1.0f);
	};
	auto uv_at = [&](float p_angle_frac, float p_y) -> Vector2 {
		return Vector2(p_angle_frac, CLAMP((p_y - y0) / (y2 - y0), 0.0f, 1.0f));
	};

	std::vector<Vector3> ring0(n), ring1(n);
	for (int i = 0; i < n; ++i) {
		const float a = ((float)Math::TAU * i) / n;
		ring0[i] = Vector3(Math::cos(a) * r0, y0, Math::sin(a) * r0);
		ring1[i] = Vector3(Math::cos(a) * r1, y1, Math::sin(a) * r1);
	}
	const Vector3 apex(0, y2, 0);
	const Vector3 bottom(0, y0, 0);

	auto emit = [&](const Vector3 &p_local, const Vector3 &p_normal, const Vector2 &p_uv) -> int {
		const int idx = r_vertices.size();
		r_vertices.push_back(p_c.frame.xform(p_local));
		r_normals.push_back(p_c.frame.basis.xform(p_normal).normalized());
		r_uvs.push_back(p_uv);
		r_colors.push_back(colour_at(p_local.y));
		if (r_points) {
			r_points->push_back(p_c.frame.xform(p_local));
		}
		return idx;
	};
	// Godot front faces wind clockwise seen from outside: the geometric cross product of a front face
	// points against its normal. Order each triangle so it does, whatever the input order.
	auto tri = [&](int a, int b, int c) {
		const Vector3 geo = (r_vertices[b] - r_vertices[a]).cross(r_vertices[c] - r_vertices[a]);
		const Vector3 want = r_normals[a] + r_normals[b] + r_normals[c];
		if (geo.dot(want) > 0.0f) {
			std::swap(b, c);
		}
		r_indices.push_back(a);
		r_indices.push_back(b);
		r_indices.push_back(c);
	};

	if (faceted) {
		for (int i = 0; i < n; ++i) {
			const int j = (i + 1) % n;
			const float fa = (float)i / n, fb = (float)(i + 1) / n;
			// Side quad (outward normal from the two edges).
			Vector3 nrm = (ring0[j] - ring0[i]).cross(ring1[i] - ring0[i]).normalized();
			if (nrm.dot(ring0[i] + ring0[j]) < 0.0f) {
				nrm = -nrm;
			}
			const int a = emit(ring0[i], nrm, uv_at(fa, y0));
			const int b = emit(ring0[j], nrm, uv_at(fb, y0));
			const int c = emit(ring1[j], nrm, uv_at(fb, y1));
			const int d = emit(ring1[i], nrm, uv_at(fa, y1));
			tri(a, c, b);
			tri(a, d, c);
			// Tip facet.
			Vector3 tn = (ring1[j] - ring1[i]).cross(apex - ring1[i]).normalized();
			if (tn.dot(ring1[i] + ring1[j]) < 0.0f) {
				tn = -tn;
			}
			const int e = emit(ring1[i], tn, uv_at(fa, y1));
			const int f = emit(ring1[j], tn, uv_at(fb, y1));
			const int g = emit(apex, tn, uv_at((fa + fb) * 0.5f, y2));
			tri(e, g, f);
			// Bottom cap.
			const Vector3 dn(0, -1, 0);
			const int h = emit(ring0[i], dn, uv_at(fa, y0));
			const int k = emit(ring0[j], dn, uv_at(fb, y0));
			const int m = emit(bottom, dn, uv_at(0.5f, y0));
			tri(h, k, m);
		}
	} else {
		std::vector<int> i0(n), i1(n);
		for (int i = 0; i < n; ++i) {
			const float fa = (float)i / n;
			const Vector3 radial = Vector3(ring0[i].x, 0, ring0[i].z).normalized();
			const Vector3 side_n = (radial + Vector3(0, (r0 - r1) / MAX(1e-4f, y1 - y0), 0)).normalized();
			i0[i] = emit(ring0[i], side_n, uv_at(fa, y0));
			i1[i] = emit(ring1[i], side_n, uv_at(fa, y1));
		}
		const int top = emit(apex, Vector3(0, 1, 0), uv_at(0.5f, y2));
		const int bot = emit(bottom, Vector3(0, -1, 0), uv_at(0.5f, y0));
		for (int i = 0; i < n; ++i) {
			const int j = (i + 1) % n;
			tri(i0[i], i1[j], i0[j]);
			tri(i0[i], i1[i], i1[j]);
			tri(i1[i], top, i1[j]);
			tri(i0[i], i0[j], bot);
		}
	}
}

void CrystalClusterMesh::rebuild() {
	_rebuild_queued = false;
	clear_surfaces();
	_main_points.clear();

	std::vector<Crystal> crystals;
	_layout(crystals);

	PackedVector3Array vertices, normals;
	PackedVector2Array uvs;
	PackedColorArray colors;
	PackedInt32Array indices;
	for (size_t i = 0; i < crystals.size(); ++i) {
		_append_crystal(crystals[i], vertices, normals, uvs, colors, indices, i == 0 ? &_main_points : nullptr);
	}
	if (vertices.size() >= 3) {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = vertices;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		arrays[Mesh::ARRAY_TEX_UV] = uvs;
		arrays[Mesh::ARRAY_COLOR] = colors;
		arrays[Mesh::ARRAY_INDEX] = indices;
		add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	}
	emit_changed();
}

Ref<Shape3D> CrystalClusterMesh::create_collision_shape() const {
	if (_main_points.size() < 4) {
		return Ref<Shape3D>();
	}
	PackedVector3Array pts;
	for (const Vector3 &p : _main_points) {
		pts.push_back(p);
	}
	Ref<ConvexPolygonShape3D> shape;
	shape.instantiate();
	shape->set_points(pts);
	return shape;
}

} // namespace godot
