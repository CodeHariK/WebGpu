#include "physics_character.h"
#include "../debug_draw/debug_quad.h"
#include "../game_manager/game_manager.h"
#include "../utils/raycast/mc_raycast.h"
#include "states/airborne_states.h"
#include "states/combat_states.h"
#include "states/grounded_states.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void PhysicsCharacter3D::_bind_methods() {
	{
		ClassDB::bind_method(D_METHOD("set_ride_height", "p_val"), &PhysicsCharacter3D::set_ride_height);
		ClassDB::bind_method(D_METHOD("get_ride_height"), &PhysicsCharacter3D::get_ride_height);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ride_height"), "set_ride_height", "get_ride_height");

		ClassDB::bind_method(D_METHOD("set_spring_stiffness", "p_val"), &PhysicsCharacter3D::set_spring_stiffness);
		ClassDB::bind_method(D_METHOD("get_spring_stiffness"), &PhysicsCharacter3D::get_spring_stiffness);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spring_stiffness"), "set_spring_stiffness", "get_spring_stiffness");

		ClassDB::bind_method(D_METHOD("set_spring_damping", "p_val"), &PhysicsCharacter3D::set_spring_damping);
		ClassDB::bind_method(D_METHOD("get_spring_damping"), &PhysicsCharacter3D::get_spring_damping);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spring_damping"), "set_spring_damping", "get_spring_damping");
	}

	{
		ClassDB::bind_method(D_METHOD("set_acceleration", "p_val"), &PhysicsCharacter3D::set_acceleration);
		ClassDB::bind_method(D_METHOD("get_acceleration"), &PhysicsCharacter3D::get_acceleration);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "acceleration"), "set_acceleration", "get_acceleration");

		ClassDB::bind_method(D_METHOD("set_max_speed", "p_val"), &PhysicsCharacter3D::set_max_speed);
		ClassDB::bind_method(D_METHOD("get_max_speed"), &PhysicsCharacter3D::get_max_speed);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_speed"), "set_max_speed", "get_max_speed");

		ClassDB::bind_method(D_METHOD("set_jump_impulse", "p_val"), &PhysicsCharacter3D::set_jump_impulse);
		ClassDB::bind_method(D_METHOD("get_jump_impulse"), &PhysicsCharacter3D::get_jump_impulse);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_impulse"), "set_jump_impulse", "get_jump_impulse");
	}

	{
		ClassDB::bind_method(D_METHOD("set_kick_speed", "p_val"), &PhysicsCharacter3D::set_kick_speed);
		ClassDB::bind_method(D_METHOD("get_kick_speed"), &PhysicsCharacter3D::get_kick_speed);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "kick_speed"), "set_kick_speed", "get_kick_speed");

		ClassDB::bind_method(D_METHOD("set_kick_max_distance", "p_val"), &PhysicsCharacter3D::set_kick_max_distance);
		ClassDB::bind_method(D_METHOD("get_kick_max_distance"), &PhysicsCharacter3D::get_kick_max_distance);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "kick_max_distance"), "set_kick_max_distance", "get_kick_max_distance");

		ClassDB::bind_method(D_METHOD("set_kick_impact_force", "p_val"), &PhysicsCharacter3D::set_kick_impact_force);
		ClassDB::bind_method(D_METHOD("get_kick_impact_force"), &PhysicsCharacter3D::get_kick_impact_force);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "kick_impact_force"), "set_kick_impact_force", "get_kick_impact_force");
	}

	// Grab/Throw Properties
	{
		ClassDB::bind_method(D_METHOD("set_grab_distance", "p_val"), &PhysicsCharacter3D::set_grab_distance);
		ClassDB::bind_method(D_METHOD("get_grab_distance"), &PhysicsCharacter3D::get_grab_distance);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "grab_distance"), "set_grab_distance", "get_grab_distance");

		ClassDB::bind_method(D_METHOD("set_throw_force", "p_val"), &PhysicsCharacter3D::set_throw_force);
		ClassDB::bind_method(D_METHOD("get_throw_force"), &PhysicsCharacter3D::get_throw_force);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "throw_force"), "set_throw_force", "get_throw_force");

		ClassDB::bind_method(D_METHOD("set_throw_upward_bias", "p_val"), &PhysicsCharacter3D::set_throw_upward_bias);
		ClassDB::bind_method(D_METHOD("get_throw_upward_bias"), &PhysicsCharacter3D::get_throw_upward_bias);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "throw_upward_bias"), "set_throw_upward_bias", "get_throw_upward_bias");

		ClassDB::bind_method(D_METHOD("set_hold_offset", "p_val"), &PhysicsCharacter3D::set_hold_offset);
		ClassDB::bind_method(D_METHOD("get_hold_offset"), &PhysicsCharacter3D::get_hold_offset);
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "hold_offset"), "set_hold_offset", "get_hold_offset");
	}
}

PhysicsCharacter3D::PhysicsCharacter3D() {
	set_mass(500.0f);
}

PhysicsCharacter3D::~PhysicsCharacter3D() {
	delete idle_state;
	delete move_state;
	delete jump_state;
	delete fall_state;
	delete grounded_state;
	delete airborne_state;
	delete dropkick_state;
	delete grab_state;
}

void PhysicsCharacter3D::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	_setup_character();
}

void PhysicsCharacter3D::_setup_character() {
	// 1. State Allocation with renamed Classes
	grounded_state = new CharGroundedState(this, nullptr);
	airborne_state = new CharAirborneState(this, nullptr);

	idle_state = new CharIdleState(this, grounded_state);
	move_state = new CharMoveState(this, grounded_state);
	jump_state = new CharJumpState(this, airborne_state);
	fall_state = new CharFallState(this, airborne_state);
	dropkick_state = new CharDropkickState(this, nullptr);
	grab_state = new CharGrabState(this, nullptr);

	current_state = fall_state;
	current_state->enter();

	set_lock_rotation_enabled(true);

	// 3. Register with GameManager
	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_character(this);
	}

	// 4. Setup Debug Ray
	ray_debug = memnew(DebugLineQuad);
	ray_debug->set_name("RayDebug");
	add_child(ray_debug);
	ray_debug->set_visible(false);

	// 5. Setup Ground Debug
	ground_debug = memnew(DebugLineQuad);
	ground_debug->set_name("GroundDebug");
	add_child(ground_debug);
	ground_debug->set_color(Color(1, 0, 1, 0.5f)); // Magenta
	ground_debug->set_as_top_level(true);
}

void PhysicsCharacter3D::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint())
		return;

	float f_delta = (float)delta;

	// 1. Detect ground and update normals/distance
	_handle_ground_detection(f_delta);

	// 2. Execute HSM logic for inputs and transitions
	if (current_state) {
		current_state->physics_update(f_delta);
	}

	// 3. Apply floating spring force if grounded
	_apply_hover_spring(f_delta);

	// 4. Update Ray Debug Timer
	if (ray_debug_timer > 0) {
		ray_debug_timer -= f_delta;
		if (ray_debug_timer <= 0) {
			if (ray_debug)
				ray_debug->set_visible(false);
		}
	}
}

void PhysicsCharacter3D::_handle_ground_detection(float delta) {
	// Spherecast parameters
	float cast_dist = ride_height * 1.5f;
	float radius = 0.45f; // Slightly smaller than capsule collision

	MCRaycastHit hit = spherecast_3d(this, get_global_position(), Vector3(0, -1, 0), cast_dist, radius);

	is_grounded = hit.is_hit;
	if (is_grounded) {
		dist_to_ground = (get_global_position() - hit.position).length();
		ground_normal = hit.normal;

		if (ground_debug) {
			ground_debug->set_line(get_global_position(), hit.position, 0.02f);
			ground_debug->set_visible(true);
		}
	} else {
		dist_to_ground = cast_dist;
		ground_normal = Vector3(0, 1, 0);

		if (ground_debug) {
			ground_debug->set_visible(false);
		}
	}
}

void PhysicsCharacter3D::_apply_hover_spring(float delta) {
	if (!is_grounded) {
		return;
	}

	// PD Controller: Force = Stiffness * error - Damping * velocity
	float error = ride_height - dist_to_ground;
	float velocity_on_ray = get_linear_velocity().y;

	float force_magnitude = (error * spring_stiffness) - (velocity_on_ray * spring_damping);

	if (force_magnitude > 0) {
		apply_central_force(Vector3(0, force_magnitude, 0));
	}
}

void PhysicsCharacter3D::change_state(CharacterState *p_new_state) {
	if (current_state == p_new_state) {
		return;
	}

	if (current_state)
		current_state->exit();
	current_state = p_new_state;
	if (current_state)
		current_state->enter();
}

void PhysicsCharacter3D::draw_debug_ray(const Vector3 &p_start, const Vector3 &p_end, const Color &p_color, float p_duration) {
	if (!ray_debug)
		return;

	// Note: DebugLineQuad uses world space internally via set_line if added to root,
	// but if it's a child of character, we need to handle its transform.
	// Actually DebugLineQuad's internal _update_geometry sets its transform to world space if it's placed correctly.
	// But since it's a child of the character, we should use local coordinates OR set_as_top_level.

	if (!ray_debug->is_set_as_top_level()) {
		ray_debug->set_as_top_level(true);
	}

	ray_debug->set_line(p_start, p_end, 0.05f);
	ray_debug->set_color(p_color);
	ray_debug->set_visible(true);
	ray_debug_timer = p_duration;
}

} // namespace godot
