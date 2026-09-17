#include "spherical.h"

#include <godot_cpp/core/math.hpp>

namespace godot {

Vector3 ViewSpherical::from_spherical(
		double p_radius,
		double p_phi,
		double p_theta
) {
	// Matches THREE.Vector3.setFromSphericalCoords(radius, phi, theta).
	const double sin_phi_radius = Math::sin(p_phi) * p_radius;
	return Vector3(
			sin_phi_radius * Math::sin(p_theta), Math::cos(p_phi) * p_radius, sin_phi_radius * Math::cos(p_theta)
	);
}

void ViewSpherical::init(
		int p_quality_level,
		double p_initial_smoothed_ratio
) {
	phi = Math::PI * (p_quality_level == 0 ? 0.31 : 0.27);
	theta = Math::PI * 0.25;

	radius_current = Math::lerp(radius_min, radius_max, 1.0 - p_initial_smoothed_ratio);
	offset = from_spherical(radius_current, phi, theta);
}

double ViewSpherical::get_radius_max(double p_ratio_overflow) const {
	return radius_max + p_ratio_overflow * non_ideal_ratio_offset;
}

void ViewSpherical::update(
		double p_smoothed_ratio,
		double p_ratio_overflow
) {
	const double radius_max_corrected = get_radius_max(p_ratio_overflow);
	radius_current = Math::lerp(radius_min, radius_max_corrected, 1.0 - p_smoothed_ratio);
	offset = from_spherical(radius_current, phi, theta);
}

} // namespace godot
