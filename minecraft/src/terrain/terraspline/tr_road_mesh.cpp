/**
 * @file tr_road_mesh.cpp
 * @brief TerrainSplineRoad: stations along the spline, sweeping the cross-section, stitching, caps.
 */
#include "tr_compositor.h"
#include "tr_deformer.h"
#include "tr_road.h"
#include "utils/curve/curve_baker.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/geometry2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

namespace {

/// Flat-shaded triangle accumulator with colour and UV, clockwise as seen from `outward`.
struct RoadMeshBuilder {
	PackedVector3Array vertices, normals;
	PackedVector2Array uvs;
	PackedVector2Array uv2s; // x = 0..1 across the deck (0.5 = centre line), y = 1 on the deck else 0
	PackedColorArray colors;
	PackedInt32Array indices;

	void
	tri(const Vector3 &a,
		const Vector3 &b,
		const Vector3 &c,
		const Vector2 &uva,
		const Vector2 &uvb,
		const Vector2 &uvc,
		const Vector3 &p_outward,
		const Color &p_color,
		const Vector2 &uv2a =
				Vector2(0.5f,
						0.0f),
		const Vector2 &uv2b =
				Vector2(0.5f,
						0.0f),
		const Vector2 &uv2c =
				Vector2(0.5f,
						0.0f)) {
		Vector3 n = (b - a).cross(c - a);
		if (n.length_squared() < 1e-12f) {
			return;
		}
		Vector3 v1 = b, v2 = c;
		Vector2 t1 = uvb, t2 = uvc;
		Vector2 s1 = uv2b, s2 = uv2c;
		if (n.dot(p_outward) > 0.0f) {
			std::swap(v1, v2);
			std::swap(t1, t2);
			std::swap(s1, s2);
			n = -n;
		}
		n = -n;
		n.normalize();
		const int base = vertices.size();
		vertices.push_back(a);
		vertices.push_back(v1);
		vertices.push_back(v2);
		uvs.push_back(uva);
		uvs.push_back(t1);
		uvs.push_back(t2);
		uv2s.push_back(uv2a);
		uv2s.push_back(s1);
		uv2s.push_back(s2);
		for (int i = 0; i < 3; ++i) {
			normals.push_back(n);
			colors.push_back(p_color);
			indices.push_back(base + i);
		}
	}
};

/// A cross-section corner placed at a station: X = lateral, Y = up (the spline's tilt is in the frame).
inline Vector3
place(const Transform3D &p_station,
	  const Vector2 &p_pos) {
	return p_station.origin + p_station.basis.get_column(0) * p_pos.x + p_station.basis.get_column(1) * p_pos.y;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Terrain mode
// ---------------------------------------------------------------------------------------------

/// The sibling HEIGHT_TERRAIN deformer's road profile (world-space points along the spline), or false.
bool TerrainSplineRoad::_terrain_profile(std::vector<Vector3> &r_points) const {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (!spline) {
		return false;
	}
	const TerrainSplineCompositor *comp = Object::cast_to<TerrainSplineCompositor>(spline->get_parent());
	if (!comp) {
		UtilityFunctions::push_warning(
				"[TerrainSplineRoad] TERRAIN mode needs the spline under a TerrainSplineCompositor."
		);
		return false;
	}
	TypedArray<Node> siblings = spline->get_children();
	for (int i = 0; i < siblings.size(); ++i) {
		TerrainSplineDeformer *d = Object::cast_to<TerrainSplineDeformer>(siblings[i]);
		if (d && d->get_height_source() == TerrainSplineDeformer::HEIGHT_TERRAIN) {
			return d->bake_road_profile(comp->make_profile_context(), spline, r_points);
		}
	}
	UtilityFunctions::push_warning(
			"[TerrainSplineRoad] TERRAIN mode needs a sibling TerrainSplineDeformer with height_source = Terrain."
	);
	return false;
}

/**
 * @brief Replaces each station's height with the profile's, interpolated by arc length along the
 * profile's own vertices, and flattens the frame (upright, no banking) - the ground cannot bank.
 */
void TerrainSplineRoad::_apply_terrain_heights(
		std::vector<Transform3D> &r_stations,
		const std::vector<float> &p_distances,
		const std::vector<Vector3> &p_profile
) const {
	// Arc length of the profile vertices (world space).
	std::vector<float> arc(p_profile.size(), 0.0f);
	for (size_t i = 1; i < p_profile.size(); ++i) {
		arc[i] =
				arc[i - 1] + Vector2(p_profile[i].x - p_profile[i - 1].x, p_profile[i].z - p_profile[i - 1].z).length();
	}
	const Transform3D to_local = get_global_transform().affine_inverse();
	const Transform3D to_world = get_global_transform();
	for (size_t s = 0; s < r_stations.size(); ++s) {
		// Height at this station's arc distance (the profile and the curve share their parametrization
		// up to bake-interval rounding, so match by distance rather than by index).
		const float d = p_distances[s];
		size_t hi = 1;
		while (hi + 1 < arc.size() && arc[hi] < d) {
			++hi;
		}
		const size_t lo = hi - 1;
		const float span = MAX(1e-4f, arc[hi] - arc[lo]);
		const float t = CLAMP((d - arc[lo]) / span, 0.0f, 1.0f);
		const float world_y = Math::lerp(p_profile[lo].y, p_profile[hi].y, t) + surface_offset;

		Transform3D &st = r_stations[s];
		Vector3 world_origin = to_world.xform(st.origin);
		world_origin.y = world_y;
		st.origin = to_local.xform(world_origin);
		// Upright frame: keep the horizontal heading, drop the tilt.
		Vector3 along = st.basis.get_column(2);
		along.y = 0.0f;
		along = along.length_squared() > 1e-8f ? along.normalized() : Vector3(0, 0, 1);
		const Vector3 up(0, 1, 0);
		Vector3 right = up.cross(along).normalized();
		if (right.dot(st.basis.get_column(0)) < 0.0f) { // Keep the original left/right sense
			right = -right;
		}
		st.basis = Basis(right, up, along);
	}
}

// ---------------------------------------------------------------------------------------------
// Stations
// ---------------------------------------------------------------------------------------------

/**
 * @brief Frames along the section of the parent curve, in this node's local space, with the arc
 * distance of each. Fixed: every `segment_length`, landing exactly on the section end (or wrapping for
 * a whole closed spline). Adaptive: CurveBaker subdivides where the direction changes faster than
 * `adaptive_angle_tol`.
 */
bool TerrainSplineRoad::_build_stations(
		std::vector<Transform3D> &r_stations,
		std::vector<float> &r_distances,
		bool &r_loop
) const {
	r_stations.clear();
	r_distances.clear();
	r_loop = false;
	const ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (!spline) {
		return false;
	}
	Ref<Curve3D> curve = spline->get_curve();
	if (curve.is_null() || curve->get_point_count() < 2) {
		return false;
	}
	const float total = curve->get_baked_length();
	float start = CLAMP(section_start, 0.0f, total);
	float end = section_end > section_start ? MIN(section_end, total) : total;
	if (end - start < 0.5f) {
		return false;
	}
	const bool whole = start <= 0.0f && end >= total;
	r_loop = whole && curve->is_closed();

	const Transform3D to_local = get_global_transform().affine_inverse() * spline->get_global_transform();

	if (sampling == SAMPLING_ADAPTIVE) {
		std::vector<Transform3D> world = CurveBaker::bake_transforms_adaptive(
				curve, start, end, adaptive_max_step, adaptive_min_step, adaptive_angle_tol,
				spline->get_global_transform()
		);
		const Transform3D inv = get_global_transform().affine_inverse();
		float d = start;
		for (size_t i = 0; i < world.size(); ++i) {
			if (i > 0) {
				d += world[i].origin.distance_to(world[i - 1].origin); // Chord length ~ arc length
			}
			r_stations.push_back(inv * world[i]);
			r_distances.push_back(d);
		}
		if (r_loop && r_stations.size() > 2) { // The adaptive bake repeats the start; the loop closes itself
			r_stations.pop_back();
			r_distances.pop_back();
		}
		if (height_source == HEIGHT_TERRAIN) {
			std::vector<Vector3> profile;
			if (_terrain_profile(profile)) {
				_apply_terrain_heights(r_stations, r_distances, profile);
			}
		}
		return r_stations.size() >= 2;
	}

	const int count = MAX(1, (int)Math::round((end - start) / segment_length));
	const float step = (end - start) / (float)count;
	const int n = r_loop ? count : count + 1;
	for (int i = 0; i < n; ++i) {
		const float d = MIN(start + i * step, end);
		r_stations.push_back(to_local * curve->sample_baked_with_rotation(d, false, /*apply_tilt=*/true));
		r_distances.push_back(d);
	}
	if (height_source == HEIGHT_TERRAIN) {
		std::vector<Vector3> profile;
		if (_terrain_profile(profile)) {
			_apply_terrain_heights(r_stations, r_distances, profile);
		}
	}
	return r_stations.size() >= 2;
}

// ---------------------------------------------------------------------------------------------
// Mesh
// ---------------------------------------------------------------------------------------------

/**
 * @brief One quad per profile edge between neighbouring stations, oriented by the edge's 2-D outward
 * normal expressed in the station frame; U runs across the profile, V along the track in
 * `texture_length` repeats, UV2 = (0..1 across the deck, on-deck flag) for the marking shader; colour per region.
 * Closed profiles on open sections get fan caps.
 */
Ref<ArrayMesh> TerrainSplineRoad::_build_mesh(
		const std::vector<Transform3D> &p_stations,
		const std::vector<float> &p_distances,
		bool p_loop,
		const std::vector<ProfilePoint> &p_profile,
		bool p_profile_closed
) const {
	RoadMeshBuilder mb;
	const size_t n = p_stations.size();
	const size_t m = p_profile.size();
	const size_t edges = p_profile_closed ? m : m - 1;

	// U per profile corner: cumulative perimeter, normalized.
	std::vector<float> u(m + 1, 0.0f);
	for (size_t j = 1; j <= edges; ++j) {
		u[j] = u[j - 1] + p_profile[j % m].pos.distance_to(p_profile[j - 1].pos);
	}
	const float perimeter = MAX(1e-4f, u[edges]);
	for (float &v : u) {
		v /= perimeter;
	}

	// Deck extent for UV2 (lane markings): the lateral span of the deck-region edges.
	float deck_min = 1e9f, deck_max = -1e9f;
	for (size_t j = 0; j < edges; ++j) {
		if (p_profile[j].region == REGION_DECK) {
			const Vector2 &pa = p_profile[j].pos, &pb = p_profile[(j + 1) % m].pos;
			deck_min = MIN(deck_min, MIN(pa.x, pb.x));
			deck_max = MAX(deck_max, MAX(pa.x, pb.x));
		}
	}
	const bool has_deck = deck_max > deck_min;
	_deck_width = has_deck ? deck_max - deck_min : 0.0f;
	auto uv2_for = [&](const ProfilePoint &p_start, const Vector2 &p_pos) -> Vector2 {
		if (!has_deck || p_start.region != REGION_DECK) {
			return Vector2(0.5f, 0.0f);
		}
		return Vector2((p_pos.x - deck_min) / _deck_width, 1.0f);
	};

	const size_t segments = p_loop ? n : n - 1;
	for (size_t s = 0; s < segments; ++s) {
		const Transform3D &s0 = p_stations[s];
		const Transform3D &s1 = p_stations[(s + 1) % n];
		const float v0 = p_distances[s] / texture_length;
		const float v1 =
				(s + 1 < n ? p_distances[s + 1] : p_distances[s] + s0.origin.distance_to(s1.origin)) / texture_length;
		for (size_t j = 0; j < edges; ++j) {
			const ProfilePoint &pa = p_profile[j];
			const ProfilePoint &pb = p_profile[(j + 1) % m];
			const Vector2 d2 = pb.pos - pa.pos;
			const Vector2 n2(d2.y, -d2.x); // Outward for a counter-clockwise polygon
			const Vector3 outward = s0.basis.get_column(0) * n2.x + s0.basis.get_column(1) * n2.y;
			const Color col = _region_color(pa.region);
			const Vector3 a0 = place(s0, pa.pos), a1 = place(s0, pb.pos);
			const Vector3 b0 = place(s1, pa.pos), b1 = place(s1, pb.pos);
			const Vector2 ua(u[j], v0), ub(u[j + 1], v0), uc(u[j + 1], v1), ud(u[j], v1);
			const Vector2 wa = uv2_for(pa, pa.pos), wb = uv2_for(pa, pb.pos);
			mb.tri(a0, a1, b1, ua, ub, uc, outward, col, wa, wb, wb);
			mb.tri(a0, b1, b0, ua, uc, ud, outward, col, wa, wb, wa);
		}
	}

	// Caps on open sections of a closed profile: the polygon itself, facing backwards / forwards.
	if (!p_loop && cap_ends && p_profile_closed && m >= 3) {
		PackedVector2Array poly;
		for (const ProfilePoint &pt : p_profile) {
			poly.push_back(pt.pos);
		}
		const PackedInt32Array tris = Geometry2D::get_singleton()->triangulate_polygon(poly);
		for (int end = 0; end < 2; ++end) {
			const Transform3D &st = p_stations[end == 0 ? 0 : n - 1];
			const Vector3 along = (p_stations[1].origin - p_stations[0].origin).normalized();
			const Vector3 outward = end == 0 ? -along : along;
			for (int t = 0; t + 2 < tris.size(); t += 3) {
				const Vector2 &q0 = p_profile[tris[t]].pos, &q1 = p_profile[tris[t + 1]].pos,
							  &q2 = p_profile[tris[t + 2]].pos;
				mb.tri(place(st, q0), place(st, q1), place(st, q2), q0 / width, q1 / width, q2 / width, outward,
					   underside_color);
			}
		}
	}

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	if (mb.vertices.size() >= 3) {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = mb.vertices;
		arrays[Mesh::ARRAY_NORMAL] = mb.normals;
		arrays[Mesh::ARRAY_TEX_UV] = mb.uvs;
		arrays[Mesh::ARRAY_TEX_UV2] = mb.uv2s;
		arrays[Mesh::ARRAY_COLOR] = mb.colors;
		arrays[Mesh::ARRAY_INDEX] = mb.indices;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	}
	return mesh;
}

} // namespace godot
