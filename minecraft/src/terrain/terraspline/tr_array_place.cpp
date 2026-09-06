/**
 * @file tr_array_place.cpp
 * @brief TerrainSplineArray: stations along the spline -> instance transforms (local to this node).
 */
#include "tr_array.h"
#include "tr_compositor.h"
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>

namespace godot {

namespace {

struct ArrayRNG {
	uint64_t state;
	explicit ArrayRNG(uint64_t p_seed) : state(p_seed * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL) {}
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

} // namespace

/**
 * @brief Distance from p_local_from straight down to the ground (0 when there is none).
 * Prefers the Terrain3D heightmap when the spline lives under a TerrainSplineCompositor - exact, and
 * available everywhere, whereas Terrain3D's collision only exists near the player. Falls back to a
 * physics raycast (props, roads, cliffs).
 */
float TerrainSplineArray::_ground_drop(const Vector3 &p_local_from) const {
	const Vector3 from = get_global_transform().xform(p_local_from);
	if (const Node *spline = get_parent()) {
		if (const TerrainSplineCompositor *comp = Object::cast_to<TerrainSplineCompositor>(spline->get_parent())) {
			if (Node *terrain = comp->get_terrain()) {
				Object *data = Object::cast_to<Object>(terrain->get("data"));
				if (data) {
					const Variant h = data->call("get_height", from);
					if (h.get_type() == Variant::FLOAT && !Math::is_nan((float)h)) {
						return MAX(0.0f, from.y - (float)h);
					}
				}
			}
		}
	}
	if (!get_world_3d().is_valid()) {
		return 0.0f;
	}
	PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state();
	if (!space) {
		return 0.0f;
	}
	Ref<PhysicsRayQueryParameters3D> ray;
	ray.instantiate();
	ray->set_from(from);
	ray->set_to(from + Vector3(0, -ground_max_distance, 0));
	if (static_body) {
		TypedArray<RID> exclude;
		exclude.push_back(static_body->get_rid());
		ray->set_exclude(exclude);
	}
	Dictionary hit = space->intersect_ray(ray);
	if (!hit.has("position")) {
		return 0.0f;
	}
	return MAX(0.0f, from.y - ((Vector3)hit["position"]).y);
}

/**
 * @brief Walks the section every `spacing` (± jitter) metres from `start_offset`. At each station the
 * spline frame (X lateral, Y up incl. tilt, Z along) places one instance per requested side:
 * sideways by lateral_offset, up by vertical_offset, yawed to the tangent (turned to face the spline
 * on the sides when face_inward), upright unless follow_tilt, scaled (with jitter), and optionally
 * stretched down to the ground.
 */
bool TerrainSplineArray::_build_transforms(std::vector<Transform3D> &r_transforms) const {
	r_transforms.clear();
	const ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (!spline) {
		return false;
	}
	Ref<Curve3D> curve = spline->get_curve();
	if (curve.is_null() || curve->get_point_count() < 2) {
		return false;
	}
	const float total = curve->get_baked_length();
	const float start = CLAMP(section_start, 0.0f, total);
	const float end = section_end > section_start ? MIN(section_end, total) : total;
	if (end <= start) {
		return false;
	}
	const Transform3D to_local = get_global_transform().affine_inverse() * spline->get_global_transform();
	ArrayRNG rng((uint64_t)(uint32_t)seed);

	// Mesh height for stretch_to_ground (pivot assumed at the base).
	const float mesh_h = mesh.is_valid() ? MAX(0.01f, mesh->get_aabb().size.y) : 1.0f;

	for (float d = start + start_offset; d <= end + 1e-3f;
		 d += spacing * (1.0f + spacing_jitter * rng.range(-1.0f, 1.0f))) {
		const Transform3D frame = to_local * curve->sample_baked_with_rotation(MIN(d, total), false, follow_tilt);
		Vector3 lateral = frame.basis.get_column(0);
		Vector3 up = follow_tilt ? frame.basis.get_column(1) : Vector3(0, 1, 0);
		Vector3 along = frame.basis.get_column(2);
		if (!follow_tilt) { // Flatten the frame: upright instances even on banked curves
			along.y = 0.0f;
			along = along.length_squared() > 1e-8f ? along.normalized() : Vector3(0, 0, 1);
			lateral = up.cross(along).normalized();
		}

		const int sides_mask = side == SIDE_CENTER ? 1 : side == SIDE_LEFT ? 2 : side == SIDE_RIGHT ? 4 : 6;
		for (int bit = 1; bit <= 4; bit <<= 1) {
			if (!(sides_mask & bit)) {
				continue;
			}
			const float side_sign = bit == 2 ? -1.0f : bit == 4 ? 1.0f : 0.0f;
			Vector3 origin = frame.origin + lateral * (side_sign * lateral_offset) + up * vertical_offset;

			// Orientation: -Z along the spline, or facing the spline from the side.
			Basis basis;
			if (align_to_tangent) {
				Vector3 fwd = along;
				if (face_inward && side_sign != 0.0f) {
					fwd = -lateral * side_sign; // Look back towards the spline
				}
				basis = Basis::looking_at(-fwd, up); // Godot: -Z is forward
			}
			const float yaw = Math::deg_to_rad(yaw_jitter_deg) * rng.range(-1.0f, 1.0f);
			if (yaw != 0.0f) {
				basis = basis.rotated(up, yaw);
			}

			Vector3 s = scale * (1.0f + scale_jitter * rng.range(-1.0f, 1.0f));
			if (stretch_to_ground) {
				const float drop = _ground_drop(origin);
				if (drop > 0.0f) {
					origin -= up * drop; // Base on the ground ...
					s.y = drop / mesh_h; // ... top at the station
				} else {
					_ground_missing = true;
				}
			}
			if (mesh_centered) { // Lift a centred mesh so its base sits where a base-pivoted one would
				origin += up * (mesh_h * s.y * 0.5f);
			}
			basis = basis.scaled_local(s);
			r_transforms.push_back(Transform3D(basis, origin));
		}
	}
	return true;
}

} // namespace godot
