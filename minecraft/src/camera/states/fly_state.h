#ifndef CAMERA_STATE_FLY_H
#define CAMERA_STATE_FLY_H

#include "../camera_state.h"

namespace godot {

/**
 * CameraStateFly — free-flying debug / spectator camera.
 * ------------------------------------------------------
 * Not attached to any pivot: the camera holds its own focus in the position
 * spring's target and the player moves it directly. While the orbit button is
 * held, look-delta orbits (yaw/pitch); with the pan modifier it strafes the
 * focus along the camera's right/up axes; scroll dollies along forward. The
 * mouse stays visible. Handy for inspecting scenes and debugging.
 */
class CameraStateFly : public CameraState {
public:
	/// Show the mouse and re-seat the springs on the current transform.
	void enter(GameCamera *p_camera) override;
	/// Apply input, then smooth rotation and position toward the free-fly focus.
	void
	update(GameCamera *p_camera,
		   float p_delta) override;
};

} // namespace godot

#endif // CAMERA_STATE_FLY_H
