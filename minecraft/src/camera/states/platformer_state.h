#ifndef CAMERA_STATE_PLATFORMER_H
#define CAMERA_STATE_PLATFORMER_H

#include "../vertical_hold.h"
#include "../camera_state.h"
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * CameraStatePlatformer — Mario Odyssey / A Hat in Time style FREE follow camera.
 * ------------------------------------------------------------------------------
 * Unlike MODE_CHARACTER (which locks the camera behind a steered heading), this
 * camera is decoupled from the character's facing. It pairs with the character's
 * CAMERA_RELATIVE control scheme: the stick is a direction in screen space, and
 * the character turns to follow it, while the camera makes its own framing choices.
 *
 *   - Soft auto-centre: once the character has been running for a moment, the
 *     yaw eases round behind the direction of travel (Odyssey's "swing behind").
 *     Idle holds the current view so the player can look around freely.
 *   - Manual orbit (drag / right stick) overrides the auto-centre and suspends it
 *     for a hold time so the camera never fights the player.
 *   - Raised look target + travel look-ahead so platforms / enemies ahead read.
 *   - Pitch eases back to a comfortable downward resting angle when hands-off.
 *   - Shared collision pull-in so walls never hide the character.
 */
class CameraStatePlatformer : public CameraState {
private:
	CameraVerticalHold vertical_hold; ///< Holds the framing height while the target is airborne.
	Vector3 smoothed_pivot; ///< Low-passed target position on the ground plane (x, z; y unused). Height: vertical_hold.
	Vector3 smoothed_velocity; ///< Low-passed target velocity (steers the auto-yaw).
	float recenter_suspend = 0.0f; ///< Seconds auto-centre stays paused after a manual orbit.
	float travel_time = 0.0f; ///< How long the character has been moving (gates auto-centre).
	bool first_frame = true; ///< Seed the smoothed values on the first update.

public:
	void enter(GameCamera *p_camera) override;
	void
	update(GameCamera *p_camera,
		   float p_delta) override;
};

} // namespace godot

#endif // CAMERA_STATE_PLATFORMER_H
