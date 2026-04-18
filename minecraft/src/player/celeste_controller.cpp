#include "celeste_controller.h"
#include "../camera/camera.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "../utils/raycast/mc_raycast.h"

namespace godot {

void CelesteController::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_speed", "p_val"), &CelesteController::set_max_speed);
	ClassDB::bind_method(D_METHOD("get_max_speed"), &CelesteController::get_max_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_speed"), "set_max_speed", "get_max_speed");

	ClassDB::bind_method(D_METHOD("set_acceleration", "p_val"), &CelesteController::set_acceleration);
	ClassDB::bind_method(D_METHOD("get_acceleration"), &CelesteController::get_acceleration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "acceleration"), "set_acceleration", "get_acceleration");

	ClassDB::bind_method(D_METHOD("set_friction", "p_val"), &CelesteController::set_friction);
	ClassDB::bind_method(D_METHOD("get_friction"), &CelesteController::get_friction);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "friction"), "set_friction", "get_friction");

	ClassDB::bind_method(D_METHOD("set_air_resistance", "p_val"), &CelesteController::set_air_resistance);
	ClassDB::bind_method(D_METHOD("get_air_resistance"), &CelesteController::get_air_resistance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "air_resistance"), "set_air_resistance", "get_air_resistance");

	ClassDB::bind_method(D_METHOD("set_jump_height", "p_val"), &CelesteController::set_jump_height);
	ClassDB::bind_method(D_METHOD("get_jump_height"), &CelesteController::get_jump_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_height"), "set_jump_height", "get_jump_height");

	ClassDB::bind_method(D_METHOD("set_jump_time_to_peak", "p_val"), &CelesteController::set_jump_time_to_peak);
	ClassDB::bind_method(D_METHOD("get_jump_time_to_peak"), &CelesteController::get_jump_time_to_peak);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_time_to_peak"), "set_jump_time_to_peak", "get_jump_time_to_peak");

	ClassDB::bind_method(D_METHOD("set_jump_time_to_descent", "p_val"), &CelesteController::set_jump_time_to_descent);
	ClassDB::bind_method(D_METHOD("get_jump_time_to_descent"), &CelesteController::get_jump_time_to_descent);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_time_to_descent"), "set_jump_time_to_descent", "get_jump_time_to_descent");

	// Apex Controls
	ClassDB::bind_method(D_METHOD("set_apex_gravity_multiplier", "p_val"), &CelesteController::set_apex_gravity_multiplier);
	ClassDB::bind_method(D_METHOD("get_apex_gravity_multiplier"), &CelesteController::get_apex_gravity_multiplier);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "apex_gravity_multiplier"), "set_apex_gravity_multiplier", "get_apex_gravity_multiplier");

	ClassDB::bind_method(D_METHOD("set_apex_speed_multiplier", "p_val"), &CelesteController::set_apex_speed_multiplier);
	ClassDB::bind_method(D_METHOD("get_apex_speed_multiplier"), &CelesteController::get_apex_speed_multiplier);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "apex_speed_multiplier"), "set_apex_speed_multiplier", "get_apex_speed_multiplier");

	ClassDB::bind_method(D_METHOD("set_apex_threshold", "p_val"), &CelesteController::set_apex_threshold);
	ClassDB::bind_method(D_METHOD("get_apex_threshold"), &CelesteController::get_apex_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "apex_threshold"), "set_apex_threshold", "get_apex_threshold");

	// Wall Controls
	ClassDB::bind_method(D_METHOD("set_wall_jump_force", "p_val"), &CelesteController::set_wall_jump_force);
	ClassDB::bind_method(D_METHOD("get_wall_jump_force"), &CelesteController::get_wall_jump_force);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wall_jump_force"), "set_wall_jump_force", "get_wall_jump_force");

	ClassDB::bind_method(D_METHOD("set_wall_slide_speed", "p_val"), &CelesteController::set_wall_slide_speed);
	ClassDB::bind_method(D_METHOD("get_wall_slide_speed"), &CelesteController::get_wall_slide_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wall_slide_speed"), "set_wall_slide_speed", "get_wall_slide_speed");

	ClassDB::bind_method(D_METHOD("set_wall_jump_lock_duration", "p_val"), &CelesteController::set_wall_jump_lock_duration);
	ClassDB::bind_method(D_METHOD("get_wall_jump_lock_duration"), &CelesteController::get_wall_jump_lock_duration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wall_jump_lock_duration"), "set_wall_jump_lock_duration", "get_wall_jump_lock_duration");

	ClassDB::bind_method(D_METHOD("set_coyote_time_duration", "p_val"), &CelesteController::set_coyote_time_duration);
	ClassDB::bind_method(D_METHOD("get_coyote_time_duration"), &CelesteController::get_coyote_time_duration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "coyote_time_duration"), "set_coyote_time_duration", "get_coyote_time_duration");

	ClassDB::bind_method(D_METHOD("set_jump_buffer_duration", "p_val"), &CelesteController::set_jump_buffer_duration);
	ClassDB::bind_method(D_METHOD("get_jump_buffer_duration"), &CelesteController::get_jump_buffer_duration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_buffer_duration"), "set_jump_buffer_duration", "get_jump_buffer_duration");

	ClassDB::bind_method(D_METHOD("set_max_fall_speed", "p_val"), &CelesteController::set_max_fall_speed);
	ClassDB::bind_method(D_METHOD("get_max_fall_speed"), &CelesteController::get_max_fall_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_fall_speed"), "set_max_fall_speed", "get_max_fall_speed");

	ClassDB::bind_method(D_METHOD("set_ride_height", "p_val"), &CelesteController::set_ride_height);
	ClassDB::bind_method(D_METHOD("get_ride_height"), &CelesteController::get_ride_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ride_height"), "set_ride_height", "get_ride_height");

	ClassDB::bind_method(D_METHOD("set_spring_stiffness", "p_val"), &CelesteController::set_spring_stiffness);
	ClassDB::bind_method(D_METHOD("get_spring_stiffness"), &CelesteController::get_spring_stiffness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spring_stiffness"), "set_spring_stiffness", "get_spring_stiffness");

	ClassDB::bind_method(D_METHOD("set_spring_damping", "p_val"), &CelesteController::set_spring_damping);
	ClassDB::bind_method(D_METHOD("get_spring_damping"), &CelesteController::get_spring_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spring_damping"), "set_spring_damping", "get_spring_damping");
}

CelesteController::CelesteController() {
	max_speed = 10.0f;
	acceleration = 80.0f;
	friction = 60.0f;
	air_resistance = 40.0f; // More air control for floatiness

	jump_height = 4.5f; // Higher jump
	jump_time_to_peak = 0.6f; // Slower ascent
	jump_time_to_descent = 0.55f; // Slower descent

	apex_gravity_multiplier = 0.15f; // extreme hang time
	apex_threshold = 3.5f; // wider float window
	max_fall_speed = 15.0f;

	_update_jump_math();
}

CelesteController::~CelesteController() {
}

void CelesteController::_update_jump_math() {
	jump_velocity = (2.0f * jump_height) / jump_time_to_peak;
	jump_gravity = (2.0f * jump_height) / (jump_time_to_peak * jump_time_to_peak);
	fall_gravity = (2.0f * jump_height) / (jump_time_to_descent * jump_time_to_descent);
}

void CelesteController::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	_update_jump_math();

	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_celeste_controller(this);
	}
}

void CelesteController::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	float f_delta = (float)delta;
	PlayerInput *input = PlayerInput::get_singleton();
	if (!input) {
		return;
	}

	const ActionState &state = input->get_state();
	Vector3 velocity = get_velocity();

	// 1. Determine local input direction (Move relative to character orientation)
	Transform3D transform = get_global_transform();

	// Align with camera if in TPS mode
	GameCamera *cam = GameManager::get_singleton() ? GameManager::get_singleton()->get_camera() : nullptr;
	if (cam && cam->get_camera_mode() == GameCamera::MODE_TPS) {
		set_rotation(Vector3(0, cam->get_yaw(), 0));
		transform = get_global_transform(); // Refresh transform for movement calculation
	}

	Vector3 forward = -transform.basis.get_column(2).normalized();
	Vector3 right = transform.basis.get_column(0).normalized();

	Vector2 move_axis = state.character.move_axis;
	// Negate move_axis.y because Godot's get_vector returns -1 for "up" (Forward)
	Vector3 move_dir = (forward * -move_axis.y + right * move_axis.x);
	if (move_dir.length() > 1.0f) {
		move_dir.normalize();
	}

	// 2. Horizontal Movement logic
	Vector3 target_horizontal_vel = move_dir * max_speed;
	float current_accel;

	if (is_on_floor()) {
		current_accel = (move_dir.length() > 0.01f) ? acceleration : friction;
	} else {
		current_accel = air_resistance;
	}

	// APEX MODIFIER: More air control at jump peak
	bool is_apex = !is_on_floor() && Math::abs(velocity.y) < apex_threshold;
	if (is_apex) {
		current_accel *= apex_speed_multiplier;
		target_horizontal_vel *= apex_speed_multiplier;
	}

	// WALL JUMP LOCK: Temporarily ignore horizontal input after a wall kick
	if (wall_jump_lock_timer > 0.0f) {
		wall_jump_lock_timer -= f_delta;
		// Blend input back in? For now, we just let the momentum carry it.
	} else {
		velocity.x = Math::move_toward(velocity.x, target_horizontal_vel.x, current_accel * f_delta);
		velocity.z = Math::move_toward(velocity.z, target_horizontal_vel.z, current_accel * f_delta);
	}

	// 3. Vertical Movement (Gravity & Jump Logic)

	// Coyote Time Handling (Enhanced with Hover support)
	// Cast for hover spring
	float cast_dist = ride_height * 1.5f;
	float radius = 0.45f;
	MCRaycastHit hover_hit = spherecast_3d(this, get_global_position(), Vector3(0, -1, 0), cast_dist, radius);
	
	is_hovering = hover_hit.is_hit;
	
	if (is_on_floor() || is_hovering) {
		coyote_timer = coyote_time_duration;
		is_jumping = false;
	} else {
		coyote_timer -= f_delta;
	}

	// Jump Buffer Handling
	if (state.character.jump_just_pressed) {
		jump_buffer_timer = jump_buffer_duration;
	} else {
		jump_buffer_timer -= f_delta;
	}

	// WALL SENSING
	bool on_wall = is_on_wall() && !is_on_floor();
	Vector3 wall_normal = get_wall_normal();

	// EXECUTE JUMP (Ground, Coyote, or Hover)
	if (jump_buffer_timer > 0.0f && coyote_timer > 0.0f && !is_jumping) {
		velocity.y = jump_velocity;
		is_jumping = true;
		is_hovering = false; // Break hover on jump
		jump_buffer_timer = 0.0f;
		coyote_timer = 0.0f;
		UtilityFunctions::print("CelesteController: Float Jump!");
	}
	// EXECUTE JUMP (Wall)
	else if (jump_buffer_timer > 0.0f && on_wall) {
		velocity.y = jump_velocity;
		velocity.x = wall_normal.x * wall_jump_force;
		velocity.z = wall_normal.z * wall_jump_force;

		wall_jump_lock_timer = wall_jump_lock_duration;
		jump_buffer_timer = 0.0f;
		is_jumping = true;
		UtilityFunctions::print("CelesteController: Wall Kick!");
	}

	// Apply Variable Jump Height (If button released early during up-phase)
	if (!state.character.jump && velocity.y > 0.0f && is_jumping) {
		velocity.y *= 0.5f; // Instant cut to upward momentum
	}

	// WALL SLIDING
	if (on_wall && velocity.y < 0.0f && move_dir.dot(-wall_normal) > 0.1f) {
		velocity.y = Math::max(velocity.y, -wall_slide_speed);
	} else {
		// Apply Hover Spring OR Gravity
		if (is_hovering && !is_jumping) {
			float dist_to_ground = (get_global_position() - hover_hit.position).length();
			float error = ride_height - dist_to_ground;
			
			// PD Controller: acceleration = stiffness * error - damping * current_velocity
			float hooke_law = error * spring_stiffness;
			float damping_force = velocity.y * spring_damping;
			float spring_accel = hooke_law - damping_force;
			
			velocity.y += spring_accel * f_delta;
		} else {
			// Apply Gravity (with Apex modifier)
			float gravity_mult = is_apex ? apex_gravity_multiplier : 1.0f;
			float current_gravity = (velocity.y > 0.0f) ? jump_gravity : fall_gravity;
			velocity.y -= current_gravity * gravity_mult * f_delta;

			// Capping fall speed for floatiness
			if (velocity.y < -max_fall_speed) {
				velocity.y = -max_fall_speed;
			}
		}
	}

	// 4. Final Application
	set_velocity(velocity);
	move_and_slide();
}

} // namespace godot
