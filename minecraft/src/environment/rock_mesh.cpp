/**
 * @file rock_mesh.cpp
 * @brief RockMesh: cluster layout, noise displacement, facets, colouring.
 */
#include "rock_mesh.h"
#include "prop_geometry.h"
#include <algorithm>
#include <godot_cpp/classes/convex_polygon_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

// clang-format off
#define RK_BIND(m_variant, m_name, m_hint, m_hint_str)                                                                 \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &RockMesh::set_##m_name);                                  \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &RockMesh::get_##m_name);                                           \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_str), "set_" #m_name, "get_" #m_name)

void RockMesh::_bind_methods() {
	ADD_GROUP("Rock", "");
	RK_BIND(INT, seed, PROPERTY_HINT_NONE, "");
	RK_BIND(VECTOR3, size, PROPERTY_HINT_NONE, "suffix:m");
	RK_BIND(INT, detail, PROPERTY_HINT_RANGE, "0,3,1");
	ADD_GROUP("Shape", "");
	ClassDB::bind_method(D_METHOD("set_base_shape", "value"), &RockMesh::set_base_shape);
	ClassDB::bind_method(D_METHOD("get_base_shape"), &RockMesh::get_base_shape);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "base_shape", PROPERTY_HINT_ENUM, "Sphere,Cube"), "set_base_shape", "get_base_shape");
	RK_BIND(FLOAT, roundness, PROPERTY_HINT_RANGE, "0,1,0.01");
	RK_BIND(VECTOR2, shear, PROPERTY_HINT_NONE, "");
	RK_BIND(FLOAT, tilt, PROPERTY_HINT_RANGE, "0,60,0.5,suffix:°");
	RK_BIND(FLOAT, noise_amplitude, PROPERTY_HINT_RANGE, "0,1,0.01");
	RK_BIND(FLOAT, noise_frequency, PROPERTY_HINT_RANGE, "0.1,10,0.05");
	RK_BIND(INT, noise_octaves, PROPERTY_HINT_RANGE, "1,6,1");
	RK_BIND(BOOL, ridged, PROPERTY_HINT_NONE, "");
	RK_BIND(FLOAT, facet_snap, PROPERTY_HINT_RANGE, "0,1,0.01");
	RK_BIND(INT, facet_count, PROPERTY_HINT_RANGE, "3,40,1");
	RK_BIND(FLOAT, flatten_bottom, PROPERTY_HINT_RANGE, "0,0.9,0.01");
	RK_BIND(FLOAT, sink, PROPERTY_HINT_RANGE, "-2,5,0.01,suffix:m");
	RK_BIND(BOOL, flat_shaded, PROPERTY_HINT_NONE, "");
	ADD_GROUP("Cluster", "");
	ClassDB::bind_method(D_METHOD("set_arrangement", "value"), &RockMesh::set_arrangement);
	ClassDB::bind_method(D_METHOD("get_arrangement"), &RockMesh::get_arrangement);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "arrangement", PROPERTY_HINT_ENUM, "Single,Pile,Outcrop,Stack"), "set_arrangement", "get_arrangement");
	RK_BIND(INT, blob_count, PROPERTY_HINT_RANGE, "1,24,1");
	RK_BIND(FLOAT, spread, PROPERTY_HINT_RANGE, "0,100,0.1,suffix:m");
	RK_BIND(FLOAT, blob_size_variation, PROPERTY_HINT_RANGE, "0,1,0.01");
	ADD_GROUP("Colour", "");
	RK_BIND(COLOR, base_color, PROPERTY_HINT_NONE, "");
	RK_BIND(COLOR, top_color, PROPERTY_HINT_NONE, "");
	RK_BIND(COLOR, crevice_color, PROPERTY_HINT_NONE, "");
	RK_BIND(FLOAT, crevice_strength, PROPERTY_HINT_RANGE, "0,3,0.05");
	RK_BIND(INT, strata, PROPERTY_HINT_RANGE, "0,32,1");
	RK_BIND(FLOAT, strata_strength, PROPERTY_HINT_RANGE, "0,1,0.01");
	RK_BIND(COLOR, moss_color, PROPERTY_HINT_NONE, "");
	RK_BIND(FLOAT, moss_amount, PROPERTY_HINT_RANGE, "0,1,0.01");
	RK_BIND(FLOAT, color_variation, PROPERTY_HINT_RANGE, "0,1,0.01");
	ClassDB::bind_method(D_METHOD("rebuild"), &RockMesh::rebuild);
	ClassDB::bind_method(D_METHOD("get_collision_shape_count"), &RockMesh::get_collision_shape_count);
	ClassDB::bind_method(D_METHOD("create_collision_shape", "index"), &RockMesh::create_collision_shape);
	BIND_ENUM_CONSTANT(ARRANGE_SINGLE);
	BIND_ENUM_CONSTANT(ARRANGE_PILE);
	BIND_ENUM_CONSTANT(ARRANGE_OUTCROP);
	BIND_ENUM_CONSTANT(ARRANGE_STACK);
	BIND_ENUM_CONSTANT(BASE_SPHERE);
	BIND_ENUM_CONSTANT(BASE_CUBE);
}
#undef RK_BIND
// clang-format on

RockMesh::RockMesh() { _queue_rebuild(); }
RockMesh::~RockMesh() {}

void RockMesh::_queue_rebuild() {
	if (!_rebuild_queued) {
		_rebuild_queued = true;
		call_deferred("rebuild");
	}
}

// ---------------------------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------------------------

/**
 * @brief SINGLE: one blob. PILE: the main blob at the centre, the rest around it at ground level and
 * a few smaller ones perched on top, all overlapping. OUTCROP: blobs along X over `spread` metres,
 * the middle ones largest, all cut at the same ground plane — a rocky ridge / cliff-foot rubble line.
 */
void RockMesh::_layout(std::vector<Blob> &r_blobs) const {
	prop::RNG rng((uint64_t)(uint32_t)seed);
	r_blobs.clear();
	auto make = [&](const Vector3 &c, const Vector3 &half, float yaw) {
		Blob b;
		b.centre = c;
		b.half = half;
		b.yaw = yaw;
		b.tint = rng.range(-1.0f, 1.0f);
		b.noise_seed = rng.next();
		const float a = rng.range(0.0f, (float)Math::TAU);
		b.tilt_axis = Vector3(Math::cos(a), 0, Math::sin(a));
		b.tilt_rad = Math::deg_to_rad(tilt) * rng.range(0.3f, 1.0f);
		r_blobs.push_back(b);
	};
	const int n = arrangement == ARRANGE_SINGLE ? 1 : MAX(1, blob_count);

	if (arrangement == ARRANGE_STACK && n > 1) {
		// Slabs about 40 % as tall as wide, each resting on the one below with a lateral shift and a
		// twist, shrinking slowly upward: a jointed rock stack / cairn.
		float y = 0.0f;
		for (int i = 0; i < n; ++i) {
			const float s = CLAMP(1.0f - 0.08f * i + blob_size_variation * 0.3f * rng.range(-1.0f, 1.0f), 0.3f, 1.2f);
			const Vector3 half(size.x * s, size.y * 0.4f * rng.range(0.8f, 1.2f), size.z * s);
			const Vector3 c(size.x * 0.18f * rng.range(-1.0f, 1.0f), y, size.z * 0.18f * rng.range(-1.0f, 1.0f));
			make(c, half, rng.range(-0.5f, 0.5f) + (float)Math::PI * 0.5f * (i % 2));
			y += half.y * (1.0f - flatten_bottom) * 1.85f; // Next slab's cut bottom sits near this one's top
		}
		return;
	}
	if (arrangement == ARRANGE_OUTCROP && n > 1) {
		for (int i = 0; i < n; ++i) {
			const float f = (i + 0.5f) / n;
			const float mid = 1.0f - Math::abs(f - 0.5f) * 1.2f; // Bigger in the middle
			const float s = CLAMP(mid * (1.0f + blob_size_variation * rng.range(-1.0f, 1.0f)), 0.3f, 1.2f);
			const Vector3 c(
					(f - 0.5f) * spread + rng.range(-0.15f, 0.15f) * size.x, 0.0f, rng.range(-0.35f, 0.35f) * size.z
			);
			make(c, size * s, rng.range(0.0f, (float)Math::TAU));
		}
		return;
	}
	make(Vector3(), size, rng.range(0.0f, (float)Math::TAU));
	for (int i = 1; i < n; ++i) {
		const bool on_top = i >= (n + 1) / 2;
		const float s =
				CLAMP((on_top ? 0.45f : 0.65f) * (1.0f + blob_size_variation * rng.range(-1.0f, 1.0f)), 0.2f, 0.95f);
		const float a = rng.range(0.0f, (float)Math::TAU);
		const float d = on_top ? rng.range(0.15f, 0.45f) : rng.range(0.7f, 1.0f);
		const Vector3 c(
				Math::cos(a) * size.x * d, on_top ? size.y * rng.range(0.55f, 0.85f) : 0.0f, Math::sin(a) * size.z * d
		);
		make(c, size * s, rng.range(0.0f, (float)Math::TAU));
	}
}

// ---------------------------------------------------------------------------------------------
// One blob
// ---------------------------------------------------------------------------------------------

void RockMesh::_append_blob(
		const Blob &p_blob,
		PackedVector3Array &r_vertices,
		PackedVector3Array &r_normals,
		PackedVector2Array &r_uvs,
		PackedColorArray &r_colors,
		PackedInt32Array &r_indices,
		PackedVector3Array &r_points
) const {
	prop::IcoSphere ico(detail);
	const size_t nv = ico.verts.size();
	const Basis yaw(Vector3(0, 1, 0), p_blob.yaw);

	// 1. Base shape (sphere, or the sphere projected onto a cube and blended back by `roundness`),
	//    displaced along its direction by fractal noise, scaled to the half extents, sheared, tilted.
	std::vector<Vector3> pos(nv);
	std::vector<float> radius(nv);
	const Basis tilt_basis = p_blob.tilt_rad > 1e-4f ? Basis(p_blob.tilt_axis, p_blob.tilt_rad) : Basis();
	for (size_t i = 0; i < nv; ++i) {
		const Vector3 &v = ico.verts[i];
		Vector3 base = v;
		if (base_shape == BASE_CUBE) {
			const float m = MAX(Math::abs(v.x), MAX(Math::abs(v.y), Math::abs(v.z)));
			base = (v / MAX(1e-4f, m)).lerp(v, roundness); // On the unit cube, softened towards the sphere
		}
		const float n = prop::fbm(v * noise_frequency, noise_octaves, 2.1f, 0.5f, p_blob.noise_seed, ridged);
		radius[i] = 1.0f + noise_amplitude * n;
		Vector3 p = Vector3(base.x * p_blob.half.x, base.y * p_blob.half.y, base.z * p_blob.half.z) * radius[i];
		p.x += shear.x * p.y;
		p.z += shear.y * p.y;
		pos[i] = tilt_basis.xform(p);
	}

	// 2. Facets: project each vertex part-way onto the plane of its nearest facet direction.
	if (facet_snap > 0.0f && facet_count > 0) {
		prop::RNG frng((uint64_t)p_blob.noise_seed ^ 0x9E3779B97F4A7C15ULL);
		std::vector<Vector3> dirs(facet_count);
		for (Vector3 &d : dirs) {
			d = frng.dir();
		}
		std::vector<int> group(nv);
		std::vector<float> plane_d(facet_count, 0.0f);
		std::vector<int> count(facet_count, 0);
		for (size_t i = 0; i < nv; ++i) {
			int best = 0;
			float best_dot = -2.0f;
			for (int k = 0; k < facet_count; ++k) {
				const float dt = ico.verts[i].dot(dirs[k]);
				if (dt > best_dot) {
					best_dot = dt;
					best = k;
				}
			}
			group[i] = best;
			plane_d[best] += pos[i].dot(dirs[best]);
			count[best]++;
		}
		for (int k = 0; k < facet_count; ++k) {
			if (count[k] > 0) {
				plane_d[k] /= count[k];
			}
		}
		for (size_t i = 0; i < nv; ++i) {
			const int k = group[i];
			const float d = pos[i].dot(dirs[k]);
			pos[i] -= dirs[k] * (d - plane_d[k]) * facet_snap;
		}
	}

	// 3. Flat bottom: everything below the cut plane is clamped to it; the plane sits at -sink.
	const float cut = -p_blob.half.y * (1.0f - flatten_bottom);
	float ymin = 1e9f, ymax = -1e9f;
	for (size_t i = 0; i < nv; ++i) {
		pos[i].y = MAX(pos[i].y, cut);
		pos[i].y -= cut + sink;
		ymin = MIN(ymin, pos[i].y);
		ymax = MAX(ymax, pos[i].y);
	}

	// 4. Concavity per vertex: how far it sits below the mean of its neighbours (positive = dent).
	std::vector<float> conc(nv, 0.0f);
	{
		std::vector<float> sum(nv, 0.0f);
		std::vector<int> cnt(nv, 0);
		for (size_t t = 0; t < ico.tris.size(); t += 3) {
			for (int e = 0; e < 3; ++e) {
				const int a = ico.tris[t + e], b = ico.tris[t + (e + 1) % 3];
				sum[a] += radius[b];
				cnt[a]++;
				sum[b] += radius[a];
				cnt[b]++;
			}
		}
		for (size_t i = 0; i < nv; ++i) {
			conc[i] = cnt[i] ? (sum[i] / cnt[i] - radius[i]) : 0.0f;
		}
	}

	// 5. Colour.
	auto colour_at = [&](size_t i, const Vector3 &nrm) -> Color {
		const float t = CLAMP((pos[i].y - ymin) / MAX(0.01f, ymax - ymin), 0.0f, 1.0f);
		Color c = base_color.lerp(top_color, t);
		const float dent = CLAMP(conc[i] / MAX(0.02f, noise_amplitude) * 1.5f, 0.0f, 1.0f);
		c = c.lerp(crevice_color, dent * crevice_strength * 0.8f);
		if (strata > 0) {
			const float band = Math::fmod(t * strata, 1.0f);
			c = c.darkened(strata_strength * (band < 0.4f ? 1.0f : 0.0f));
		}
		if (moss_amount > 0.0f) {
			const float up = CLAMP((nrm.y - 0.35f) / 0.4f, 0.0f, 1.0f);
			c = c.lerp(moss_color, up * moss_amount);
		}
		const float v = 1.0f + p_blob.tint * color_variation;
		return Color(CLAMP(c.r * v, 0.f, 1.f), CLAMP(c.g * v, 0.f, 1.f), CLAMP(c.b * v, 0.f, 1.f));
	};
	auto uv_at = [&](size_t i) -> Vector2 {
		const Vector3 &v = ico.verts[i];
		return Vector2(
				Math::atan2(v.z, v.x) / (float)Math::TAU + 0.5f,
				CLAMP((pos[i].y - ymin) / MAX(0.01f, ymax - ymin), 0.0f, 1.0f)
		);
	};
	auto world = [&](size_t i) -> Vector3 { return p_blob.centre + yaw.xform(pos[i]); };

	// 6. Emit. Flat: per-face vertices + face normal. Smooth: accumulated vertex normals.
	for (size_t i = 0; i < nv; ++i) {
		r_points.push_back(world(i));
	}
	if (flat_shaded) {
		for (size_t t = 0; t < ico.tris.size(); t += 3) {
			const int ia = ico.tris[t], ib = ico.tris[t + 1], ic = ico.tris[t + 2];
			const Vector3 a = world(ia), b = world(ib), c = world(ic);
			Vector3 n = (b - a).cross(c - a);
			if (n.length_squared() < 1e-12f) {
				continue;
			}
			n.normalize();
			// Icosphere triangles are outward-wound (CCW from outside); Godot wants CW → swap b, c.
			const int base = r_vertices.size();
			const int order[3] = { ia, ic, ib };
			const Vector3 pts[3] = { a, c, b };
			for (int k = 0; k < 3; ++k) {
				r_vertices.push_back(pts[k]);
				r_normals.push_back(n);
				r_uvs.push_back(uv_at(order[k]));
				r_colors.push_back(colour_at(order[k], n));
				r_indices.push_back(base + k);
			}
		}
	} else {
		std::vector<Vector3> nrm(nv, Vector3());
		for (size_t t = 0; t < ico.tris.size(); t += 3) {
			const int ia = ico.tris[t], ib = ico.tris[t + 1], ic = ico.tris[t + 2];
			const Vector3 n = (world(ib) - world(ia)).cross(world(ic) - world(ia));
			nrm[ia] += n;
			nrm[ib] += n;
			nrm[ic] += n;
		}
		const int base = r_vertices.size();
		for (size_t i = 0; i < nv; ++i) {
			const Vector3 n = nrm[i].length_squared() > 1e-12f ? nrm[i].normalized() : Vector3(0, 1, 0);
			r_vertices.push_back(world(i));
			r_normals.push_back(n);
			r_uvs.push_back(uv_at(i));
			r_colors.push_back(colour_at(i, n));
		}
		for (size_t t = 0; t < ico.tris.size(); t += 3) {
			r_indices.push_back(base + ico.tris[t]);
			r_indices.push_back(base + ico.tris[t + 2]);
			r_indices.push_back(base + ico.tris[t + 1]);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------------------------

void RockMesh::rebuild() {
	_rebuild_queued = false;
	clear_surfaces();
	_blob_points.clear();

	std::vector<Blob> blobs;
	_layout(blobs);

	PackedVector3Array vertices, normals;
	PackedVector2Array uvs;
	PackedColorArray colors;
	PackedInt32Array indices;
	for (const Blob &blob : blobs) {
		PackedVector3Array pts;
		_append_blob(blob, vertices, normals, uvs, colors, indices, pts);
		_blob_points.push_back(pts);
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

Ref<Shape3D> RockMesh::create_collision_shape(int p_index) const {
	if (p_index < 0 || p_index >= (int)_blob_points.size() || _blob_points[p_index].size() < 4) {
		return Ref<Shape3D>();
	}
	Ref<ConvexPolygonShape3D> shape;
	shape.instantiate();
	shape->set_points(_blob_points[p_index]);
	return shape;
}

} // namespace godot
