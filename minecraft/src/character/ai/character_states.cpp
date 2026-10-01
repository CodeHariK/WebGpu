#include "character_states.h"

#include "../spring_character.h"


#include <godot_cpp/core/math.hpp>

namespace godot {

// --- CharacterGroundedState: ground servo + upright/facing + planar movement ---
void CharacterGroundedState::physics_update(float delta) {
	character->_apply_ground_servo();
	character->_apply_upright_and_facing(character->_face_dir);
	character->_apply_movement(delta);
}

// --- CharacterAirborneState: upright/facing + air control + corner + gravity ---
void CharacterAirborneState::physics_update(float delta) {
	character->_apply_upright_and_facing(character->_face_dir);
	character->_apply_movement(delta);
	// Corner-correction (the sideways "collide and slide" nudge) is disabled in the air:
	// airborne motion stays purely ballistic, nothing shoves the body around edges.
	character->_apply_air_gravity(character->_in_jump_held, delta);
	character->_apply_landing_guard(delta); // don't overshoot the floor on a fast fall
}

// --- CharacterDashState ---
void CharacterDashState::enter() {
	Vector3 dir = character->_wish;
	if (dir.length() < 0.1f) {
		// No throttle: dash along the canonical heading (camera look / move-forward),
		// NOT the physics body's own basis — that lags and oscillates behind _face_yaw,
		// which made the dash fire off in the wrong direction.
		Basis hb(Vector3(0.0f, 1.0f, 0.0f), character->_face_yaw);
		dir = -hb.get_column(2); // heading forward (-Z)
	}
	dir.y = 0.0f;
	if (dir.length() > 0.001f) {
		dir = dir.normalized();
	}
	character->_dash_dir = dir;
	character->_dash_timer = character->dash_time;
}

void CharacterDashState::physics_update(float delta) {
	// Flat, gravity-free burst: crisp and readable.
	Vector3 v = character->_dash_dir * character->dash_speed;
	character->_vel = Vector3(v.x, 0.0f, v.z);
	character->_apply_upright_and_facing(character->_dash_dir);
}

// --- CharacterGroundPoundState ---
void CharacterGroundPoundState::enter() {
	character->_pound_hang_timer = character->pound_hang_time;
	character->_vel = Vector3(0.0f, 0.0f, 0.0f); // freeze for the hang
}

void CharacterGroundPoundState::physics_update(float delta) {
	if (character->_pound_hang_timer > 0.0f) {
		character->_vel = Vector3(0.0f, 0.0f, 0.0f); // hover
		character->_apply_upright_and_facing(Vector3(0.0f, 0.0f, 0.0f));
	} else {
		character->_vel = Vector3(0.0f, -character->pound_speed, 0.0f); // slam
	}
}

} // namespace godot
