#ifndef TENNIS_PLAYER_H
#define TENNIS_PLAYER_H

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/area3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include "tennis_types.h"

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

	Area3D *swing_area = nullptr;
	TennisBall *target_ball = nullptr;

	void _handle_movement(double delta);
	void _handle_swing();

protected:
	static void _bind_methods();

public:
	TennisPlayer();
	~TennisPlayer();

	void _ready() override;
	void _physics_process(double delta) override;

	void _on_swing_area_body_entered(Node *p_body);
	void _on_swing_area_body_exited(Node *p_body);

	// Getters/Setters
	void set_move_speed(float p_speed) { move_speed = p_speed; }
	float get_move_speed() const { return move_speed; }
};

} // namespace godot

#endif // TENNIS_PLAYER_H
