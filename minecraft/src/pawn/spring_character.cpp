#include "spring_character.h"

#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "../utils/spring/stateless_spring.h"
#include "../utils/raycast/mc_raycast.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <cmath>

namespace godot {

static const float PAWN_PI = 3.14159265358979f;

SpringCharacter::SpringCharacter() {}

void SpringCharacter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_speed", "v"), &SpringCharacter::set_max_speed);
	ClassDB::bind_method(D_METHOD("get_max_speed"), &SpringCharacter::get_max_speed);
	ClassDB::bind_method(D_METHOD("set_jump_height", "v"), &SpringCharacter::set_jump_height);
	ClassDB::bind_method(D_METHOD("get_jump_height"), &SpringCharacter::get_jump_height);
	ClassDB::bind_method(D_METHOD("set_ride_height", "v"), &SpringCharacter::set_ride_height);
	ClassDB::bind_method(D_METHOD("get_ride_height"), &SpringCharacter::get_ride_height);
	ClassDB::bind_method(D_METHOD("set_ride_spring_strength", "v"), &SpringCharacter::set_ride_spring_strength);
	ClassDB::bind_method(D_METHOD("get_ride_spring_strength"), &SpringCharacter::get_ride_spring_strength);
	ClassDB::bind_method(D_METHOD("set_ride_spring_damper", "v"), &SpringCharacter::set_ride_spring_damper);
	ClassDB::bind_method(D_METHOD("get_ride_spring_damper"), &SpringCharacter::get_ride_spring_damper);
	ClassDB::bind_method(D_METHOD("set_upright_strength", "v"), &SpringCharacter::set_upright_strength);
	ClassDB::bind_method(D_METHOD("get_upright_strength"), &SpringCharacter::get_upright_strength);
	ClassDB::bind_method(D_METHOD("set_upright_damper", "v"), &SpringCharacter::set_upright_damper);
	ClassDB::bind_method(D_METHOD("get_upright_damper"), &SpringCharacter::get_upright_damper);
	ClassDB::bind_method(D_METHOD("is_grounded"), &SpringCharacter::is_grounded);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_speed"), "set_max_speed", "get_max_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_height"), "set_jump_height", "get_jump_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ride_height"), "set_ride_height", "get_ride_height");
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "ride_spring_strength"), "set_ride_spring_strength", "get_ride_spring_strength"
	);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ride_spring_damper"), "set_ride_spring_damper", "get_ride_spring_damper");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "upright_strength"), "set_upright_strength", "get_upright_strength");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "upright_damper"), "set_upright_damper", "get_upright_damper");
}

// Larger acceleration when the desired direction opposes current motion, so a
// hard reverse feels snappy (VVV's accel-from-dot curve, linearised).
float SpringCharacter::_accel_factor(float p_vel_dot, float p_boost) {
	// p_vel_dot in [-1, 1]: -1 = full reverse, +1 = same direction.
	float t = (p_vel_dot + 1.0f) * 0.5f; // 0 at reverse, 1 at aligned
	return p_boost + (1.0f - p_boost) * t; // boost at reverse -> 1 aligned
}

void SpringCharacter::_recompute_jump() {
	double g = 9.8;
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (ps) {
		Variant v = ps->get_setting("physics/3d/default_gravity");
		if (v.get_type() == Variant::FLOAT || v.get_type() == Variant::INT) {
			g = (double)v;
		}
	}
	g *= (double)get_gravity_scale();
	if (g < 0.1) {
		g = 9.8;
	}
	_jump_velocity = (float)std::sqrt(2.0 * g * (double)jump_height);
}

void SpringCharacter::_ensure_body() {
	for (int i = 0; i < get_child_count(); i++) {
		if (Object::cast_to<CollisionShape3D>(get_child(i))) {
			return; // scene supplied its own collider
		}
	}
	_collider = memnew(CollisionShape3D);
	Ref<CapsuleShape3D> cap;
	cap.instantiate();
	cap->set_radius(capsule_radius);
	cap->set_height(capsule_height);
	_collider->set_shape(cap);
	add_child(_collider);

	_mesh = memnew(MeshInstance3D);
	Ref<CapsuleMesh> cm;
	cm.instantiate();
	cm->set_radius(capsule_radius);
	cm->set_height(capsule_height);
	_mesh->set_mesh(cm);
	add_child(_mesh);
}

void SpringCharacter::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (player_input == nullptr) {
		player_input = PlayerInput::get_singleton();
	}
	_ensure_body();
	set_gravity_scale(1.0f); // light base gravity; the Mario arc is shaped in code
	// Free rotation: the upright torque spring keeps us vertical (VVV), so we do
	// NOT lock axes — the body can tip on impacts and recover.
	_recompute_jump();

	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_spring_character(this);
	}
}

void SpringCharacter::_exit_tree() {
	GameManager *gm = GameManager::get_singleton();
	if (gm && gm->get_spring_character() == this) {
		gm->register_spring_character(nullptr);
	}
}

void SpringCharacter::_cast_ground() {
	Vector3 origin = get_global_position();
	float max_len = ride_height + ride_ray_extra;
	Vector3 to = origin + Vector3(0.0f, -1.0f, 0.0f) * max_len;

	TypedArray<RID> exclude;
	exclude.push_back(get_rid());
	MCRaycastHit hit = raycast_3d(this, origin, to, 0xFFFFFFFF, exclude);

	if (hit.is_hit) {
		_has_support = true;
		_ground_distance = origin.distance_to(hit.position);
		_ground_normal = hit.normal;
		_ground_point = hit.position;
		_ground_body = hit.collider;
		_grounded = _ground_distance <= ride_height + 0.15f;
	} else {
		_has_support = false;
		_grounded = false;
		_ground_distance = max_len;
		_ground_normal = Vector3(0, 1, 0);
		_ground_body = nullptr;
	}
}

// PD ride spring along world-down. Bidirectional (pulls the body back to
// ride_height either way) and applies the equal-and-opposite force to the ground
// body, so the character presses on platforms and cars it stands on.
void SpringCharacter::_apply_float_spring() {
	if (!_has_support) {
		return;
	}
	Vector3 ray_dir(0.0f, -1.0f, 0.0f);
	float ray_dir_vel = ray_dir.dot(get_linear_velocity());
	float x = _ground_distance - ride_height; // displacement from rest (ride_height)
	float spring_force = PDSpring::pd(x, ray_dir_vel, ride_spring_strength, ride_spring_damper);

	apply_central_force(ray_dir * spring_force);

	RigidBody3D *hit_body = Object::cast_to<RigidBody3D>(_ground_body);
	if (hit_body) {
		Vector3 rel = _ground_point - hit_body->get_global_position();
		hit_body->apply_force(ray_dir * -spring_force, rel);
	}
}

// PD torque spring toward a goal orientation that is upright (world up) and, when
// moving, yawed to face travel. Lets the body tip and recover (VVV).
void SpringCharacter::_apply_upright_and_facing(const Vector3 &p_wish) {
	Quaternion current = get_global_transform().basis.get_rotation_quaternion();

	float goal_yaw;
	if (p_wish.length() > 0.1f) {
		goal_yaw = std::atan2(-p_wish.x, -p_wish.z); // body -Z faces travel
	} else {
		goal_yaw = current.get_euler().y; // hold current heading
	}
	Quaternion goal(Vector3(0.0f, 1.0f, 0.0f), goal_yaw);

	Vector3 torque = PDSpring::pd_torque(current, goal, get_angular_velocity(), upright_strength, upright_damper);
	apply_torque(torque);
}

// VVV movement: ramp a goal velocity toward the input (faster when reversing),
// then apply a mass-scaled steering force clamped to max_accel_force. Y is left
// to gravity / jump / the ride spring.
void SpringCharacter::_apply_movement(const Vector3 &p_wish, float p_dt) {
	Vector3 goal_vel_target = p_wish * max_speed;

	float vel_dot = 0.0f;
	if (_goal_vel.length() > 0.01f && p_wish.length() > 0.01f) {
		vel_dot = p_wish.normalized().dot(_goal_vel.normalized());
	}
	float accel = acceleration * _accel_factor(vel_dot, accel_turn_boost);
	_goal_vel = _goal_vel.move_toward(goal_vel_target, accel * p_dt);

	Vector3 vel = get_linear_velocity();
	Vector3 needed = (_goal_vel - Vector3(vel.x, 0.0f, vel.z)) / p_dt;
	float max_force = max_accel_force * _accel_factor(vel_dot, accel_turn_boost);
	needed = needed.limit_length(max_force);

	apply_central_force(Vector3(needed.x, 0.0f, needed.z) * get_mass());
}

void SpringCharacter::_apply_jump_and_gravity(bool p_jump_now, bool p_jump_held, float p_dt) {
	if (_grounded) {
		_coyote_timer = coyote_time;
	} else {
		_coyote_timer -= p_dt;
	}
	if (p_jump_now) {
		_jump_buffer_timer = jump_buffer;
	} else {
		_jump_buffer_timer -= p_dt;
	}

	if (_jump_buffer_timer > 0.0f && _coyote_timer > 0.0f) {
		Vector3 v = get_linear_velocity();
		v.y = _jump_velocity;
		set_linear_velocity(v);
		_coyote_timer = 0.0f;
		_jump_buffer_timer = 0.0f;
		_jump_lock = jump_lock_time;
	}

	float v_y = get_linear_velocity().y;
	bool airborne = !_grounded || _jump_lock > 0.0f;
	if (airborne) {
		float extra = 0.0f;
		if (v_y < 0.0f) {
			extra = fall_gravity_extra; // fast fall past the apex
		} else if (!p_jump_held) {
			extra = low_jump_gravity_extra; // released while rising -> short hop
		}
		if (extra > 0.0f) {
			apply_central_force(Vector3(0.0f, -1.0f, 0.0f) * extra * get_mass());
		}
	}
}

Vector3 SpringCharacter::_camera_wish(const Vector2 &p_move_axis) const {
	Vector3 cam_forward(0.0f, 0.0f, -1.0f);
	Vector3 cam_right(1.0f, 0.0f, 0.0f);
	Viewport *vp = get_viewport();
	Camera3D *cam = vp ? vp->get_camera_3d() : nullptr;
	if (cam) {
		Basis b = cam->get_global_transform().basis;
		cam_forward = -b.get_column(2);
		cam_right = b.get_column(0);
	}
	cam_forward.y = 0.0f;
	cam_right.y = 0.0f;
	if (cam_forward.length() > 0.001f) {
		cam_forward = cam_forward.normalized();
	}
	if (cam_right.length() > 0.001f) {
		cam_right = cam_right.normalized();
	}
	// move_axis.y is +1 for "backward", so forward (-1) pushes along cam_forward.
	Vector3 wish = cam_right * p_move_axis.x + cam_forward * (-p_move_axis.y);
	if (wish.length() > 1.0f) {
		wish = wish.normalized();
	}
	return wish;
}

void SpringCharacter::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	float dt = (float)delta;
	if (_jump_lock > 0.0f) {
		_jump_lock -= dt;
	}

	Vector2 move_axis(0.0f, 0.0f);
	bool jump_now = false;
	bool jump_held = false;
	if (player_input) {
		const ActionState &st = player_input->get_state();
		move_axis = st.character.move_axis;
		jump_now = st.character.jump_just_pressed;
		jump_held = st.character.jump;
	}
	Vector3 wish = _camera_wish(move_axis);

	_cast_ground();
	if (_jump_lock <= 0.0f) {
		_apply_float_spring(); // off briefly after a jump so it doesn't fight the launch
	}
	_apply_upright_and_facing(wish);
	_apply_movement(wish, dt);
	_apply_jump_and_gravity(jump_now, jump_held, dt);
}

} // namespace godot
