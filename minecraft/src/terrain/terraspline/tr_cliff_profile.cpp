/**
 * @file tr_cliff_profile.cpp
 * @brief TerrainSplineCliff: stations along the spline and the cross-section at each one.
 */
#include "tr_cliff.h"
#include "tr_deformer.h"
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>

namespace godot {

/// Small deterministic generator for the per-stratum constants (thickness, base step).
struct CliffRNG {
	uint64_t state;
	explicit CliffRNG(uint64_t p_seed) : state(p_seed * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL) {}
	float next() {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		return float(uint32_t(state >> 32)) / 4294967296.0f;
	}
	float
	range(float a,
		  float b) {
		return a + next() * (b - a);
	}
};

// ---------------------------------------------------------------------------------------------
// Stations
// ---------------------------------------------------------------------------------------------

/// base_offset, plus the plateau rim of a sibling deformer (its core half-width + falloff) when enabled,
/// so the wall stands just outside the terrain's own slope instead of inside it.
float TerrainSplineCliff::_rim_offset() const {
	float off = base_offset;
	if (rim_from_deformer && get_parent()) {
		TypedArray<Node> siblings = get_parent()->get_children();
		for (int i = 0; i < siblings.size(); ++i) {
			const TerrainSplineDeformer *d = Object::cast_to<TerrainSplineDeformer>(siblings[i]);
			if (d) {
				off += d->get_spline_width() + d->get_falloff_distance() + 1.25f; // +1 m: heightmap cells are 1 m
				break;
			}
		}
	}
	return off;
}

/**
 * @brief Samples the parent's curve every `segment_length` metres (plus the end point for open
 * curves) in this node's local space, with a horizontal "out" direction to the right of travel.
 * Optionally raycasts to find how far the ground is below the nominal foot.
 */
bool TerrainSplineCliff::_build_stations(
		std::vector<Station> &r_stations,
		bool &r_closed
) const {
	r_stations.clear();
	const ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (!spline) {
		return false;
	}
	Ref<Curve3D> curve = spline->get_curve();
	if (curve.is_null() || curve->get_point_count() < 2) {
		return false;
	}
	const float total = curve->get_baked_length();
	if (total <= segment_length * 0.5f) {
		return false;
	}
	r_closed = curve->is_closed();

	// Curve space -> world -> our local space (the mesh lives under this node).
	const Transform3D to_local = get_global_transform().affine_inverse() * spline->get_global_transform();

	// Even spacing that lands exactly on the end (or wraps exactly for closed curves).
	const int count = MAX(2, (int)Math::round(total / segment_length));
	const float step = total / (float)count;
	const int n = r_closed ? count : count + 1;
	const float side = flip_side ? -1.0f : 1.0f;

	PhysicsDirectSpaceState3D *space = nullptr;
	if (bottom_mode == BOTTOM_GROUND && get_world_3d().is_valid()) {
		space = get_world_3d()->get_direct_space_state();
	}
	Ref<PhysicsRayQueryParameters3D> ray;
	if (space) {
		ray.instantiate();
		if (static_body) {
			TypedArray<RID> exclude;
			exclude.push_back(static_body->get_rid());
			ray->set_exclude(exclude);
		}
	}

	r_stations.resize(n);
	for (int i = 0; i < n; ++i) {
		r_stations[i].out = Vector3();
	}
	for (int i = 0; i < n; ++i) {
		const float d = MIN(i * step, total);
		Station &st = r_stations[i];
		st.distance = d;
		st.top = to_local.xform(curve->sample_baked(d));

		// Horizontal tangent from a small central difference (wraps for closed curves).
		const float eps = MIN(0.5f, step * 0.25f);
		float d0 = d - eps, d1 = d + eps;
		if (r_closed) {
			d0 = Math::fposmod(d0, total);
			d1 = Math::fposmod(d1, total);
		} else {
			d0 = MAX(0.0f, d0);
			d1 = MIN(total, d1);
		}
		Vector3 tangent = to_local.basis.xform(curve->sample_baked(d1) - curve->sample_baked(d0));
		tangent.y = 0.0f;
		if (tangent.length_squared() < 1e-8f) {
			tangent = Vector3(1, 0, 0);
		}
		tangent.normalize();
		st.out = tangent.cross(Vector3(0, 1, 0)) * side; // Right of travel (tangent x up)
		st.out.normalize();
	}

	// Closed loops: the wall must face away from the plateau whatever the curve's winding. Test whether
	// the first station's "out" probe lands inside the loop (even-odd in XZ) and flip everything if so.
	if (r_closed && n >= 3) {
		const Vector3 probe = r_stations[0].top + r_stations[0].out * 0.5f;
		bool inside = false;
		for (int i = 0, j = n - 1; i < n; j = i++) {
			const Vector3 &a = r_stations[i].top, &b = r_stations[j].top;
			if (((a.z > probe.z) != (b.z > probe.z)) && (probe.x < (b.x - a.x) * (probe.z - a.z) / (b.z - a.z) + a.x)) {
				inside = !inside;
			}
		}
		if (inside) {
			for (Station &st : r_stations) {
				st.out = -st.out;
			}
		}
	}

	const float rim = _rim_offset();
	for (int i = 0; i < n; ++i) {
		Station &st = r_stations[i];
		st.top += st.out * rim; // Top edge on the plateau rim, not on the spline itself

		// Wall height at this station.
		float h = height;
		if (height_curve.is_valid() && total > 0.0f) {
			h *= MAX(0.05f, (float)height_curve->sample_baked(st.distance / total));
		}
		if (bottom_mode == BOTTOM_ABSOLUTE) {
			const float top_world_y = get_global_transform().xform(st.top).y;
			h = top_world_y - bottom_y;
		} else if (space) {
			const Vector3 probe_local = st.top + st.out * (step_out + ledge_depth);
			const Vector3 from = get_global_transform().xform(probe_local);
			ray->set_from(from);
			ray->set_to(from + Vector3(0, -1000.0f, 0));
			Dictionary hit = space->intersect_ray(ray);
			if (hit.has("position")) {
				h = from.y - ((Vector3)hit["position"]).y;
			}
		}
		st.wall_height = MAX(0.1f, h);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Cross-section
// ---------------------------------------------------------------------------------------------

/// Along-wall wobble for one stratum: quantized simplex noise in [-amplitude, amplitude].
float TerrainSplineCliff::_wobble(
		float p_distance,
		int p_stratum,
		float p_channel
) const {
	if (noise_amplitude <= 0.0f || noise_frequency <= 0.0f) {
		return 0.0f;
	}
	if (column_width > 0.0f) { // Whole vertical panels share one value: fractured, columnar rock
		p_distance = (Math::floor(p_distance / column_width) + 0.5f) * column_width;
	}
	float v =
			_noise->get_noise_2d(p_distance * noise_frequency, p_stratum * 17.31f + p_channel * 101.7f + seed * 0.37f);
	v *= noise_amplitude;
	if (noise_quantize > 0.0f) {
		v = Math::round(v / noise_quantize) * noise_quantize;
	}
	return v;
}

/**
 * @brief The cross-section at one station, top to bottom:
 *   lip (over the plateau) -> top edge -> for each stratum: bevel, vertical face, step to the next
 *   layer (out = ledge, in = overhang) -> skirt below the foot.
 * Layer thicknesses and base steps are per-stratum constants from `seed`; the along-wall variation
 * comes from `_wobble` and the ledge mask, so neighbouring stations agree and the wall reads as one
 * body of rock rather than a random polyline.
 */
void TerrainSplineCliff::_build_profile(
		const Station &p_station,
		float p_total_length,
		bool p_closed,
		std::vector<ProfilePoint> &r_profile
) const {
	r_profile.clear();
	CliffRNG rng((uint64_t)(uint32_t)seed);

	// Per-stratum constants (identical for every station).
	std::vector<float> thickness(strata), base_step(strata);
	float sum = 0.0f;
	for (int k = 0; k < strata; ++k) {
		thickness[k] = 1.0f + strata_variation * rng.range(-0.9f, 0.9f);
		sum += thickness[k];
		base_step[k] = rng.range(-step_out, step_out);
	}
	(void)p_total_length;
	for (int k = 0; k < strata; ++k) {
		thickness[k] *= p_station.wall_height / sum;
	}

	auto push = [&](float p_offset, float p_depth, int p_stratum) {
		ProfilePoint pt;
		pt.offset = p_offset;
		pt.depth = p_depth;
		pt.stratum = p_stratum;
		r_profile.push_back(pt);
	};

	// Lip: start slightly inside and below the plateau surface so the seam is hidden under the grass.
	// Not needed when the cliff builds its own top cap - the cap meets the wall exactly at the edge.
	const bool self_capped = cap_top && p_closed;
	if (lip > 0.0f && !self_capped) {
		push(-lip, 0.15f, 0);
		push(-lip * 0.5f, -0.05f, 0);
	}

	// Each layer: [chamfer in from the corner ->] chamfer down -> vertical face; the next layer's first
	// point closes the horizontal step (ledge top when it goes out, overhang underside when it goes in).
	float offset = 0.0f;
	float depth = 0.0f;
	for (int k = 0; k < strata; ++k) {
		const float face_top = depth;
		const float face_bottom = depth + thickness[k];
		const float b = MIN(bevel, thickness[k] * 0.45f);
		push(offset - b, face_top, k);
		if (b > 0.0f) {
			push(offset, face_top + b, k);
		}
		push(offset, face_bottom, k);

		if (k + 1 < strata) {
			// Step to the next layer: base step + wobble, plus a ledge where the ledge mask is high.
			float step = base_step[k] + _wobble(p_station.distance, k, 0.0f);
			const float ledge_mask =
					_noise->get_noise_2d(p_station.distance * noise_frequency * 0.6f, k * 53.7f + 7.0f + seed);
			if (ledge_chance > 0.0f && (ledge_mask * 0.5f + 0.5f) < ledge_chance) {
				step += ledge_depth;
			}
			// Never step back behind the rim: the terrain's own face is there and would poke through.
			offset = MAX(0.0f, offset + step);
		}
		depth = face_bottom;
	}

	// Skirt: straight down past the foot, hiding the ground seam.
	const float skirt_depth = depth + skirt;
	if (skirt_depth > depth) {
		push(offset, skirt_depth, strata - 1);
	}
}

} // namespace godot
