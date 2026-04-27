#ifndef TENNIS_PLAYER_H
#define TENNIS_PLAYER_H

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/area3d.hpp>

namespace godot {

class TennisPlayer : public CharacterBody3D {
	GDCLASS(TennisPlayer, CharacterBody3D)

public:
	enum PlayerState {
		IDLE,
		MOVING,
		SWINGING,
		SERVING
	};

private:
	PlayerState current_state;
	float move_speed;
	float swing_timer;
	float swing_duration;
	
	float charge_power;
	bool is_charging;
	Vector2 aim_direction;
	
	Area3D *swing_hitbox;

protected:
	static void _bind_methods();

public:
	TennisPlayer();
	~TennisPlayer();

	void _ready() override;
	void _physics_process(double delta) override;

	void set_state(PlayerState p_state);
	PlayerState get_state() const { return current_state; }

	// Input handling or AI driver
	void move(const Vector2 &p_input_dir);
	void set_aim_direction(const Vector2 &p_aim_dir);
	
	void start_charge();
	void release_charge();
	void swing();
	
	// Called by GameManager or Ball
	void on_ball_entered_hitbox(Node3D *p_body);
	
	float get_charge_power() const { return charge_power; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TennisPlayer::PlayerState);

#endif // TENNIS_PLAYER_H
