#ifndef CAMERA_VERTICAL_HOLD_H
#define CAMERA_VERTICAL_HOLD_H

#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class Camera3D;
class Node3D;

/**
 * CameraVerticalHold — Odyssey-style "don't follow the jump", bounded by a SCREEN band.
 * ----------------------------------------------------------------------------------
 * The framing height follows the character only while it's on the ground. Take off and the
 * camera keeps the take-off height, so the world doesn't bob with every jump and you judge
 * the landing against a still ground.
 *
 * While airborne it only moves to keep the character's centre inside a vertical band of the
 * screen: between `band_top` and `band_bottom` (fractions of screen height, 0 = top edge).
 * Like Cinemachine's dead zone / soft zone: `soft` (a fraction of the screen) inside each edge,
 * the camera starts easing to keep her at that inner line, so it picks up speed gradually;
 * the band edges themselves are the hard limit she can never cross.
 * That limit is solved exactly from the camera's orientation and field of view, so it holds
 * at any speed (long falls), any distance (collision pull-in near walls), pitch or zoom, which
 * world-space metre margins can't do. On landing it eases to the new ground height.
 *
 * "Grounded" comes from the target's `is_grounded()` (Celeste, SpringCharacter); a target
 * without it is treated as always grounded (plain follow).
 */
class CameraVerticalHold {
public:
	/// Returns the framing height for this frame. `p_cam` is the camera as placed last frame
	/// (its orientation and FOV define the band); may be null (no screen limit).
	/// `p_target_velocity` lets the limit use where the target will be this frame (fast falls).
	float update(
			const Vector3 &p_target,
			const Vector3 &p_target_velocity,
			bool p_grounded,
			const Camera3D *p_cam,
			float p_band_top,
			float p_band_bottom,
			float p_soft,
			float p_dt
	);
	void reset(float p_y);
	static bool target_grounded(Node3D *p_target);

private:
	float anchor_y = 0.0f; ///< Height of the last ground contact.
	float frame_y = 0.0f; ///< Smoothed framing height (what the camera uses).
	bool has_camera_frame = false; ///< The camera has been placed from frame_y at least once.

	/// Framing heights that put the target exactly on the top / bottom screen lines.
	static bool _band_heights(
			const Camera3D *p_cam,
			const Vector3 &p_target,
			float p_placed_y,
			float p_top,
			float p_bottom,
			float &r_lowest,
			float &r_highest
	);
	/// How much the framing (= the camera) must rise for the target to sit at screen-NDC `p_ndc_y`.
	static bool _shift_for_ndc(
			const Camera3D *p_cam,
			const Vector3 &p_target,
			float p_ndc_y,
			float &r_shift
	);
};

} // namespace godot

#endif // CAMERA_VERTICAL_HOLD_H
