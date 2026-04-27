#include "tennis_player.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void TennisPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_state", "state"), &TennisPlayer::set_state);
	ClassDB::bind_method(D_METHOD("get_state"), &TennisPlayer::get_state);
	ClassDB::bind_method(D_METHOD("move", "input_dir"), &TennisPlayer::move);
	ClassDB::bind_method(D_METHOD("set_aim_direction", "aim_dir"), &TennisPlayer::set_aim_direction);
	ClassDB::bind_method(D_METHOD("start_charge"), &TennisPlayer::start_charge);
	ClassDB::bind_method(D_METHOD("release_charge"), &TennisPlayer::release_charge);
	ClassDB::bind_method(D_METHOD("get_charge_power"), &TennisPlayer::get_charge_power);
	ClassDB::bind_method(D_METHOD("swing"), &TennisPlayer::swing);
	ClassDB::bind_method(D_METHOD("on_ball_entered_hitbox", "body"), &TennisPlayer::on_ball_entered_hitbox);

	BIND_ENUM_CONSTANT(IDLE);
	BIND_ENUM_CONSTANT(MOVING);
	BIND_ENUM_CONSTANT(SWINGING);
	BIND_ENUM_CONSTANT(SERVING);
}

TennisPlayer::TennisPlayer() {
	current_state = IDLE;
	move_speed = 8.0f;
	swing_timer = 0.0f;
	swing_duration = 0.3f;
	swing_hitbox = nullptr;
	charge_power = 0.0f;
	is_charging = false;
	aim_direction = Vector2(0, 0);
}

TennisPlayer::~TennisPlayer() {
}

void TennisPlayer::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	// Assuming the player has an Area3D child named "SwingHitbox"
	Node *node = get_node_or_null("SwingHitbox");
	if (node != nullptr) {
		swing_hitbox = Object::cast_to<Area3D>(node);
		if (swing_hitbox) {
			swing_hitbox->connect("body_entered", Callable(this, "on_ball_entered_hitbox"));
		}
	} else {
		UtilityFunctions::print("Warning: TennisPlayer requires an Area3D child named 'SwingHitbox'");
	}
}

void TennisPlayer::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (current_state == SWINGING) {
		swing_timer -= delta;
		if (swing_timer <= 0.0f) {
			set_state(IDLE);
		}
	}
	
	if (is_charging) {
		charge_power += delta * 2.0f; // Max power in 0.5 seconds
		if (charge_power > 1.0f) {
			charge_power = 1.0f;
		}
	}
}

void TennisPlayer::set_state(PlayerState p_state) {
	current_state = p_state;
}

void TennisPlayer::move(const Vector2 &p_input_dir) {
	if (current_state == SWINGING || current_state == SERVING) {
		return; // Cannot move while swinging or serving
	}
	
	if (is_charging) {
		// Can move slowly while charging
		move_speed = 3.0f;
	} else {
		move_speed = 8.0f;
	}

	Vector3 velocity = get_velocity();
	
	if (p_input_dir.length() > 0.01f) {
		set_state(MOVING);
		Vector3 move_dir(p_input_dir.x, 0, p_input_dir.y);
		move_dir = move_dir.normalized();
		velocity.x = move_dir.x * move_speed;
		velocity.z = move_dir.z * move_speed;
	} else {
		set_state(IDLE);
		velocity.x = 0;
		velocity.z = 0;
	}

	set_velocity(velocity);
	move_and_slide();
}

void TennisPlayer::set_aim_direction(const Vector2 &p_aim_dir) {
	aim_direction = p_aim_dir;
}

void TennisPlayer::start_charge() {
	if (current_state == SWINGING) return;
	is_charging = true;
	charge_power = 0.0f;
}

void TennisPlayer::release_charge() {
	if (!is_charging) return;
	is_charging = false;
	swing();
}

void TennisPlayer::swing() {
	if (current_state == SWINGING) {
		return;
	}
	
	set_state(SWINGING);
	swing_timer = swing_duration;
	
	// The actual hit logic happens in on_ball_entered_hitbox if the ball is inside
	// or we manually check overlapping bodies here.
	if (swing_hitbox) {
		TypedArray<Node3D> bodies = swing_hitbox->get_overlapping_bodies();
		for (int i = 0; i < bodies.size(); i++) {
			Node3D *body = Object::cast_to<Node3D>(bodies[i]);
			on_ball_entered_hitbox(body);
		}
	}
}

void TennisPlayer::on_ball_entered_hitbox(Node3D *p_body) {
	if (current_state != SWINGING) {
		return; // Only hit if we are actively swinging
	}

	// Check if body is a TennisBall (assuming we can call hit on it via GDScript or cast)
	// For now, we will rely on GDScript side to route the hit, or we can use call()
	if (p_body->has_method("hit")) {
		// Calculate target position based on aim direction and charge power
		float flight_time = 1.2f - (charge_power * 0.4f); // Faster if charged
		
		// If AI is hitting (aim_direction is default 0,0 usually unless programmed)
		// Or player is hitting (uses aim_direction from input)
		float z_dir = (get_global_position().z > 0) ? -20.0f : 20.0f; // Hit to opposite side
		float x_dir = aim_direction.x * 8.0f; // Aim left/right
		
		// If aiming down, maybe hit a drop shot? 
		// For simple tennis: aim up/down adjusts depth
		float depth_adjust = aim_direction.y * -5.0f;
		
		Vector3 target_pos = Vector3(x_dir, 0, z_dir + depth_adjust);
		
		p_body->call("hit", target_pos, flight_time, 0); // 0 is FLAT
		
		UtilityFunctions::print("Player hit the ball! Power: ", charge_power, " Aim: ", aim_direction);
	}
}

} // namespace godot
