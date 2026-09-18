#ifndef FOLIO_VIEW_OPTIMAL_AREA_H
#define FOLIO_VIEW_OPTIMAL_AREA_H

#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * Folio port — FolioView / OptimalArea  (from `Game/View.js` setOptimalArea + update)
 * ------------------------------------------------------------------------------
 * Single responsibility: figure out the patch of ground the default camera can
 * see. It projects the camera's frustum corners onto the floor plane (y = 0) and
 * derives a centre, a framing `radius`, and near/far ground distances. Consumers
 * (Floor sizing, spawn framing) read `radius` / `position`.
 *
 * Plain helper (no GDCLASS): folio's `this.optimalArea = {}` sub-object.
 *
 * Port note: folio uses a THREE.Raycaster with NDC corners and a temp camera.
 * Here the same result is computed analytically from the camera transform + fov +
 * aspect (no screen size, no Camera3D object needed), which is self-contained.
 *
 * Two-phase, mirroring folio:
 *   - `recompute(...)` — heavy: run on init and on throttled resize.
 *   - `apply_focus(...)` — cheap per-frame: slide the base geometry to the focus.
 */
class FolioViewOptimalArea {
public:
	bool needs_update = true;

	// Base geometry in local (focus-relative) space, from recompute().
	Vector3 base_position;
	double radius = 0.0;
	double near_distance = 0.0;
	double far_distance = 0.0;
	Vector2 quad_base[4]; // ground quad corners (x, z)

	// Per-frame world-space results, from apply_focus().
	Vector3 position;
	Vector2 quad_offseted[4];

	// Heavy recompute: place a virtual camera at the orbit's max radius looking at
	// origin, project its frustum onto y = 0. fov_y in radians, aspect = w/h.
	void recompute(
			double p_phi,
			double p_theta,
			double p_radius_max,
			double p_fov_y,
			double p_aspect
	);

	// Cheap per-frame: position uses the SMOOTHED focus, quads use the RAW focus
	// (matches folio).
	void apply_focus(
			const Vector3 &p_smoothed_focus,
			const Vector3 &p_raw_focus
	);
};

} // namespace godot

#endif // FOLIO_VIEW_OPTIMAL_AREA_H
