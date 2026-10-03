#include "vertical_hold.h"

#include "../utils/spring/stateful_spring.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

namespace godot {

static const float GROUND_RATE = 10.0f; // following steps / slopes while grounded (e-folds/s)
static const float AIR_RATE = 8.0f; // drifting back toward the take-off height in the air
static const float LAND_RATE = 5.0f; // easing to a new ground height after landing
static const float SOFT_RATE = 6.0f; // soft zone: how fast the camera eases her back to the inner line

void CameraVerticalHold::reset(float p_y) {
	anchor_y = p_y;
	frame_y = p_y;
	has_camera_frame = false;
}

bool CameraVerticalHold::target_grounded(Node3D *p_target) {
	if (!p_target || !p_target->has_method("is_grounded")) {
		return true;
	}
	return bool(p_target->call("is_grounded"));
}

// Moving the framing height by s moves the camera straight up by s (the orbit offset is
// fixed), which shifts the target in camera space by -s * (world up in camera axes).
// Perspective: ndc_y = ly / (-lz * tan(fov/2)). Solving ndc_y(s) = n for s is linear:
//     s = (ly + n*t*lz) / (Yy + n*t*Zy)      t = tan(fov/2), Y/Z = camera axes' world-up parts
bool CameraVerticalHold::_shift_for_ndc(
		const Camera3D *p_cam,
		const Vector3 &p_target,
		float p_ndc_y,
		float &r_shift
) {
	Transform3D xf = p_cam->get_global_transform();
	Vector3 axis_y = xf.basis.get_column(1);
	Vector3 axis_z = xf.basis.get_column(2);
	Vector3 d = p_target - xf.origin;
	float ly = d.dot(axis_y);
	float lz = d.dot(axis_z);
	if (lz > -0.1f) {
		return false; // target beside / behind the camera: no meaningful screen position
	}
	float t = std::tan(Math::deg_to_rad((float)p_cam->get_fov()) * 0.5f);
	float denom = axis_y.y + p_ndc_y * t * axis_z.y;
	if (denom < 0.05f) {
		return false; // camera looking (almost) straight up/down: moving it up barely helps
	}
	r_shift = (ly + p_ndc_y * t * lz) / denom;
	return true;
}

bool CameraVerticalHold::_band_heights(
		const Camera3D *p_cam,
		const Vector3 &p_target,
		float p_placed_y,
		float p_top,
		float p_bottom,
		float &r_lowest,
		float &r_highest
) {
	float rise_to_top = 0.0f;
	float rise_to_bottom = 0.0f;
	if (!_shift_for_ndc(p_cam, p_target, 1.0f - 2.0f * p_top, rise_to_top) ||
		!_shift_for_ndc(p_cam, p_target, 1.0f - 2.0f * p_bottom, rise_to_bottom)) {
		return false;
	}
	r_lowest = p_placed_y + rise_to_top;
	r_highest = p_placed_y + rise_to_bottom;
	return r_lowest <= r_highest;
}

float CameraVerticalHold::update(
		const Vector3 &p_target,
		const Vector3 &p_target_velocity,
		bool p_grounded,
		const Camera3D *p_cam,
		float p_band_top,
		float p_band_bottom,
		float p_soft,
		float p_dt
) {
	float placed_y = frame_y; // the height the camera was placed from last frame
	float desired = anchor_y;
	float rate = AIR_RATE;
	if (p_grounded) {
		bool landed_elsewhere = std::abs(p_target.y - anchor_y) > 0.5f;
		anchor_y = p_target.y;
		desired = p_target.y;
		rate = landed_elsewhere ? LAND_RATE : GROUND_RATE;
	}
	frame_y += (desired - frame_y) * spring_damp_factor(rate, p_dt);

	// Screen fraction f (0 = top) -> ndc y = 1 - 2f. Raising the camera moves the target DOWN
	// the screen, so a TOP line gives the lowest allowed framing and a BOTTOM line the highest.
	if (p_cam && has_camera_frame) {
		// Where the target will be once it moves this frame: at 48 m/s that's 0.8 m, enough
		// to sit visibly outside the band if we solved against the old position.
		Vector3 ahead = p_target + p_target_velocity * p_dt;
		float lowest = 0.0f;
		float highest = 0.0f;
		if (_band_heights(p_cam, ahead, placed_y, p_band_top, p_band_bottom, lowest, highest)) {
			// Soft zone (airborne only; on the ground the framing just follows her): once she's
			// within `soft` of an edge, ease toward keeping her on the inner line.
			float soft_low = 0.0f;
			float soft_high = 0.0f;
			if (!p_grounded && p_soft > 0.0f &&
				_band_heights(p_cam, ahead, placed_y, p_band_top + p_soft, p_band_bottom - p_soft, soft_low, soft_high)) {
				float k = spring_damp_factor(SOFT_RATE, p_dt);
				if (frame_y < soft_low) {
					frame_y += (soft_low - frame_y) * k;
				} else if (frame_y > soft_high) {
					frame_y += (soft_high - frame_y) * k;
				}
			}
			// Hard limit: never let her centre leave the band, at any speed.
			frame_y = CLAMP(frame_y, lowest, highest);
		}
	}
	has_camera_frame = true;
	return frame_y;
}

} // namespace godot
