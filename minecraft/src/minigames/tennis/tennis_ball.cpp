#include "tennis_ball.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/kinematic_collision3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void TennisBall::_bind_methods() {
	ClassDB::bind_method(D_METHOD("hit", "target_pos", "flight_time", "spin"), &TennisBall::hit);
	ClassDB::bind_method(D_METHOD("serve", "target_pos", "flight_time"), &TennisBall::serve);
	ClassDB::bind_method(D_METHOD("reset_ball", "pos"), &TennisBall::reset_ball);
	ClassDB::bind_method(D_METHOD("get_is_active"), &TennisBall::get_is_active);
	ClassDB::bind_method(D_METHOD("get_bounce_count"), &TennisBall::get_bounce_count);
    
	BIND_ENUM_CONSTANT(FLAT);
	BIND_ENUM_CONSTANT(TOPSPIN);
	BIND_ENUM_CONSTANT(SLICE);
	BIND_ENUM_CONSTANT(LOB);
}

TennisBall::TennisBall() {
	current_velocity = Vector3(0, 0, 0);
	is_active = false;
	bounce_count = 0;
	current_time = 0.0f;
	flight_time = 1.0f;
	current_spin = FLAT;
}

TennisBall::~TennisBall() {
}

void TennisBall::_physics_process(double delta) {
	if (!is_active) {
		return;
	}

	// Simple kinematic trajectory: horizontal movement is linear (for now), vertical has gravity.
	// We can add Magnus effect based on spin later.
	current_time += delta;
	
	// Gravity calculation
	float gravity = 9.8f * 2.0f; // Exaggerated gravity for arcade feel
	
	// Update velocity
	current_velocity.y -= gravity * delta;
	
	Ref<KinematicCollision3D> collision = move_and_collide(current_velocity * delta);
	
	if (collision.is_valid()) {
		// Handle bounce
		Vector3 normal = collision->get_normal();
		if (normal.y > 0.5f) { // Floor bounce
			bounce_count++;
			current_velocity.y = Math::abs(current_velocity.y) * 0.7f; // Dampen bounce
			current_velocity.x *= 0.8f; // Friction
			current_velocity.z *= 0.8f;
		} else {
			// Wall/Net bounce
			current_velocity = current_velocity.bounce(normal) * 0.5f;
		}
	}
}

void TennisBall::hit(const Vector3 &p_target_pos, float p_flight_time, SpinType p_spin) {
	start_pos = get_global_position();
	target_pos = p_target_pos;
	flight_time = p_flight_time;
	current_time = 0.0f;
	current_spin = p_spin;
	bounce_count = 0;
	is_active = true;

	// Calculate initial velocity required to hit target_pos in flight_time
	// d = v0 * t + 0.5 * a * t^2
	// v0 = (d - 0.5 * a * t^2) / t
	
	float gravity = 9.8f * 2.0f;
	Vector3 displacement = target_pos - start_pos;
	
	current_velocity.x = displacement.x / flight_time;
	current_velocity.z = displacement.z / flight_time;
	current_velocity.y = (displacement.y - 0.5f * (-gravity) * flight_time * flight_time) / flight_time;
}

void TennisBall::serve(const Vector3 &p_target_pos, float p_flight_time) {
	hit(p_target_pos, p_flight_time, FLAT);
}

void TennisBall::reset_ball(const Vector3 &p_pos) {
	set_global_position(p_pos);
	current_velocity = Vector3(0, 0, 0);
	is_active = false;
	bounce_count = 0;
}

} // namespace godot
