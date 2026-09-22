#ifndef CAMERA_STATE_TPS_H
#define CAMERA_STATE_TPS_H

#include "../camera_state.h"

namespace godot {

/**
 * CameraStateTPS — over-the-shoulder third-person camera.
 * -------------------------------------------------------
 * Captures the mouse and always steers the look from it (no orbit button). The
 * camera orbits the follow target at `target_distance` along `follow_offset`'s
 * direction, so a lateral component in the offset gives a shoulder view. Uses
 * the shared collision resolve (absolute metres, same units as the car cam) so
 * switching between TPS and car never jumps. Suited to on-foot control.
 */
class CameraStateTPS : public CameraState {
public:
	/// Capture the mouse and re-seat the springs on the current transform.
	virtual void enter(GameCamera *p_camera) override;
	/// Steer look from the mouse, then orbit + collide + smooth to the target.
	virtual void
	update(GameCamera *p_camera,
		   float p_delta) override;
};

} // namespace godot

#endif // CAMERA_STATE_TPS_H
