#include "optimal_area.h"

#include "spherical.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/transform3d.hpp>

namespace godot {

// Intersect a ray (origin + t*dir) with the floor plane y = 0. Returns false if
// the ray is (near) parallel to the plane.
static bool intersect_floor(
		const Vector3 &p_origin,
		const Vector3 &p_dir,
		Vector3 &r_hit
) {
	if (Math::abs(p_dir.y) < 1e-6) {
		return false;
	}
	const double t = -p_origin.y / p_dir.y;
	r_hit = p_origin + p_dir * t;
	return true;
}

void ViewOptimalArea::recompute(
		double p_phi,
		double p_theta,
		double p_radius_max,
		double p_fov_y,
		double p_aspect
) {
	// Virtual camera at the orbit's max radius, looking at the origin.
	const Vector3 pos = ViewSpherical::from_spherical(p_radius_max, p_phi, p_theta);
	const Transform3D cam = Transform3D(Basis(), pos).looking_at(Vector3(0, 0, 0), Vector3(0, 1, 0));
	const Basis b = cam.basis;

	const double tan_y = Math::tan(p_fov_y * 0.5);
	const double tan_x = tan_y * p_aspect;

	// World-space ray direction through an NDC corner (nx, ny) in [-1, 1].
	// Camera looks down local -Z, +Y up, +X right (same convention as THREE).
	auto ray = [&](double nx, double ny) -> Vector3 { return b.xform(Vector3(nx * tan_x, ny * tan_y, -1.0)); };

	Vector3 hit;

	// First diagonal: near (1,-1) and far (-1,1).
	intersect_floor(pos, ray(1.0, -1.0), hit);
	quad_base[0] = Vector2(hit.x, hit.z);
	const Vector3 near_a = hit;

	intersect_floor(pos, ray(-1.0, 1.0), hit);
	quad_base[2] = Vector2(hit.x, hit.z);
	const Vector3 far_a = hit;

	const Vector3 center_a = near_a.lerp(far_a, 0.5);

	// Second diagonal: near (-1,-1) and far (1,1).
	intersect_floor(pos, ray(-1.0, -1.0), hit);
	quad_base[3] = Vector2(hit.x, hit.z);
	const Vector3 near_b = hit;

	intersect_floor(pos, ray(1.0, 1.0), hit);
	quad_base[1] = Vector2(hit.x, hit.z);
	const Vector3 far_b = hit;

	const Vector3 center_b = near_b.lerp(far_b, 0.5);

	// Centre between the two diagonal centres; radius to a far corner.
	base_position = center_a.lerp(center_b, 0.5);
	radius = base_position.distance_to(far_b);

	// Near/far ground distances along the vertical screen centre line.
	Vector3 near_p, far_p;
	intersect_floor(pos, ray(0.0, -1.0), near_p);
	intersect_floor(pos, ray(0.0, 1.0), far_p);
	near_distance = pos.distance_to(near_p);
	far_distance = pos.distance_to(far_p);

	needs_update = false;
}

void ViewOptimalArea::apply_focus(
		const Vector3 &p_smoothed_focus,
		const Vector3 &p_raw_focus
) {
	position = base_position + Vector3(p_smoothed_focus.x, 0.0, p_smoothed_focus.z);

	for (int i = 0; i < 4; i++) {
		quad_offseted[i] = quad_base[i] + Vector2(p_raw_focus.x, p_raw_focus.z);
	}
}

} // namespace godot
