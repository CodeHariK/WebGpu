#ifndef CHARACTER_CONTROLS_H
#define CHARACTER_CONTROLS_H

#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class GameCamera;

/**
 * CharacterControls — turns raw stick + mouse input into a facing and a move direction,
 * for either of the two on-foot control schemes, and keeps the paired camera in step.
 * ------------------------------------------------------------------------------------
 * Body-agnostic: it never moves anything. The owner reads get_face_yaw() and
 * get_move_dir() each frame and applies them however its body works (CharacterBody3D
 * velocity, RigidBody3D velocity, ...).
 *
 *   SCHEME_STEER: A/D (or the mouse, which takes priority) rotate the heading and W drives
 *     along it; the camera (MODE_CHARACTER) is locked behind the heading. S / S+A / S+D run
 *     TOWARD the camera instead of backpedalling, and the camera holds the yaw it had when
 *     S went down until the player drives forward again.
 *   SCHEME_CAMERA_RELATIVE: the stick is a screen-space direction off the camera's ground
 *     plane and facing eases toward travel; the camera is the free MODE_PLATFORMER.
 *
 * Raw steer input accumulates into a TARGET heading that the real heading eases toward,
 * which filters bursty integer mouse pixels into a smooth turn.
 */
class CharacterControls {
public:
	enum Scheme {
		SCHEME_STEER = 0,
		SCHEME_CAMERA_RELATIVE = 1
	};

	// --- Tunables (the owner exposes / saves these) ---
	float steer_rate = 3.2f; ///< Keyboard heading turn speed (rad/s).
	float face_turn_rate = 14.0f; ///< Heading easing rate (1/s); higher = snappier.

private:
	Scheme scheme = SCHEME_STEER;
	float face_yaw = 0.0f; ///< Current (eased) heading; forward = (-sin, 0, -cos).
	float steer_target = 0.0f; ///< Steer mode: heading the input is asking for.
	bool target_valid = false;
	bool hold_cam = false; ///< Camera frozen after a backward run, until W.
	float back_cam_yaw = 0.0f; ///< The frozen camera yaw.
	Vector3 move_dir; ///< This frame's unit move direction (zero = no travel).

	void _update_steer(
			const Vector2 &p_axis,
			float p_mouse_x,
			GameCamera *p_cam,
			float p_dt
	);
	void _update_camera_relative(
			const Vector2 &p_axis,
			GameCamera *p_cam,
			float p_dt
	);
	void _ease_toward(
			float p_target,
			float p_dt
	);

public:
	/// Start from a known heading (e.g. the body's yaw at spawn).
	void seed(float p_yaw);

	/// Run once per physics frame while the owner is the active target. `p_mouse_x` is the
	/// frame's horizontal look delta (pixels); `p_cam` may be null.
	void update(
			const Vector2 &p_axis,
			float p_mouse_x,
			GameCamera *p_cam,
			float p_dt
	);
	/// No input this frame (owner inactive): keep facing, stop travel.
	void idle() { move_dir = Vector3(); }

	/// Select the camera mode for the scheme and point it (call after update()).
	void apply_camera(GameCamera *p_cam) const;

	void set_scheme(Scheme p_scheme);
	Scheme get_scheme() const { return scheme; }
	void toggle_scheme() { set_scheme(scheme == SCHEME_STEER ? SCHEME_CAMERA_RELATIVE : SCHEME_STEER); }

	float get_face_yaw() const { return face_yaw; }
	Vector3 get_move_dir() const { return move_dir; }
	bool has_move() const { return move_dir.length_squared() > 0.01f; }
};

} // namespace godot

#endif // CHARACTER_CONTROLS_H
