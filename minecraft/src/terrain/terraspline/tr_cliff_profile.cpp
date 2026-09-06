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

	const float wall_h = MAX(0.001f, p_station.wall_height);

	// Where this station sits within its column: 1 inside a cleft (groove at a column boundary), else 0.
	float cleft = 0.0f;
	float col_variation = 0.0f; // Per-column brightness jitter in [-1, 1]
	if (column_width > 0.0f) {
		const float in_col = Math::fposmod(p_station.distance, column_width);
		const float to_boundary = MIN(in_col, column_width - in_col);
		if (cleft_width > 0.0f && to_boundary < cleft_width * 0.5f) {
			cleft = 1.0f;
		}
		const float col_centre = (Math::floor(p_station.distance / column_width) + 0.5f) * column_width;
		col_variation = _noise->get_noise_2d(col_centre * 0.37f, 911.0f + seed);
	}
	// Talus factor: 0 on the wall, ramping to 1 over the 10 % below talus_start (smooth apron).
	auto talus = [&](float p_depth) -> float {
		if (talus_start >= 1.0f) {
			return 0.0f;
		}
		const float t = (p_depth / wall_h - talus_start) / 0.1f;
		return CLAMP(t, 0.0f, 1.0f);
	};

	auto push = [&](float p_offset, float p_depth, int p_stratum) {
		ProfilePoint pt;
		const float rock = 1.0f - talus(p_depth); // Clefts and column shading belong to the wall, not the apron
		pt.offset = p_offset - cleft_depth * cleft * rock;
		pt.depth = p_depth;
		pt.depth_norm = CLAMP(p_depth / wall_h, 0.0f, 1.0f);
		pt.shade = (1.0f - cleft_shade * cleft * rock) * (1.0f + column_shade * col_variation * rock);
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

	// Silhouette from the profile curve: horizontal offset as a function of normalized depth.
	auto shape = [&](float p_depth) -> float {
		if (profile_curve.is_null()) {
			return 0.0f;
		}
		return (float)profile_curve->sample_baked(CLAMP(p_depth / wall_h, 0.0f, 1.0f)) * profile_amount;
	};

	// Each layer: [chamfer in from the corner ->] chamfer down -> face; the next layer's first point
	// closes the horizontal step (ledge top when it goes out, overhang underside when it goes in).
	// `steps` is the accumulated random in/out of the strata; the curve's shape is added on top, so a
	// layer's face follows the silhouette (it may lean) while the steps stay crisp.
	float steps = 0.0f;
	float depth = 0.0f;
	for (int k = 0; k < strata; ++k) {
		const float face_top = depth;
		const float face_bottom = depth + thickness[k];
		const float b = MIN(bevel, thickness[k] * 0.45f) * (1.0f - talus(face_top));
		const float o_top = steps + shape(face_top);
		const float o_bottom = steps + shape(face_bottom);
		push(o_top - b, face_top, k);
		if (b > 0.0f) {
			push(o_top + (o_bottom - o_top) * (b / thickness[k]), face_top + b, k);
		}
		push(o_bottom, face_bottom, k);

		if (k + 1 < strata) {
			// Step to the next layer: base step + wobble, plus a ledge where the ledge mask is high.
			// Wobble: per layer, or shared by the whole column (vertical fins) as column_coherence -> 1.
			const float wobble = Math::lerp(
					_wobble(p_station.distance, k, 0.0f), _wobble(p_station.distance, 1000, 0.0f), column_coherence
			);
			float step = base_step[k] + wobble;
			const float ledge_mask =
					_noise->get_noise_2d(p_station.distance * noise_frequency * 0.6f, k * 53.7f + 7.0f + seed);
			if (ledge_chance > 0.0f && (ledge_mask * 0.5f + 0.5f) < ledge_chance) {
				step += ledge_depth;
			}
			step *= 1.0f - talus(face_bottom); // No steps on the apron
			// The random part never steps back behind the rim (a hosting terrain face would poke through);
			// tucking under the rim is the profile curve's job.
			steps = MAX(0.0f, steps + step);
		}
		depth = face_bottom;
	}
	const float offset = steps + shape(depth);

	// Skirt: straight down past the foot, hiding the ground seam.
	const float skirt_depth = depth + skirt;
	if (skirt_depth > depth) {
		push(offset, skirt_depth, strata - 1);
	}
}

} // namespace godot
