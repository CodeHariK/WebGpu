#ifndef CAMERA_STATE_H
#define CAMERA_STATE_H

#include <godot_cpp/core/class_db.hpp>

namespace godot {

class GameCamera;

/**
 * CameraState — behaviour interface for the GameCamera state machine.
 * -------------------------------------------------------------------
 * Each concrete state (fly / car / TPS / fixed) implements one camera
 * behaviour. `GameCamera` owns exactly one live state at a time and forwards
 * every physics tick to `update()`. States are lightweight: they hold only
 * their own transient smoothing scratch and reach the shared rig state (springs,
 * yaw/pitch, follow target, helpers) through friendship with `GameCamera`.
 *
 * Contract:
 *   - `enter()` runs once when the mode becomes active (set up mouse mode,
 *     reset scratch, and rebase the springs so there is no snap).
 *   - `update()` runs every physics frame while active.
 *   - `exit()` runs once when leaving the mode (optional cleanup).
 */
class CameraState {
public:
	virtual ~CameraState() {}

	/// Called once when this behaviour becomes the active mode.
	virtual void enter(GameCamera *p_camera) {}
	/// Called once when this behaviour stops being the active mode.
	virtual void exit(GameCamera *p_camera) {}

	/// Called every physics frame while active; `p_delta` is seconds.
	virtual void
	update(GameCamera *p_camera,
		   float p_delta) = 0;
};

} // namespace godot

#endif // CAMERA_STATE_H
