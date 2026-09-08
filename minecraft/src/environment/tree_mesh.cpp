/**
 * @file tree_mesh.cpp
 * @brief FoliageTreeMesh: trunk with buttress roots, a canopy shell densely clad in leaf / blossom
 *        cards, and hanging cherries — all in one ArrayMesh.
 */
#include "tree_mesh.h"
#include "prop_geometry.h"
#include <algorithm>
#include <cstdint>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

using TreeRNG = prop::RNG;
using IcoSphere = prop::IcoSphere;

// clang-format off
#define FT_BIND(m_variant, m_name, m_hint, m_hint_str)                                                                 \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &FoliageTreeMesh::set_##m_name);                          \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &FoliageTreeMesh::get_##m_name);                                  \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_str), "set_" #m_name, "get_" #m_name)

void FoliageTreeMesh::_bind_methods() {
	ADD_GROUP("Tree", "");
	FT_BIND(INT, seed, PROPERTY_HINT_NONE, "");
	ClassDB::bind_method(D_METHOD("set_species", "value"), &FoliageTreeMesh::set_species);
	ClassDB::bind_method(D_METHOD("get_species"), &FoliageTreeMesh::get_species);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "species", PROPERTY_HINT_ENUM, "Deciduous,Pine"), "set_species", "get_species");
	ADD_GROUP("Trunk", "trunk_");
	FT_BIND(FLOAT, trunk_height, PROPERTY_HINT_RANGE, "0.05,20,0.05,suffix:m");
	FT_BIND(FLOAT, trunk_radius, PROPERTY_HINT_RANGE, "0.01,5,0.01,suffix:m");
	FT_BIND(FLOAT, trunk_flare, PROPERTY_HINT_RANGE, "1,4,0.05");
	FT_BIND(INT, trunk_sides, PROPERTY_HINT_RANGE, "3,20,1");
	FT_BIND(COLOR, trunk_color, PROPERTY_HINT_NONE, "");
	ADD_GROUP("Roots", "root_");
	FT_BIND(INT, root_count, PROPERTY_HINT_RANGE, "0,12,1");
	FT_BIND(FLOAT, root_spread, PROPERTY_HINT_RANGE, "0,2,0.01");
	ADD_GROUP("Canopy", "canopy_");
	FT_BIND(FLOAT, canopy_radius, PROPERTY_HINT_RANGE, "0.05,20,0.05,suffix:m");
	FT_BIND(FLOAT, canopy_height, PROPERTY_HINT_RANGE, "0.1,30,0.05,suffix:m");
	FT_BIND(FLOAT, canopy_flatten, PROPERTY_HINT_RANGE, "0.3,1.5,0.01");
	ClassDB::bind_method(D_METHOD("set_lobe_count", "value"), &FoliageTreeMesh::set_lobe_count);
	ClassDB::bind_method(D_METHOD("get_lobe_count"), &FoliageTreeMesh::get_lobe_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lobe_count", PROPERTY_HINT_RANGE, "1,12,1"), "set_lobe_count", "get_lobe_count");
	ClassDB::bind_method(D_METHOD("set_lobe_jitter", "value"), &FoliageTreeMesh::set_lobe_jitter);
	ClassDB::bind_method(D_METHOD("get_lobe_jitter"), &FoliageTreeMesh::get_lobe_jitter);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lobe_jitter", PROPERTY_HINT_RANGE, "0,1.5,0.01"), "set_lobe_jitter", "get_lobe_jitter");
	ClassDB::bind_method(D_METHOD("set_canopy_detail", "value"), &FoliageTreeMesh::set_canopy_detail);
	ClassDB::bind_method(D_METHOD("get_canopy_detail"), &FoliageTreeMesh::get_canopy_detail);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "canopy_detail", PROPERTY_HINT_RANGE, "0,2,1"), "set_canopy_detail", "get_canopy_detail");
	ADD_GROUP("Foliage", "");
	ClassDB::bind_method(D_METHOD("set_foliage_style", "value"), &FoliageTreeMesh::set_foliage_style);
	ClassDB::bind_method(D_METHOD("get_foliage_style"), &FoliageTreeMesh::get_foliage_style);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "foliage_style", PROPERTY_HINT_ENUM, "Leaves,Blossom"), "set_foliage_style", "get_foliage_style");
	FT_BIND(INT, leaf_count, PROPERTY_HINT_RANGE, "0,4000,10");
	FT_BIND(FLOAT, leaf_size, PROPERTY_HINT_RANGE, "0.02,3,0.01,suffix:m");
	FT_BIND(FLOAT, leaf_tilt, PROPERTY_HINT_RANGE, "0,90,1,suffix:°");
	FT_BIND(COLOR, canopy_bottom, PROPERTY_HINT_NONE, "");
	FT_BIND(COLOR, canopy_top, PROPERTY_HINT_NONE, "");
	FT_BIND(FLOAT, color_variation, PROPERTY_HINT_RANGE, "0,1,0.01");
	FT_BIND(COLOR, blossom_color, PROPERTY_HINT_NONE, "");
	FT_BIND(FLOAT, blossom_white_fraction, PROPERTY_HINT_RANGE, "0,1,0.01");
	ADD_GROUP("Cherries", "cherry_");
	FT_BIND(INT, cherry_count, PROPERTY_HINT_RANGE, "0,60,1");
	FT_BIND(FLOAT, cherry_radius, PROPERTY_HINT_RANGE, "0.01,1,0.01,suffix:m");
	FT_BIND(COLOR, cherry_color, PROPERTY_HINT_NONE, "");

	ClassDB::bind_method(D_METHOD("rebuild"), &FoliageTreeMesh::rebuild);
	ClassDB::bind_method(D_METHOD("create_collision_shape"), &FoliageTreeMesh::create_collision_shape);

	BIND_ENUM_CONSTANT(SPECIES_DECIDUOUS);
	BIND_ENUM_CONSTANT(SPECIES_PINE);
	BIND_ENUM_CONSTANT(FOLIAGE_LEAVES);
	BIND_ENUM_CONSTANT(FOLIAGE_BLOSSOM);
}
#undef FT_BIND
// clang-format on

FoliageTreeMesh::FoliageTreeMesh() { _queue_rebuild(); }
FoliageTreeMesh::~FoliageTreeMesh() {}

void FoliageTreeMesh::_queue_rebuild() {
	if (!_rebuild_queued) {
		_rebuild_queued = true;
		call_deferred("rebuild");
	}
}

void FoliageTreeMesh::rebuild() {
	_rebuild_queued = false;
	clear_surfaces();
	Builder b;
	_build(b);
	if (b.vertices.size() >= 3) {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = b.vertices;
		arrays[Mesh::ARRAY_NORMAL] = b.normals;
		arrays[Mesh::ARRAY_TEX_UV] = b.uvs;
		arrays[Mesh::ARRAY_COLOR] = b.colors;
		arrays[Mesh::ARRAY_INDEX] = b.indices;
		add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	}
	emit_changed();
}

Ref<Shape3D> FoliageTreeMesh::create_collision_shape() const {
	Ref<CylinderShape3D> shape;
	shape.instantiate();
	shape->set_height(trunk_height);
	shape->set_radius(trunk_radius * trunk_flare);
	return shape;
}

// ---------------------------------------------------------------------------------------------
// Canopy volume shared by the shell and the leaf scatter
// ---------------------------------------------------------------------------------------------

/// Lobe centres/radii (deciduous: main + jittered; pine: a shrinking stack) and the canopy y-range.
void FoliageTreeMesh::_canopy_lobes(
		std::vector<Lobe> &r_lobes,
		float &r_ymin,
		float &r_ymax
) const {
	TreeRNG rng((uint64_t)(uint32_t)seed);
	const float top = trunk_height;
	r_lobes.clear();
	if (species == SPECIES_PINE) {
		const int cones = 5;
		for (int i = 0; i < cones; ++i) {
			const float f = (float)i / cones;
			r_lobes.push_back({ Vector3(0, top + canopy_height * f, 0), canopy_radius * (1.0f - 0.72f * f) });
		}
	} else {
		// A cluster of similar-sized clumps spread over a shell (Fibonacci points + jitter), so the
		// canopy is a bumpy mass of overlapping spheres rather than one big ball (the AC look).
		const int n = MAX(1, lobe_count);
		const float lr = canopy_radius * 0.55f; // Clump radius
		const float spread = canopy_radius * 0.45f * (0.6f + lobe_jitter); // How far clumps wander from centre
		const Vector3 cc(0, top + canopy_height * 0.52f, 0);
		for (int i = 0; i < n; ++i) {
			const float y = 1.0f - (i + 0.5f) * 2.0f / n; // 1 .. -1 down the shell
			const float rad = Math::sqrt(MAX(0.0f, 1.0f - y * y));
			const float phi = i * 2.399963f; // Golden angle
			Vector3 d = Vector3(Math::cos(phi) * rad, y, Math::sin(phi) * rad) + rng.dir() * 0.28f;
			d.normalize();
			const float dist = i == 0 ? 0.0f : spread * rng.range(0.5f, 1.0f);
			const Vector3 off(d.x * dist, d.y * dist * canopy_flatten, d.z * dist);
			r_lobes.push_back({ cc + off, lr * rng.range(0.82f, 1.15f) });
		}
	}
	r_ymin = top;
	r_ymax = top;
	for (const Lobe &l : r_lobes) {
		r_ymin = MIN(r_ymin, l.c.y - l.r * canopy_flatten);
		r_ymax = MAX(r_ymax, l.c.y + l.r * canopy_flatten);
	}
}

// ---------------------------------------------------------------------------------------------
// Solid mesh: trunk (with buttress roots) + dark inner canopy shell
// ---------------------------------------------------------------------------------------------

/**
 * @brief The occluding geometry only — the leaf/fruit cards are instanced by the node. The trunk is a
 * tapered cylinder whose base ring bulges at `root_count` roots (buttress flare); the canopy shell is
 * the lobes as flattened icospheres in a dark shade so gaps between leaves read as shadow, not sky.
 */
void FoliageTreeMesh::_build(Builder &b) const {
	const float total_h = trunk_height + canopy_height;
	auto add_v = [&](const Vector3 &p, const Vector3 &n, const Color &c) -> int {
		int i = b.vertices.size();
		b.vertices.push_back(p);
		b.normals.push_back(n.normalized());
		b.uvs.push_back(Vector2(0.5f, CLAMP(p.y / MAX(0.01f, total_h), 0.0f, 1.0f)));
		b.colors.push_back(c);
		return i;
	};
	auto add_tri = [&](int i, int j, int k, const Vector3 &target) {
		Vector3 geo = (b.vertices[j] - b.vertices[i]).cross(b.vertices[k] - b.vertices[i]);
		if (geo.dot(target) > 0.0f) {
			std::swap(j, k);
		}
		b.indices.push_back(i);
		b.indices.push_back(j);
		b.indices.push_back(k);
	};

	// ---- Trunk: flare ring (ground, wavy for roots) -> base ring -> top ring ----
	{
		const int n = MAX(trunk_sides, root_count * 2);
		const float r0 = trunk_radius * trunk_flare, r1 = trunk_radius;
		const float base_y = trunk_height * 0.16f;
		const float root_phase = 0.0f;
		std::vector<int> flare(n), lo(n), hi(n);
		const Color base_col(trunk_color.r * 0.8f, trunk_color.g * 0.8f, trunk_color.b * 0.8f);
		for (int i = 0; i < n; ++i) {
			const float a = (float)Math::TAU * i / n;
			const Vector3 rad(Math::cos(a), 0, Math::sin(a));
			float bulge = 0.0f;
			if (root_count > 0 && root_spread > 0.0f) {
				const float c = Math::cos(root_count * (a - root_phase));
				bulge = root_spread * 0.6f * MAX(0.0f, c) * MAX(0.0f, c);
			}
			flare[i] = add_v(rad * (r0 * (1.0f + bulge)), (rad + Vector3(0, 0.4f, 0)).normalized(), base_col);
			lo[i] = add_v(Vector3(rad.x * r0, base_y, rad.z * r0), rad, base_col);
			hi[i] = add_v(Vector3(rad.x * r1, trunk_height, rad.z * r1), rad, trunk_color);
		}
		for (int i = 0; i < n; ++i) {
			const int j = (i + 1) % n;
			const Vector3 out = b.vertices[lo[i]] + b.vertices[lo[j]] + Vector3(0, 0.01f, 0);
			add_tri(flare[i], flare[j], lo[j], out);
			add_tri(flare[i], lo[j], lo[i], out);
			add_tri(lo[i], lo[j], hi[j], out);
			add_tri(lo[i], hi[j], hi[i], out);
		}
	}

	// ---- Canopy shell: flattened lobes, dark shade ----
	std::vector<Lobe> lobes;
	float ymin, ymax;
	_canopy_lobes(lobes, ymin, ymax);
	// Pine has no leaf cards, so its shell IS the tree (full gradient); a deciduous shell only shows
	// through leaf gaps, so it is a uniform dark foliage tone that reads as inner shadow, not grey sky.
	const bool pine = species == SPECIES_PINE;
	const Color decid_shell(canopy_bottom.r * 0.85f, canopy_bottom.g * 0.85f, canopy_bottom.b * 0.85f);
	IcoSphere ico(canopy_detail);
	for (const Lobe &l : lobes) {
		const int base = b.vertices.size();
		for (const Vector3 &v : ico.verts) {
			const Vector3 p = l.c + Vector3(v.x * l.r, v.y * l.r * canopy_flatten, v.z * l.r) * 0.92f;
			Color col = decid_shell;
			if (pine) {
				float t = CLAMP((p.y - ymin) / MAX(0.01f, ymax - ymin), 0.0f, 1.0f);
				t = t * t * (3.0f - 2.0f * t);
				col = canopy_bottom.lerp(canopy_top, t);
			}
			add_v(p, Vector3(v.x, v.y / canopy_flatten, v.z), col);
		}
		for (size_t t = 0; t < ico.tris.size(); t += 3) {
			add_tri(base + ico.tris[t], base + ico.tris[t + 1], base + ico.tris[t + 2],
					b.vertices[base + ico.tris[t]] - l.c);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Instance scatter (read by FoliageTree into MultiMeshes)
// ---------------------------------------------------------------------------------------------

void FoliageTreeMesh::compute_leaves(Scatter &r_out) const {
	r_out.xforms.clear();
	r_out.colors.clear();
	if (leaf_count <= 0) {
		return;
	}
	TreeRNG rng((uint64_t)(uint32_t)(seed * 131 + 7)); // A different stream from the layout
	std::vector<Lobe> lobes;
	float ymin, ymax;
	_canopy_lobes(lobes, ymin, ymax);
	if (lobes.empty()) {
		return;
	}
	// Pick lobes in proportion to surface area (r^2).
	std::vector<float> cdf(lobes.size());
	float sum = 0.0f;
	for (size_t i = 0; i < lobes.size(); ++i) {
		sum += lobes[i].r * lobes[i].r;
		cdf[i] = sum;
	}
	const float k = Math::tan(Math::deg_to_rad(leaf_tilt));
	for (int i = 0; i < leaf_count; ++i) {
		const float pick = rng.range(0.0f, sum);
		size_t li = 0;
		while (li + 1 < lobes.size() && cdf[li] < pick) {
			++li;
		}
		const Lobe &l = lobes[li];
		const Vector3 d = rng.dir();
		const Vector3 p = l.c + Vector3(d.x * l.r, d.y * l.r * canopy_flatten, d.z * l.r);
		const Vector3 nrm = Vector3(d.x, d.y / canopy_flatten, d.z).normalized();
		Vector3 face = (nrm + rng.dir() * k).normalized();
		Vector3 x = Vector3(0, 1, 0).cross(face);
		if (x.length_squared() < 1e-4f) {
			x = Vector3(1, 0, 0);
		}
		x.normalize();
		Vector3 y = face.cross(x).normalized();
		const float roll = rng.range(0.0f, (float)Math::TAU);
		const float cs = Math::cos(roll), sn = Math::sin(roll);
		const Vector3 rx = x * cs + y * sn;
		const Vector3 ry = -x * sn + y * cs;
		const float sc = leaf_size * rng.range(0.8f, 1.2f);
		Basis basis(rx * sc, ry * sc, face * sc);
		r_out.xforms.push_back(Transform3D(basis, p + face * leaf_size * 0.15f));

		float t = CLAMP((p.y - ymin) / MAX(0.01f, ymax - ymin), 0.0f, 1.0f);
		t = t * t * (3.0f - 2.0f * t);
		Color c = canopy_bottom.lerp(canopy_top, t);
		if (foliage_style == FOLIAGE_BLOSSOM && rng.unit() < blossom_white_fraction) {
			c = blossom_color;
		}
		const float v = 1.0f + rng.range(-color_variation, color_variation);
		r_out.colors.push_back(Color(CLAMP(c.r * v, 0.f, 1.f), CLAMP(c.g * v, 0.f, 1.f), CLAMP(c.b * v, 0.f, 1.f)));
	}
}

void FoliageTreeMesh::compute_fruit(Scatter &r_out) const {
	r_out.xforms.clear();
	r_out.colors.clear();
	if (cherry_count <= 0) {
		return;
	}
	TreeRNG rng((uint64_t)(uint32_t)(seed * 977 + 13));
	const float top = trunk_height;
	for (int i = 0; i < cherry_count; ++i) {
		const float a = rng.range(0, (float)Math::TAU);
		const Vector3 out(Math::cos(a), 0, Math::sin(a));
		// Hang just under the canopy's lower edge so the fruit dangles clear of the leaf mass.
		const Vector3 p = out * canopy_radius * rng.range(0.68f, 0.9f) +
				Vector3(0, top - cherry_radius * 2.0f + canopy_height * rng.range(-0.02f, 0.12f), 0);
		Basis basis;
		basis.rotate(Vector3(0, 1, 0), a);
		basis.scale(Vector3(cherry_radius, cherry_radius, cherry_radius));
		r_out.xforms.push_back(Transform3D(basis, p));
		r_out.colors.push_back(cherry_color);
	}
}

} // namespace godot
