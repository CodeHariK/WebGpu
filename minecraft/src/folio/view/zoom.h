#ifndef FOLIO_VIEW_ZOOM_H
#define FOLIO_VIEW_ZOOM_H

namespace godot {

/**
 * Folio port — View / Zoom  (from `Game/View.js` setZoom + update)
 * ----------------------------------------------------------------
 * Single responsibility: the camera zoom ratio (0 = closest, 1 = farthest),
 * which drives the spherical radius. Combines a player-controlled `base_ratio`
 * with a speed-reactive term (pull out while moving fast, high quality only) and
 * smooths the result.
 *
 * Plain helper (no GDCLASS): folio's `this.zoom = {}` sub-object.
 *
 * Input hooks (wired when `Inputs` is ported): `scroll()` = wheel 'zoom' action,
 * `set_toggle_active()` = gamepad 'zoomToggle', `add_base_ratio()` = pinch / map
 * drag. Until then they are simply callable no-op-safe setters.
 */
class ViewZoom {
public:
	double base_ratio = 0.6; // player-set target
	double ratio = 0.6; // base + speed term
	double smoothed_ratio = 0.6; // eased, what the radius reads

	double speed_amplitude = -0.4; // pull-out amount at high focus speed
	double speed_edge_min = 5.0;
	double speed_edge_max = 40.0;
	double sensitivity = 0.05; // wheel step

	double toggle = 0.0; // gamepad toggle drift
	double toggle_last = -1.0;

	// --- input hooks ---
	void scroll(double p_value); // wheel: base_ratio -= value * sensitivity
	void add_base_ratio(double p_delta); // pinch / map drag
	void set_toggle_active(bool p_active); // gamepad r3

	// Per-frame (default mode only): apply toggle drift, speed-reactive zoom, ease.
	void
	update(double p_delta,
		   double p_focus_point_speed,
		   bool p_focus_is_tracking,
		   int p_quality_level);

	double get_smoothed_ratio() const { return smoothed_ratio; }
};

} // namespace godot

#endif // FOLIO_VIEW_ZOOM_H
