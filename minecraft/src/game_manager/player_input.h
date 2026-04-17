#ifndef PLAYER_INPUT_H
#define PLAYER_INPUT_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/input.hpp>

namespace godot {

/**
 * Centralized Input State for the player.
 * Decouples game logic from raw hardware keys.
 */
struct ActionState {
	// Movement
	Vector2 move_axis;
	bool jump = false;
	bool jump_just_pressed = false;
	
	// Driving
	float throttle = 0.0f;
	float brake = 0.0f;
	float steering = 0.0f;
	bool handbrake = false;
	
	// Combat
	bool kick = false;
	bool kick_just_pressed = false;
	
	// Systems
	bool swap_target = false;
	bool swap_target_just_pressed = false;
};

class PlayerInput : public Object {
	GDCLASS(PlayerInput, Object)

private:
	static PlayerInput *singleton;
	ActionState current_state;

	// Internal state trackers for "Just Pressed" logic
	bool prev_jump = false;
	bool prev_kick = false;
	bool prev_swap = false;

protected:
	static void _bind_methods();

public:
	PlayerInput();
	~PlayerInput();

	static PlayerInput *get_singleton();

	void update(); // Called by GameManager every frame
	
	const ActionState &get_state() const { return current_state; }
	
	// Convenience helpers
	Vector2 get_move_axis() const { return current_state.move_axis; }
	bool is_jumping() const { return current_state.jump; }
	bool is_jump_just_pressed() const { return current_state.jump_just_pressed; }
	bool is_kicking() const { return current_state.kick; }
};

} // namespace godot

#endif // PLAYER_INPUT_H
