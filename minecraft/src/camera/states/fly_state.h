#ifndef CAMERA_STATE_FLY_H
#define CAMERA_STATE_FLY_H

#include "../camera_state.h"
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * CameraStateFly — free-flying debug / spectator camera, Blender-viewport style.
 * -----------------------------------------------------------------------------
 * Everything works around the point under the mouse (found by a raycast against the camera's
 * collision_mask), so speeds follow how far away things are:
 *   - orbit (right / middle drag): turns round the point that was under the mouse when the drag
 *     started; that point stays where it is on screen.
 *   - pan (Shift + middle drag): the grabbed point stays under the mouse — metres per pixel are
 *     worked out from its depth and the field of view, so far away pans fast and close up slowly.
 *   - zoom (wheel): moves toward / away from the point under the mouse by a fraction of the
 *     distance to it (zoom_speed scales the fraction), so it slows down as you get close and never
 *     goes through the surface.
 * When the mouse is over nothing (sky), the last known depth is used along the mouse ray.
 * No smoothing (like Blender): rotation and position follow the input exactly, so an orbit's
 * pivot stays put on screen.
 */
class CameraStateFly : public CameraState {
public:
	/// Show the mouse and re-seat the springs on the current transform.
	void enter(GameCamera *p_camera) override;
	/// Apply input, then smooth rotation and position toward the free-fly focus.
	void
	update(GameCamera *p_camera,
		   float p_delta) override;

private:
	bool was_dragging = false; ///< orbit / pan button held last frame (to catch the start of a drag)
	Vector3 pivot; ///< the point orbits turn round / pans hold under the mouse
	float depth = 10.0f; ///< metres to the last point found under the mouse (fallback for the sky)

	/// The point under the mouse (true) or, over the sky, the point `depth` along the mouse ray.
	bool point_under_mouse(
			GameCamera *p_camera,
			Vector3 &r_point
	) const;
	void orbit(
			GameCamera *p_camera,
			const Vector2 &p_look
	);
	void pan(
			GameCamera *p_camera,
			const Vector2 &p_look
	);
	void zoom(
			GameCamera *p_camera,
			float p_steps
	);
};

} // namespace godot

#endif // CAMERA_STATE_FLY_H
