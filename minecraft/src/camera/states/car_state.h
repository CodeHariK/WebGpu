#ifndef CAMERA_STATE_CAR_H
#define CAMERA_STATE_CAR_H

#include "../camera_state.h"
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * CameraStateCar — arcade chase camera for a vehicle.
 * ---------------------------------------------------
 * Trails a RigidBody target. When the player is not manually orbiting it auto-
 * centres behind the direction of travel (falling back to the car's facing when
 * slow or reversing, and holding still while the car is flipped), leads the
 * framing ahead along the velocity, flattens the pitch a touch at speed, and
 * pulls back with speed (dynamic zoom). The pivot and velocity are pre-smoothed
 * for stability. Collision keeps it out of walls. Its `follow_offset` sets the
 * resting height/distance.
 */
class CameraStateCar : public CameraState {
private:
	Vector3 smoothed_pivot; ///< Low-passed target position (kills physics jitter).
	Vector3 smoothed_velocity; ///< Low-passed target velocity (steers auto-yaw).
	bool first_frame = true; ///< Seed the smoothed values on the first update.

public:
	/// Show the mouse, reset smoothing, re-seat the springs on entry.
	void enter(GameCamera *p_camera) override;
	/// Smooth the target, auto-frame behind it, then collide + smooth to place.
	void
	update(GameCamera *p_camera,
		   float p_delta) override;
};

} // namespace godot

#endif // CAMERA_STATE_CAR_H
