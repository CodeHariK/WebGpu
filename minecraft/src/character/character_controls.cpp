#include "character_controls.h"

#include "../camera/camera.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

namespace godot {

static const float BACK_THRESHOLD = 0.3f; // stick pulled back this far = "run toward the camera"
static const float FORWARD_THRESHOLD = 0.1f; // stick pushed forward this far = drive / release the held camera

static Vector3 yaw_forward(float p_yaw) {
	return Vector3(-std::sin(p_yaw), 0.0f, -std::cos(p_yaw));
}

static Vector3 yaw_right(float p_yaw) {
	return Vector3(std::cos(p_yaw), 0.0f, -std::sin(p_yaw));
}

void CharacterControls::seed(float p_yaw) {
	face_yaw = p_yaw;
	steer_target = p_yaw;
	target_valid = true;
	hold_cam = false;
	move_dir = Vector3();
}

void CharacterControls::set_scheme(Scheme p_scheme) {
	scheme = p_scheme;
	target_valid = false; // re-seed the steer target from the live heading on entry
	hold_cam = false;
}

void CharacterControls::_ease_toward(
		float p_target,
		float p_dt
) {
	float diff = UtilityFunctions::wrapf(p_target - face_yaw, -(float)Math::PI, (float)Math::PI);
	face_yaw += diff * (1.0f - std::exp(-face_turn_rate * p_dt));
	face_yaw = UtilityFunctions::wrapf(face_yaw, -(float)Math::PI, (float)Math::PI);
}

void CharacterControls::update(
		const Vector2 &p_axis,
		float p_mouse_x,
		GameCamera *p_cam,
		float p_dt
) {
	if (scheme == SCHEME_STEER) {
		_update_steer(p_axis, p_mouse_x, p_cam, p_dt);
	} else {
		_update_camera_relative(p_axis, p_cam, p_dt);
	}
}

void CharacterControls::_update_steer(
		const Vector2 &p_axis,
		float p_mouse_x,
		GameCamera *p_cam,
		float p_dt
) {
	if (!target_valid) {
		steer_target = face_yaw;
		target_valid = true;
	}
	bool mouse_moved = std::abs(p_mouse_x) > 0.01f && p_cam;
	bool backing = p_axis.y > BACK_THRESHOLD; // S held
	bool forward = p_axis.y < -FORWARD_THRESHOLD; // W held

	if (backing && !hold_cam) {
		back_cam_yaw = p_cam ? p_cam->get_yaw() : face_yaw; // freeze the view where it is
	}
	if (backing) {
		hold_cam = true;
	} else if (forward) {
		hold_cam = false; // only driving forward recenters the camera
	}

	Vector3 back_dir;
	if (backing) {
		if (mouse_moved) {
			back_cam_yaw -= p_mouse_x * p_cam->get_orbit_sensitivity(); // mouse still looks around
		}
		// Camera-relative off the FROZEN camera yaw: S = toward the camera, A/D = diagonals.
		back_dir = yaw_right(back_cam_yaw) * p_axis.x + yaw_forward(back_cam_yaw) * (-p_axis.y);
		if (back_dir.length() > 0.001f) {
			back_dir = back_dir.normalized();
			steer_target = std::atan2(-back_dir.x, -back_dir.z); // face where we run
		}
	} else if (mouse_moved && hold_cam) {
		back_cam_yaw -= p_mouse_x * p_cam->get_orbit_sensitivity(); // camera held: mouse just looks
	} else if (mouse_moved) {
		steer_target -= p_mouse_x * p_cam->get_orbit_sensitivity(); // mouse steers, beats A/D
	} else {
		steer_target -= p_axis.x * steer_rate * p_dt; // keys: right = clockwise
	}
	steer_target = UtilityFunctions::wrapf(steer_target, -(float)Math::PI, (float)Math::PI);
	_ease_toward(steer_target, p_dt);

	if (backing) {
		move_dir = back_dir;
	} else if (forward) {
		move_dir = yaw_forward(face_yaw);
	} else {
		move_dir = Vector3();
	}
}

void CharacterControls::_update_camera_relative(
		const Vector2 &p_axis,
		GameCamera *p_cam,
		float p_dt
) {
	target_valid = false; // switching back to steer re-seeds from the live heading
	hold_cam = false;
	float cam_yaw = p_cam ? p_cam->get_yaw() : 0.0f;
	Vector3 dir = yaw_right(cam_yaw) * p_axis.x + yaw_forward(cam_yaw) * (-p_axis.y);
	if (dir.length() > 0.1f) {
		dir = dir.normalized();
		_ease_toward(std::atan2(-dir.x, -dir.z), p_dt);
		move_dir = dir;
	} else {
		move_dir = Vector3();
	}
}

void CharacterControls::apply_camera(GameCamera *p_cam) const {
	if (!p_cam) {
		return;
	}
	if (scheme == SCHEME_STEER) {
		p_cam->set_camera_mode(GameCamera::MODE_CHARACTER);
		p_cam->set_heading_follow(true, hold_cam ? back_cam_yaw : face_yaw);
	} else {
		p_cam->set_camera_mode(GameCamera::MODE_PLATFORMER);
		p_cam->set_heading_follow(false, 0.0f);
	}
}

} // namespace godot
