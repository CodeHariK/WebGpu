#ifndef CAMERA_STATE_FIXED_H
#define CAMERA_STATE_FIXED_H

#include "../camera_state.h"
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * CameraStateFixed — top-down follow with an optional dead-zone.
 * -------------------------------------------------------------
 * Holds a fixed orientation (never orbits) and looks down at the target from a
 * constant offset, Zelda / Link's-Awakening style. With `fixed_deadzone > 0`
 * the framed point only chases the target once it leaves a ground-plane radius,
 * so small wander doesn't nudge the view; vertical position always follows.
 * `follow_offset` (or a default top-down offset) sets the vantage point.
 */
class CameraStateFixed : public CameraState {
private:
	Vector3 framed_pivot; ///< The ground point the camera is currently framing.
	bool first_frame = true; ///< Seed `framed_pivot` on the first update.

public:
	/// Show the mouse, reset framing, re-seat the springs on entry.
	void enter(GameCamera *p_camera) override;
	/// Advance the dead-zone framing, then smooth position and look at it.
	void
	update(GameCamera *p_camera,
		   float p_delta) override;
};

} // namespace godot

#endif // CAMERA_STATE_FIXED_H
