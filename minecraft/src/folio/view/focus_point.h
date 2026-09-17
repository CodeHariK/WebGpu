#ifndef FOLIO_VIEW_FOCUS_POINT_H
#define FOLIO_VIEW_FOCUS_POINT_H

#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * Folio port — View / FocusPoint  (from `Game/View.js` setFocusPoint + update)
 * ----------------------------------------------------------------------------
 * Single responsibility: the point on the ground the camera frames. It follows a
 * `tracked_position` (the player copies its position into it each frame), applies
 * a "magnet" that eases toward the tracked point, then low-pass smooths into
 * `smoothed_position` (what the camera actually looks at). Its per-frame travel
 * speed feeds the zoom's speed-reactive pull-out.
 *
 * Plain helper (no GDCLASS): folio's `this.focusPoint = {}` sub-object. Only the X
 * and Z axes matter (ground plane); Y stays 0.
 *
 * Hooks (wired to gameplay/inputs later):
 *   - `set_tracked_position()` — the vehicle/player calls this each frame.
 *   - `resume_tracking()`      — an input action re-enables following.
 *   - `pan()`                  — map drag / right stick nudges the point freely.
 *   - `set_position()/set_tracking()` — areas can force a framing (podium, etc.).
 */
class ViewFocusPoint {
public:
	Vector3 tracked_position; // where we want to be (player position)
	Vector3 position; // eased-with-magnet point
	Vector3 smoothed_position; // low-passed, what the camera looks at

	bool is_tracking = true;
	double easing = 1.0;

	bool magnet_active = true;
	double magnet_multiplier = 0.25;

	void init(const Vector3 &p_initial);

	// --- hooks ---
	void set_tracked_position(const Vector3 &p_pos); // player follow (X/Z)
	void resume_tracking() { is_tracking = true; }
	void set_tracking(bool p_tracking) { is_tracking = p_tracking; }
	void set_position(const Vector3 &p_pos) { position = p_pos; }
	void
	pan(double p_dx,
		double p_dz); // free move (disables tracking)

	// Advance one frame (unscaled delta). Returns the focus travel speed (units/s),
	// which the zoom system consumes.
	double update(double p_delta);

	Vector3 get_position() const { return position; }
	Vector3 get_smoothed_position() const { return smoothed_position; }
	bool get_is_tracking() const { return is_tracking; }
};

} // namespace godot

#endif // FOLIO_VIEW_FOCUS_POINT_H
