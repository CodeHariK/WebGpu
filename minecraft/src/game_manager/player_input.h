#ifndef PLAYER_INPUT_H
#define PLAYER_INPUT_H

#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace godot {

struct CameraInput {
	Vector2 look_delta;
	float zoom_delta = 0.0f;
	bool is_orbiting = false;
	bool is_panning = false;
};

struct VehicleInput {
	float throttle = 0.0f;
	float brake = 0.0f;
	float steering = 0.0f;
	bool handbrake = false;
	bool nitro = false;
	bool glide = false;
};

struct CharacterInput {
	Vector2 move_axis;
	bool jump = false;
	bool jump_just_pressed = false;
	bool kick = false;
	bool kick_just_pressed = false;
	bool dash = false;
	bool dash_just_pressed = false;
	bool grab = false;
	bool grab_just_pressed = false;
	bool interact = false;
	bool interact_just_pressed = false;
	bool bow = false; ///< Held: draw the bow (release fires).
};

struct SystemInput {
	bool swap_target = false;
	bool swap_target_just_pressed = false;
	bool toggle_recipe_editor = false;
	bool toggle_recipe_editor_just_pressed = false;
};

struct TennisInput {
	bool shot_a = false;
	bool shot_a_just_pressed = false;
	bool shot_b = false;
	bool shot_b_just_pressed = false;
	bool shot_y = false;
	bool shot_y_just_pressed = false;
	bool shot_x = false;
	bool shot_x_just_pressed = false;
};

/**
 * Centralized Input State for the player.
 * Decouples game logic from raw hardware keys.
 */
struct ActionState {
	CameraInput camera;
	VehicleInput vehicle;
	CharacterInput character;
	SystemInput system;
	TennisInput tennis;
};

class PlayerInput : public Object {
	GDCLASS(PlayerInput,
			Object)

private:
	static PlayerInput *singleton;
	ActionState current_state;
	bool jump_locked = false;

	// Accumulated deltas from events
	Vector2 accumulated_look;
	/// Full deflection of the look axis (right virtual joystick / gamepad right stick) is
	/// treated as moving the mouse this many pixels per second, so every camera mode
	/// consumes it through look_delta unchanged.
	float look_axis_speed = 600.0f;
	float accumulated_zoom = 0.0f;

	// Event-tracked states for camera synchronization
	bool is_mb_middle_down = false;
	bool is_mb_right_down = false; // right-drag also orbits (Mac trackpad: two-finger click)
	bool is_shift_down = false;

	// Platform-specific strength handlers
	typedef float (PlayerInput::*StrengthHandler)(float) const;
	StrengthHandler _current_strength_handler = nullptr;

	float _get_strength_desktop(float p_sprint_multiplier) const;
	float _get_strength_mobile(float p_sprint_multiplier) const;

protected:
	static void _bind_methods();

public:
	PlayerInput();
	~PlayerInput();

	static PlayerInput *get_singleton();

	void update(); // Called by GameManager every frame
	/// While locked the character jump inputs read as released (e.g. top-down puzzle rooms).
	void set_jump_locked(bool p_locked) { jump_locked = p_locked; }
	bool is_jump_locked() const { return jump_locked; }
	void handle_input(const Ref<class InputEvent> &p_event);

	const ActionState &get_state() const { return current_state; }

	// Convenience helpers
	Vector2 get_move_axis() const { return current_state.character.move_axis; }
	bool is_jumping() const { return current_state.character.jump; }
	bool is_jump_just_pressed() const { return current_state.character.jump_just_pressed; }
	bool is_kicking() const { return current_state.character.kick; }

	float get_movement_strength(float p_sprint_multiplier) const;
};

} // namespace godot

#endif // PLAYER_INPUT_H
