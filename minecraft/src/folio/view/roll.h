#ifndef FOLIO_VIEW_ROLL_H
#define FOLIO_VIEW_ROLL_H

namespace godot {

/**
 * Folio port — FolioView / Roll  (from `Game/View.js` setRoll + update)
 * ----------------------------------------------------------------
 * Single responsibility: a 1-D damped spring that produces a small camera z-roll
 * (bank). `value` is the current roll angle added to the default camera's
 * rotation.z each frame; `kick()` injects a random-direction impulse (used on
 * events like landings / hits) that springs back to zero.
 *
 * Plain helper (no GDCLASS): an internal part of `FolioView` (folio's `this.roll = {}`).
 *
 * Integration (folio, uses the SCALED delta so it slows in bullet time):
 *   velocity = -value * pull_strength * dt_scaled
 *   speed   += velocity
 *   value   += speed * dt_scaled
 *   speed   *= 1 - damping * dt_scaled
 */
class FolioViewRoll {
public:
	double value = 0.0; // current roll angle (radians), applied to camera.rotation.z
	double velocity = 0.0;
	double speed = 0.0;

	double damping = 4.0;
	double pull_strength = 100.0; // spring pull back toward zero
	double kick_strength = 1.0; // scales a kick() impulse

	// Inject a random-direction roll impulse.
	void kick(double p_strength);

	// Advance the spring by one scaled-delta step; updates `value`.
	void update(double p_delta_scaled);
};

} // namespace godot

#endif // FOLIO_VIEW_ROLL_H
