#include "tennis_ball.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void TennisBall::_bind_methods() {
	ClassDB::bind_method(D_METHOD("hit", "direction", "speed", "type"), &TennisBall::hit);
	ClassDB::bind_method(D_METHOD("serve", "position", "direction", "speed"), &TennisBall::serve);
	ClassDB::bind_method(D_METHOD("reset", "position"), &TennisBall::reset);
	
	ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &TennisBall::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &TennisBall::get_velocity);
	
	ClassDB::bind_method(D_METHOD("set_state", "state"), &TennisBall::set_state);
	ClassDB::bind_method(D_METHOD("get_state"), &TennisBall::get_state);

	ClassDB::bind_method(D_METHOD("set_gravity", "gravity"), &TennisBall::set_gravity);
	ClassDB::bind_method(D_METHOD("get_gravity"), &TennisBall::get_gravity);

	BIND_ENUM_CONSTANT(SHOT_FLAT);
	BIND_ENUM_CONSTANT(SHOT_TOPSPIN);
	BIND_ENUM_CONSTANT(SHOT_SLICE);
	BIND_ENUM_CONSTANT(SHOT_LOB);
	BIND_ENUM_CONSTANT(SHOT_DROP);

	BIND_ENUM_CONSTANT(STATE_IDLE);
	BIND_ENUM_CONSTANT(STATE_SERVING);
	BIND_ENUM_CONSTANT(STATE_IN_PLAY);
	BIND_ENUM_CONSTANT(STATE_BOUNCED);
	BIND_ENUM_CONSTANT(STATE_DEAD);
}

TennisBall::TennisBall() {
	velocity = Vector3(0, 0, 0);
}

TennisBall::~TennisBall() {}

void TennisBall::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) return;
}

void TennisBall::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) return;
	if (state == STATE_IDLE) return;

	// Apply Gravity
	velocity.y -= gravity * delta;

	// Apply Drag (Air Resistance)
	velocity *= drag;

	// Move and Collide
	Ref<KinematicCollision3D> collision = move_and_collide(velocity * delta);

	if (collision.is_valid()) {
		Vector3 normal = collision->get_normal();
		
		// Bounce
		velocity = velocity.bounce(normal) * elasticity;

		// If hit the ground
		if (normal.y > 0.7f) {
			bounce_count++;
			state = STATE_BOUNCED;
			
			// If it bounces twice, it's dead
			if (bounce_count >= 2) {
				state = STATE_DEAD;
				velocity = Vector3(0, 0, 0);
			}
		}
	}
}

void TennisBall::hit(Vector3 p_direction, float p_speed, ShotType p_type) {
	bounce_count = 0;
	state = STATE_IN_PLAY;
	
	Vector3 base_dir = p_direction.normalized();
	float final_speed = p_speed;
	float vertical_bias = 0.0f;

	switch (p_type) {
		case SHOT_TOPSPIN:
			vertical_bias = 0.2f; // Slight upward arc that dips fast (high gravity later?)
			final_speed *= 1.2f;
			break;
		case SHOT_SLICE:
			vertical_bias = 0.05f;
			final_speed *= 0.8f;
			break;
		case SHOT_LOB:
			vertical_bias = 0.8f;
			final_speed *= 0.7f;
			break;
		case SHOT_DROP:
			vertical_bias = 0.1f;
			final_speed *= 0.4f;
			break;
		case SHOT_FLAT:
		default:
			vertical_bias = 0.1f;
			final_speed *= 1.4f;
			break;
	}

	velocity = (base_dir + Vector3(0, vertical_bias, 0)).normalized() * final_speed;
}

void TennisBall::serve(Vector3 p_position, Vector3 p_direction, float p_speed) {
	reset(p_position);
	state = STATE_SERVING;
	velocity = p_direction.normalized() * p_speed;
}

void TennisBall::reset(Vector3 p_position) {
	set_global_position(p_position);
	velocity = Vector3(0, 0, 0);
	bounce_count = 0;
	state = STATE_IDLE;
}

} // namespace godot
