#ifndef FOLIO_VIEW_SPHERICAL_H
#define FOLIO_VIEW_SPHERICAL_H

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * Folio port — FolioView / Spherical  (from `Game/View.js` setSpherical + update)
 * --------------------------------------------------------------------------
 * Single responsibility: the orbit geometry of the default camera. Holds the
 * orbit angles (phi = down-tilt, theta = yaw) and the zoom-driven radius, and
 * produces the world-space `offset` added to the focus point to place the camera.
 *
 * Plain helper (no GDCLASS): it is an internal part of `FolioView`, mirroring folio's
 * `this.spherical = {}` sub-object. No scene-tree involvement.
 *
 * Radius: lerps between `radius_min` and a ratio-corrected `radius_max` by the
 * (inverted) smoothed zoom ratio. `non_ideal_ratio_offset` pushes the camera back
 * on non-ideal aspect ratios so framing stays consistent.
 */
class FolioViewSpherical {
public:
	// Orbit angles (radians). phi from +Y down, theta yaw around Y.
	double phi = 0.0;
	double theta = Math::PI * 0.25;

	// Radius edges + current, and the non-ideal-ratio pushback.
	double radius_min = 15.0;
	double radius_max = 30.0;
	double non_ideal_ratio_offset = 9.0;
	double radius_current = 21.0;

	// World offset from the focus point to the camera (result of the orbit).
	Vector3 offset;

	// quality_level 0 = highest (folio tilts phi slightly more at high quality).
	void
	init(int p_quality_level,
		 double p_initial_smoothed_ratio);

	// Recompute radius + offset from the current smoothed zoom ratio and the
	// aspect-ratio overflow. Call once per frame.
	void
	update(double p_smoothed_ratio,
		   double p_ratio_overflow);

	// Max radius used for framing (matches optimal-area's reset radius).
	double get_radius_max(double p_ratio_overflow) const;

	// Three.js Vector3.setFromSphericalCoords(radius, phi, theta).
	static Vector3 from_spherical(
			double p_radius,
			double p_phi,
			double p_theta
	);
};

} // namespace godot

#endif // FOLIO_VIEW_SPHERICAL_H
