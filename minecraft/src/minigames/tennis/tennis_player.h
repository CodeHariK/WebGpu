#ifndef TENNIS_PLAYER_H
#define TENNIS_PLAYER_H

#include "tennis_types.h"
#include <godot_cpp/classes/area3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/character_body3d.hpp>

namespace godot {

class TennisBall;
class PlayerInput;

class TennisPlayer : public CharacterBody3D {
	GDCLASS(TennisPlayer, CharacterBody3D)

private:
	PlayerInput *player_input = nullptr;
	float move_speed = 10.0f;
	float acceleration = 60.0f;
	float friction = 10.0f;
	float hitting_speed = 30.0f;

protected:
	void _handle_movement(double delta);
	void _handle_swing();

	static void _bind_methods();

public:
	TennisPlayer();
	~TennisPlayer();

	void _ready() override;
	void _physics_process(double delta) override;

	// Getters/Setters
	void set_move_speed(float p_speed) { move_speed = p_speed; }
	float get_move_speed() const { return move_speed; }

	void set_hitting_speed(float p_speed) { hitting_speed = p_speed; }
	float get_hitting_speed() const { return hitting_speed; }
};

} // namespace godot

#endif // TENNIS_PLAYER_H
