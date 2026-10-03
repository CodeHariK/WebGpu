#ifndef CAMERA_STATE_CHARACTER_H
#define CAMERA_STATE_CHARACTER_H

#include "../vertical_hold.h"
#include "../camera_state.h"
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * CameraStateCharacter — Odyssey / A Hat in Time follow camera for an on-foot
 * character. Modeled on the car cam but tuned for platforming:
 *   - gentle auto-centre behind the direction of travel; holds still when idle
 *     (never snaps to the character's facing like the car does),
 *   - a raised look target + small look-ahead so platforms and enemies ahead read,
 *   - a comfortable downward pitch that eases back when hands-off,
 *   - wall-collision pull-in (shared helper) so the environment stays visible,
 *   - no speed-FOV / speed-lines.
 * Manual drag orbits the camera and briefly suspends the auto-centre.
 */
class CameraStateCharacter : public CameraState {
private:
	CameraVerticalHold vertical_hold; ///< Holds the framing height while the target is airborne.
	Vector3 smoothed_pivot; ///< Low-passed target position on the ground plane (x, z; y unused). Height: vertical_hold.
	Vector3 smoothed_velocity; ///< Low-passed target velocity (steers auto-yaw).
	float recenter_suspend = 0.0f; ///< Seconds left where auto-centre is paused after a manual orbit.
	bool first_frame = true; ///< Seed the smoothed values on the first update.

public:
	void enter(GameCamera *p_camera) override;
	void
	update(GameCamera *p_camera,
		   float p_delta) override;
};

} // namespace godot

#endif // CAMERA_STATE_CHARACTER_H
