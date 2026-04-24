#ifndef PLAYER_INPUT_H
#define PLAYER_INPUT_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/input.hpp>

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
};

struct CharacterInput {
	Vector2 move_axis;
	bool jump = false;
	bool jump_just_pressed = false;
	bool kick = false;
	bool kick_just_pressed = false;
	bool grab = false;
	bool grab_just_pressed = false;
};

struct SystemInput {
	bool swap_target = false;
	bool swap_target_just_pressed = false;
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
};

class PlayerInput : public Object {
	GDCLASS(PlayerInput, Object)

private:
	static PlayerInput *singleton;
	ActionState current_state;

	// Accumulated deltas from events
	Vector2 accumulated_look;
	float accumulated_zoom = 0.0f;

	// Event-tracked states for camera synchronization
	bool is_mb_middle_down = false;
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
