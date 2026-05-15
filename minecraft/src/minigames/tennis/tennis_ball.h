#ifndef TENNIS_BALL_H
#define TENNIS_BALL_H

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/kinematic_collision3d.hpp>
#include "tennis_types.h"

namespace godot {

class TennisBall : public CharacterBody3D {
	GDCLASS(TennisBall, CharacterBody3D)

public:
	enum ShotType {
		SHOT_FLAT,
		SHOT_TOPSPIN,
		SHOT_SLICE,
		SHOT_LOB,
		SHOT_DROP
	};

	enum BallState {
		STATE_IDLE,
		STATE_SERVING,
		STATE_IN_PLAY,
		STATE_BOUNCED,
		STATE_DEAD
	};

private:
	Vector3 velocity;
	float gravity = 18.0f;
	float drag = 0.995f;
	float elasticity = 0.85f;
	BallState state = STATE_IDLE;

	// Shot parameters
	float last_bounce_time = 0.0f;
	int bounce_count = 0;

protected:
	static void _bind_methods();

public:
	TennisBall();
	~TennisBall();

	void _ready() override;
	void _physics_process(double delta) override;

	void hit(Vector3 p_direction, float p_speed, ShotType p_type);
	void serve(Vector3 p_position, Vector3 p_direction, float p_speed);
	void reset(Vector3 p_position);

	// Getters/Setters
	void set_velocity(const Vector3 &p_vel) { velocity = p_vel; }
	Vector3 get_velocity() const { return velocity; }

	void set_state(BallState p_state) { state = p_state; }
	BallState get_state() const { return state; }
	
	void set_gravity(float p_gravity) { gravity = p_gravity; }
	float get_gravity() const { return gravity; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TennisBall::ShotType);
VARIANT_ENUM_CAST(godot::TennisBall::BallState);

#endif // TENNIS_BALL_H
