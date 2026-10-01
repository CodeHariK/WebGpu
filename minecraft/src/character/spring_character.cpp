#include "spring_character.h"

#include "ai/character_states.h"
#include "character_ui.h"

#include "../cui/cui.h"

#include "../camera/camera.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "../utils/raycast/mc_raycast.h"
#include "../interaction/environment/moving_platform.h"
#include "../debug_draw/debug_manager.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/physics_material.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

namespace godot {

SpringCharacter::SpringCharacter() {}

void SpringCharacter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_speed", "v"), &SpringCharacter::set_max_speed);
	ClassDB::bind_method(D_METHOD("get_max_speed"), &SpringCharacter::get_max_speed);
	ClassDB::bind_method(D_METHOD("set_steer_rate", "v"), &SpringCharacter::set_steer_rate);
	ClassDB::bind_method(D_METHOD("get_steer_rate"), &SpringCharacter::get_steer_rate);
	ClassDB::bind_method(D_METHOD("set_acceleration", "v"), &SpringCharacter::set_acceleration);
	ClassDB::bind_method(D_METHOD("get_acceleration"), &SpringCharacter::get_acceleration);
	ClassDB::bind_method(D_METHOD("set_jump_height", "v"), &SpringCharacter::set_jump_height);
	ClassDB::bind_method(D_METHOD("get_jump_height"), &SpringCharacter::get_jump_height);
	ClassDB::bind_method(D_METHOD("set_jump_time_to_peak", "v"), &SpringCharacter::set_jump_time_to_peak);
	ClassDB::bind_method(D_METHOD("get_jump_time_to_peak"), &SpringCharacter::get_jump_time_to_peak);
	ClassDB::bind_method(D_METHOD("set_jump_time_to_descent", "v"), &SpringCharacter::set_jump_time_to_descent);
	ClassDB::bind_method(D_METHOD("get_jump_time_to_descent"), &SpringCharacter::get_jump_time_to_descent);
	ClassDB::bind_method(D_METHOD("set_ride_height", "v"), &SpringCharacter::set_ride_height);
	ClassDB::bind_method(D_METHOD("get_ride_height"), &SpringCharacter::get_ride_height);
	ClassDB::bind_method(D_METHOD("set_ride_follow", "v"), &SpringCharacter::set_ride_follow);
	ClassDB::bind_method(D_METHOD("get_ride_follow"), &SpringCharacter::get_ride_follow);
	ClassDB::bind_method(D_METHOD("set_dash_speed", "v"), &SpringCharacter::set_dash_speed);
	ClassDB::bind_method(D_METHOD("get_dash_speed"), &SpringCharacter::get_dash_speed);
	ClassDB::bind_method(D_METHOD("set_dash_time", "v"), &SpringCharacter::set_dash_time);
	ClassDB::bind_method(D_METHOD("get_dash_time"), &SpringCharacter::get_dash_time);
	ClassDB::bind_method(D_METHOD("set_dash_cooldown", "v"), &SpringCharacter::set_dash_cooldown);
	ClassDB::bind_method(D_METHOD("get_dash_cooldown"), &SpringCharacter::get_dash_cooldown);
	ClassDB::bind_method(D_METHOD("set_air_jumps", "v"), &SpringCharacter::set_air_jumps);
	ClassDB::bind_method(D_METHOD("get_air_jumps"), &SpringCharacter::get_air_jumps);
	ClassDB::bind_method(D_METHOD("set_wall_jump_up", "v"), &SpringCharacter::set_wall_jump_up);
	ClassDB::bind_method(D_METHOD("get_wall_jump_up"), &SpringCharacter::get_wall_jump_up);
	ClassDB::bind_method(D_METHOD("set_wall_jump_out", "v"), &SpringCharacter::set_wall_jump_out);
	ClassDB::bind_method(D_METHOD("get_wall_jump_out"), &SpringCharacter::get_wall_jump_out);
	ClassDB::bind_method(D_METHOD("set_sprint_multiplier", "v"), &SpringCharacter::set_sprint_multiplier);
	ClassDB::bind_method(D_METHOD("get_sprint_multiplier"), &SpringCharacter::get_sprint_multiplier);
	ClassDB::bind_method(D_METHOD("set_pound_speed", "v"), &SpringCharacter::set_pound_speed);
	ClassDB::bind_method(D_METHOD("get_pound_speed"), &SpringCharacter::get_pound_speed);
	ClassDB::bind_method(D_METHOD("is_grounded"), &SpringCharacter::is_grounded);
	ClassDB::bind_method(D_METHOD("_on_ui_toggle"), &SpringCharacter::_on_ui_toggle);
	ClassDB::bind_method(D_METHOD("_on_ui_slider_value_changed", "value", "property"), &SpringCharacter::_on_ui_slider_value_changed);
	ClassDB::bind_method(D_METHOD("save_settings"), &SpringCharacter::save_settings);
	ClassDB::bind_method(D_METHOD("load_settings"), &SpringCharacter::load_settings);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_speed"), "set_max_speed", "get_max_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "steer_rate"), "set_steer_rate", "get_steer_rate");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "acceleration"), "set_acceleration", "get_acceleration");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_height"), "set_jump_height", "get_jump_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_time_to_peak"), "set_jump_time_to_peak", "get_jump_time_to_peak");
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "jump_time_to_descent"), "set_jump_time_to_descent", "get_jump_time_to_descent"
	);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ride_height"), "set_ride_height", "get_ride_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ride_follow"), "set_ride_follow", "get_ride_follow");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dash_speed"), "set_dash_speed", "get_dash_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dash_time"), "set_dash_time", "get_dash_time");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dash_cooldown"), "set_dash_cooldown", "get_dash_cooldown");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "air_jumps"), "set_air_jumps", "get_air_jumps");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wall_jump_up"), "set_wall_jump_up", "get_wall_jump_up");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wall_jump_out"), "set_wall_jump_out", "get_wall_jump_out");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sprint_multiplier"), "set_sprint_multiplier", "get_sprint_multiplier");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pound_speed"), "set_pound_speed", "get_pound_speed");
}

// Larger acceleration when the desired direction opposes current motion, so a
// hard reverse feels snappy (VVV's accel-from-dot curve, linearised).
float SpringCharacter::_accel_factor(float p_vel_dot, float p_boost) {
	float t = (p_vel_dot + 1.0f) * 0.5f; // 0 at reverse, 1 at aligned
	return p_boost + (1.0f - p_boost) * t;
}

// Kinematic jump math: jump velocity and the two gravities fall straight out of
// the desired height and the up / down times (Celeste-style, fully deterministic).
void SpringCharacter::_recompute_jump() {
	float tp = jump_time_to_peak > 0.01f ? jump_time_to_peak : 0.01f;
	float td = jump_time_to_descent > 0.01f ? jump_time_to_descent : 0.01f;
	_jump_velocity = (2.0f * jump_height) / tp;
	_jump_gravity = (2.0f * jump_height) / (tp * tp);
	_fall_gravity = (2.0f * jump_height) / (td * td);
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

	// A bright "nose" on the upper-front (-Z is the facing direction) so the look
	// direction is visible on the otherwise featureless capsule.
	MeshInstance3D *face = memnew(MeshInstance3D);
	Ref<BoxMesh> box;
	box.instantiate();
	box->set_size(Vector3(0.22f, 0.16f, 0.16f));
	face->set_mesh(box);
	face->set_position(Vector3(0.0f, capsule_height * 0.28f, -(capsule_radius + 0.02f)));
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_albedo(Color(1.0f, 0.35f, 0.1f));
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	face->set_material_override(mat);
	_mesh->add_child(face); // child of the capsule mesh -> rotates with the body
}

void SpringCharacter::_init_states() {
	grounded_state = new CharacterGroundedState("grounded", this, nullptr);
	airborne_state = new CharacterAirborneState("airborne", this, nullptr);
	walk_state = new CharacterWalkState("walk", this, grounded_state);
	fall_state = new CharacterFallState("fall", this, airborne_state);
	dash_state = new CharacterDashState("dash", this, nullptr);
	pound_state = new CharacterGroundPoundState("pound", this, airborne_state);

	current_state = walk_state;
	current_state->enter();
}

void SpringCharacter::_free_states() {
	current_state = nullptr;
	delete grounded_state;
	delete airborne_state;
	delete walk_state;
	delete fall_state;
	delete dash_state;
	delete pound_state;
	grounded_state = nullptr;
	airborne_state = nullptr;
	walk_state = nullptr;
	fall_state = nullptr;
	dash_state = nullptr;
	pound_state = nullptr;
}

void SpringCharacter::change_state(CharacterState *p_next) {
	if (current_state == p_next) {
		return;
	}
	if (current_state) {
		current_state->exit();
	}
	current_state = p_next;
	if (current_state) {
		current_state->enter();
	}
}

void SpringCharacter::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (player_input == nullptr) {
		player_input = PlayerInput::get_singleton();
	}
	_ensure_body();

	// Deterministic control: WE own the velocity, so turn off engine gravity and
	// damping. The body still collides and pushes dynamic bodies through contact.
	set_gravity_scale(0.0f);
	set_linear_damp(0.0f);
	set_can_sleep(false);
	// Lock rotation: an animated mesh will own facing/lean, so the physics capsule just
	// stays upright by constraint (no tipping, no per-frame torque spring to fight anims).
	set_lock_rotation_enabled(true);
	// Frictionless so the capsule never clings to a wall (Prototype-style): wall contact
	// cannot rub off our velocity. Ground movement is velocity-driven, so 0 friction is safe.
	Ref<PhysicsMaterial> pm;
	pm.instantiate();
	pm->set_friction(0.0f);
	pm->set_rough(false);
	pm->set_bounce(0.0f);
	set_physics_material_override(pm);

	_recompute_jump();
	_air_jumps_left = air_jumps;
	_init_states();

	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_spring_character(this);
	}

	// Live tuning UI: register the tunables, build the slider panel, load saved values.
	ui_vars["max_speed"] = &max_speed;
	ui_vars["acceleration"] = &acceleration;
	ui_vars["sprint_multiplier"] = &sprint_multiplier;
	ui_vars["steer_rate"] = &steer_rate;
	ui_vars["face_turn_rate"] = &face_turn_rate;
	ui_vars["jump_height"] = &jump_height;
	ui_vars["jump_time_to_peak"] = &jump_time_to_peak;
	ui_vars["jump_time_to_descent"] = &jump_time_to_descent;
	ui_vars["coyote_time"] = &coyote_time;
	ui_vars["jump_buffer"] = &jump_buffer;
	ui_vars["min_ride_height"] = &min_ride_height;
	ui_vars["max_ride_height"] = &max_ride_height;
	ui_vars["ride_height_speed"] = &ride_height_speed;
	ui_vars["ride_follow"] = &ride_follow;
	ui_vars["dash_speed"] = &dash_speed;
	ui_vars["dash_cooldown"] = &dash_cooldown;
	ui_vars["pound_speed"] = &pound_speed;
	ui_vars["wall_jump_up"] = &wall_jump_up;
	ui_vars["wall_jump_out"] = &wall_jump_out;

	ui_root = CUI::create_on_new_layer(this);
	ui_helper = new CharacterUI();
	ui_helper->setup(this, ui_root);
	load_settings();
}

void SpringCharacter::_exit_tree() {
	GameManager *gm = GameManager::get_singleton();
	if (gm && gm->get_spring_character() == this) {
		gm->register_spring_character(nullptr);
	}
	if (ui_helper) {
		delete ui_helper;
		ui_helper = nullptr;
	}
	_free_states();
}

void SpringCharacter::_update_ride_height(float p_delta) {
	// Celeste-style dynamic ride height, measured as foot clearance. Ride low (min) to look
	// planted; when a STEP is in front of the feet, ease up to max so the ground servo floats
	// the body up and over it. "Step vs wall" is decided with two forward rays from the sole:
	//   - a LOW ray at the feet that catches a step riser (a steep-ish face right ahead), and
	//   - a HIGH ray at where the body would ride after rising; if that is CLEAR the obstacle
	//     is short enough to float over, if it is ALSO blocked it is a tall wall -> don't lift
	//     (the wall-climb / slide system owns that).
	float target = min_ride_height;

	Vector3 hvel = get_linear_velocity();
	hvel.y = 0.0f;
	if (hvel.length() > 0.5f) {
		Vector3 up(0.0f, 1.0f, 0.0f);
		Basis hb(up, _face_yaw);
		Vector3 fwd = -hb.get_column(2); // heading forward
		Vector3 sole = get_global_position() - Vector3(0.0f, capsule_height * 0.5f, 0.0f);

		TypedArray<RID> exclude;
		exclude.push_back(get_rid());

		Vector3 low = sole + up * 0.1f; // just above the sole -> hits a step riser
		Vector3 high = sole + up * (max_ride_height + 0.25f); // above where we'd ride after rising
		MCRaycastHit low_hit = raycast_3d(this, low, low + fwd * ride_probe_ahead, 0xFFFFFFFF, exclude);
		bool high_clear = !raycast_3d(this, high, high + fwd * ride_probe_ahead, 0xFFFFFFFF, exclude).is_hit;

		bool riser = low_hit.is_hit && low_hit.normal.dot(up) < 0.5f; // steep face = a step, not a slope
		if (riser && high_clear) {
			target = max_ride_height; // a low step ahead: rise and float over it
		}
	}

	ride_height = Math::move_toward(ride_height, target, p_delta * ride_height_speed);
}

void SpringCharacter::_cast_ground() {
	// Cast DOWN from the sole (capsule bottom), like Celeste, not from the body centre, so
	// _ground_distance is the true foot-to-ground clearance the servo holds at ride_height.
	// Start a small skin above the sole so a grazed step edge doesn't begin inside geometry.
	const float skin = 0.2f;
	Vector3 sole = get_global_position() - Vector3(0.0f, capsule_height * 0.5f, 0.0f);
	Vector3 origin = sole + Vector3(0.0f, skin, 0.0f);
	float max_len = skin + ride_height + ride_ray_extra; // reach ride_height+extra below the sole
	Vector3 to = origin + Vector3(0.0f, -1.0f, 0.0f) * max_len;

	TypedArray<RID> exclude;
	exclude.push_back(get_rid());
	MCRaycastHit hit = raycast_3d(this, origin, to, 0xFFFFFFFF, exclude);

	if (hit.is_hit) {
		_has_support = true;
		_ground_distance = origin.distance_to(hit.position) - skin; // sole -> ground
		if (_ground_distance < 0.0f) {
			_ground_distance = 0.0f;
		}
		_ground_normal = hit.normal;
		_ground_point = hit.position;
		_ground_body = hit.collider;
		MovingPlatform *mp = Object::cast_to<MovingPlatform>(_ground_body);
		_platform_velocity = mp ? mp->get_velocity() : Vector3(0.0f, 0.0f, 0.0f);
		_grounded = _ground_distance <= ride_height + 0.15f;
	} else {
		_has_support = false;
		_grounded = false;
		_ground_distance = ride_height + ride_ray_extra;
		_ground_normal = Vector3(0, 1, 0);
		_ground_body = nullptr;
		_platform_velocity = Vector3(0.0f, 0.0f, 0.0f);
	}
}

// Short horizontal ray in the movement direction to find a wall to climb. Only a
// near-vertical surface counts (|normal.y| small).
void SpringCharacter::_cast_wall(const Vector3 &p_wish) {
	_has_wall = false;

	Vector3 dir = p_wish;
	if (dir.length() < 0.1f) {
		dir = Vector3(_vel.x, 0.0f, _vel.z);
	}
	dir.y = 0.0f;
	if (dir.length() < 0.1f) {
		return;
	}
	dir = dir.normalized();

	Vector3 origin = get_global_position();
	float half = capsule_height * 0.5f;
	TypedArray<RID> exclude;
	exclude.push_back(get_rid());

	// Two horizontal rays: chest + feet confirm a real vertical wall (not a lip or a
	// step) so we can wall-jump off it.
	Vector3 chest_o = origin;
	Vector3 feet_o = origin - Vector3(0.0f, half * 0.6f, 0.0f);
	MCRaycastHit chest = raycast_3d(this, chest_o, chest_o + dir * wall_probe, 0xFFFFFFFF, exclude);
	MCRaycastHit feet = raycast_3d(this, feet_o, feet_o + dir * wall_probe, 0xFFFFFFFF, exclude);

	bool chest_wall = chest.is_hit && chest.normal.y < 0.4f && chest.normal.y > -0.4f;
	bool feet_wall = feet.is_hit && feet.normal.y < 0.4f && feet.normal.y > -0.4f;
	if (chest_wall && feet_wall) {
		_has_wall = true;
		_wall_normal = chest.normal;
	}
}
void SpringCharacter::_tick_timers(float p_dt) {
	if (_jump_lock > 0.0f) {
		_jump_lock -= p_dt;
	}
	if (_dash_cd_timer > 0.0f) {
		_dash_cd_timer -= p_dt;
	}
	if (_dash_timer > 0.0f) {
		_dash_timer -= p_dt;
	}
	if (_pound_hang_timer > 0.0f) {
		_pound_hang_timer -= p_dt;
	}
}

void SpringCharacter::_update_jump_timers(float p_dt) {
	if (_grounded) {
		_coyote_timer = coyote_time;
	} else {
		_coyote_timer -= p_dt;
	}
	if (_in_jump_pressed) {
		_jump_buffer_timer = jump_buffer;
	} else {
		_jump_buffer_timer -= p_dt;
	}
}

// All change_state decisions live here (one place, like ArcadeVehicle). Jumps
// (ground / air / wall) set the exact launch velocity here, then hand off.
void SpringCharacter::_update_transitions() {
	// 1. Mid-action states run to completion.
	if (current_state == dash_state) {
		if (_dash_timer <= 0.0f) {
			_dash_cd_timer = dash_cooldown;
			change_state(_grounded ? (CharacterState *)walk_state : (CharacterState *)fall_state);
		}
		return;
	}
	if (current_state == pound_state) {
		if (_grounded && _pound_hang_timer <= 0.0f) {
			change_state(walk_state);
		}
		return;
	}

	// 2. Dash / ground-pound trigger (off cooldown). Airborne with no stick
	//    direction becomes a ground pound; otherwise a dash.
	if (_in_dash_pressed && _dash_cd_timer <= 0.0f) {
		if (!_grounded && _wish.length() < 0.1f) {
			change_state(pound_state);
		} else {
			change_state(dash_state);
		}
		return;
	}

	// 3. Ground jump (buffered press inside the coyote window).
	if (_jump_buffer_timer > 0.0f && _coyote_timer > 0.0f) {
		_vel.y = _jump_velocity;
		_coyote_timer = 0.0f;
		_jump_buffer_timer = 0.0f;
		_jump_lock = jump_lock_time;
		_air_jumps_left = air_jumps; // refill for the double jump
		change_state(fall_state);
		return;
	}

	// 4. Wall jump: airborne, pressed against a wall, buffered jump -> straight up
	//    (no climb / slide / mantle; horizontal speed is preserved).
	Vector3 wall_in(-_wall_normal.x, 0.0f, -_wall_normal.z);
	bool into_wall = _has_wall && wall_in.length() > 0.01f && _wish.dot(wall_in.normalized()) > 0.1f;
	if (_jump_buffer_timer > 0.0f && !_grounded && into_wall) {
		// Launch AWAY from the wall + up (like CelesteController): leaving the face is what
		// stops it clinging. Tap jump against the wall again to hop up it repeatedly.
		Vector3 out = _wall_normal * wall_jump_out;
		_vel = Vector3(out.x, wall_jump_up, out.z);
		_jump_buffer_timer = 0.0f;
		_jump_lock = jump_lock_time;
		_air_jumps_left = air_jumps;
		change_state(fall_state);
		return;
	}

	// 5. Grounded default (refills air jumps once settled).
	if (_grounded && _jump_lock <= 0.0f) {
		_air_jumps_left = air_jumps;
		change_state(walk_state);
		return;
	}

	// 6. Airborne: spend an air jump if one is buffered, then fall.
	if (_jump_buffer_timer > 0.0f && _air_jumps_left > 0) {
		_vel.y = _jump_velocity;
		_jump_buffer_timer = 0.0f;
		_air_jumps_left -= 1;
		_jump_lock = jump_lock_time;
	}
	change_state(fall_state);
}
// Grounded vertical servo: drive `_vel.y` so the body sits exactly at ride_height
// (planted, no float / overshoot), and press the ground body down so the character
// still pushes platforms and cars it stands on.
void SpringCharacter::_apply_ground_servo() {
	if (!_has_support || _jump_lock > 0.0f) {
		return; // off briefly after a jump so it doesn't eat the launch
	}

	RigidBody3D *hit_body = Object::cast_to<RigidBody3D>(_ground_body);
	if (hit_body) {
		Vector3 rel = _ground_point - hit_body->get_global_position();
		hit_body->apply_force(Vector3(0.0f, -1.0f, 0.0f) * get_mass() * ground_press, rel);
	}
}

// Updates the heading goal `_face_yaw` only. The physics body's rotation is locked
// (an animated mesh will own facing/lean), and the visual mesh is pinned to _face_yaw
// in _physics_process, so there is no torque to apply here any more.
void SpringCharacter::_apply_upright_and_facing(const Vector3 &p_dir) {
	if (!_face_init) {
		_face_yaw = get_global_transform().basis.get_euler().y;
		_face_init = true;
	}

	// Only update the heading when there is a real direction; otherwise HOLD the last
	// heading so idle facing is steady. EASE toward the travel yaw rather than snapping.
	Vector3 flat(p_dir.x, 0.0f, p_dir.z);
	if (flat.length() > 0.1f) {
		float target = std::atan2(-flat.x, -flat.z);
		float diff = UtilityFunctions::wrapf(target - _face_yaw, -(float)Math::PI, (float)Math::PI);
		float t = 1.0f - std::exp(-face_turn_rate * _dt);
		_face_yaw += diff * t;
	}
}

// Deterministic horizontal movement: ramp the velocity straight toward the goal
// (faster when reversing). No force accumulation — the velocity IS the motion.
void SpringCharacter::_apply_movement(float p_dt) {
	// Ramp the horizontal velocity toward the desired velocity (_move_target),
	// faster when reversing so turns are tight. _move_target is built per control
	// scheme at the top of _physics_process.
	Vector3 flat(_vel.x, 0.0f, _vel.z);
	Vector3 tdir(_move_target.x, 0.0f, _move_target.z);
	float vel_dot = 0.0f;
	if (flat.length() > 0.01f && tdir.length() > 0.01f) {
		vel_dot = tdir.normalized().dot(flat.normalized());
	}
	float accel = acceleration * _accel_factor(vel_dot, accel_turn_boost);

	_vel.x = Math::move_toward(_vel.x, _move_target.x, accel * p_dt);
	_vel.z = Math::move_toward(_vel.z, _move_target.z, accel * p_dt);
}

// Self-integrated asymmetric gravity (the Mario arc): lighter rising, heavier
// falling, heavier still when the jump is released early (short hop). Clamped to
// a terminal speed. Deterministic because we integrate it, not the solver.
void SpringCharacter::_apply_air_gravity(bool p_jump_held, float p_dt) {
	float g;
	if (_vel.y > 0.0f) {
		g = p_jump_held ? _jump_gravity : _jump_gravity * low_jump_mult;
	} else {
		g = _fall_gravity;
	}
	_vel.y -= g * p_dt;
	if (_vel.y < -terminal_velocity) {
		_vel.y = -terminal_velocity;
	}
}

// Celeste corner correction: while rising, if the head is about to clip a ledge
// but a small sideways offset is clear, nudge that way so we slip past the corner
// instead of bonking and losing the jump. Does nothing under a solid ceiling.
void SpringCharacter::_apply_corner_correction() {
	if (_vel.y <= 0.5f) {
		return; // only while rising with real upward momentum
	}
	Vector3 origin = get_global_position();
	Vector3 head_o = origin + Vector3(0.0f, capsule_height * 0.5f, 0.0f);
	Vector3 up(0.0f, corner_probe_up, 0.0f);
	TypedArray<RID> exclude;
	exclude.push_back(get_rid());

	MCRaycastHit center = raycast_3d(this, head_o, head_o + up, 0xFFFFFFFF, exclude);
	if (!center.is_hit) {
		return; // head is clear, nothing to correct
	}

	const Vector3 dirs[4] = {
		Vector3(1.0f, 0.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, 0.0f, -1.0f)
	};
	for (int i = 0; i < 4; i++) {
		Vector3 off = dirs[i] * corner_correct_dist;
		MCRaycastHit probe = raycast_3d(this, head_o + off, head_o + off + up, 0xFFFFFFFF, exclude);
		if (!probe.is_hit) {
			// Clear on this side: nudge toward it, preserving upward momentum.
			_vel.x += dirs[i].x * corner_push;
			_vel.z += dirs[i].z * corner_push;
			return; // one correction per frame
		}
	}
}


void SpringCharacter::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	float dt = (float)delta;
	_dt = dt;

	// 1. Snapshot input — only while we are the active target (GameManager cycles
	//    targets on TAB). Inactive -> zero input, so the body just holds position.
	GameManager *gm = GameManager::get_singleton();
	bool active = gm && gm->is_active(this);

	Vector2 move_axis(0.0f, 0.0f);
	_in_jump_pressed = false;
	_in_jump_held = false;
	_in_dash_pressed = false;
	_move_strength = 0.0f;
	if (active && player_input) {
		const ActionState &st = player_input->get_state();
		move_axis = st.character.move_axis;
		_in_jump_pressed = st.character.jump_just_pressed;
		_in_jump_held = st.character.jump;
		_in_dash_pressed = st.character.dash_just_pressed;
		_move_strength = player_input->get_movement_strength(sprint_multiplier);
	}

	// Steering movement: Left/Right rotate the heading, Up/Down drive along it.
	_face_yaw -= move_axis.x * steer_rate * dt; // right = clockwise
	float cy = std::cos(_face_yaw);
	float sy = std::sin(_face_yaw);
	Vector3 fwd(-sy, 0.0f, -cy); // body -Z at this yaw
	float throttle = -move_axis.y; // up = forward (+)
	float sprint_factor = (move_axis.length() > 0.01f) ? (_move_strength / move_axis.length()) : 1.0f;
	float sp = max_speed * sprint_factor * (throttle >= 0.0f ? 1.0f : reverse_speed_mult);
	_move_target = fwd * (throttle * sp);
	_wish = (std::abs(throttle) > 0.1f) ? (throttle > 0.0f ? fwd : -fwd) : Vector3(0.0f, 0.0f, 0.0f);
	_face_dir = Vector3(0.0f, 0.0f, 0.0f); // facing is driven directly by steering above

	// Lock the follow-cam behind the steered heading so it rotates WITH the character.
	if (active) {
		GameCamera *cam = gm ? gm->get_camera() : nullptr;
		if (cam) {
			cam->set_heading_follow(true, _face_yaw);
		}
	}

	// 2. Probe the world, seed the working velocity, advance timers.
	_update_ride_height(dt);
	_cast_ground();
	_cast_wall(_wish);
	_vel = get_linear_velocity() - _platform_velocity; // work in platform-relative space
	_tick_timers(dt);
	_update_jump_timers(dt);

	// 3. Decide the state (may set a launch velocity), let it shape `_vel`, commit.
	_update_transitions();
	if (current_state) {
		current_state->physics_update(dt);
	}
	set_linear_velocity(_vel + _platform_velocity); // carry the moving platform

	// Keep the visual (capsule + orange nose) pinned to the canonical heading so it
	// matches the camera / move-forward / dash direction exactly. The physics body's
	// own yaw is a PD-spring proxy that lags and overshoots _face_yaw by ~20deg, which
	// is what made the nose flicker; decoupling the mesh removes that wobble entirely.
	if (_mesh) {
		Transform3D mt = _mesh->get_global_transform();
		mt.basis = Basis(Vector3(0.0f, 1.0f, 0.0f), _face_yaw);
		_mesh->set_global_transform(mt);
	}

	_debug_draw_trajectory(dt);

	if (ui_helper) {
		Vector3 hv = get_linear_velocity();
		hv.y = 0.0f;
		ui_helper->update_graph(hv.length());
	}
}

void SpringCharacter::_debug_draw_trajectory(float p_delta) {
	DebugManager *dm = DebugManager::get_singleton();
	if (!dm) {
		return;
	}
	String id = "traj_" + get_name();
	if (debug_trajectory) {
		// Same cyan trail the car leaves: record the position every 0.1s and connect the dots.
		dm->draw_trajectory(id, get_global_transform().origin, p_delta, 0.1f, 200, Color(0.1f, 0.9f, 0.9f));
	} else {
		dm->clear_trajectory(id);
	}
}

void SpringCharacter::_on_ui_toggle() {
	if (ui_helper) {
		ui_helper->toggle_visibility();
	}
}

void SpringCharacter::_on_ui_slider_value_changed(double p_value, String p_property) {
	std::map<String, float *>::iterator it = ui_vars.find(p_property);
	if (it != ui_vars.end()) {
		*(it->second) = (float)p_value;
		if (p_property == String("jump_height") || p_property == String("jump_time_to_peak") ||
				p_property == String("jump_time_to_descent")) {
			_recompute_jump();
		}
	}
}

float SpringCharacter::get_ui_var(const String &p_name) const {
	std::map<String, float *>::const_iterator it = ui_vars.find(p_name);
	return it != ui_vars.end() ? *(it->second) : 0.0f;
}

void SpringCharacter::save_settings() {
	Ref<ConfigFile> config;
	config.instantiate();
	for (std::map<String, float *>::const_iterator it = ui_vars.begin(); it != ui_vars.end(); ++it) {
		config->set_value("character", it->first, *(it->second));
	}
	config->save("user://character_settings.cfg");
	UtilityFunctions::print("SpringCharacter: settings saved to user://character_settings.cfg");
}

void SpringCharacter::load_settings() {
	Ref<ConfigFile> config;
	config.instantiate();
	if (config->load("user://character_settings.cfg") != OK) {
		return;
	}
	for (std::map<String, float *>::iterator it = ui_vars.begin(); it != ui_vars.end(); ++it) {
		*(it->second) = (float)config->get_value("character", it->first, *(it->second));
	}
	_recompute_jump();
	if (ui_root) {
		for (std::map<String, float *>::const_iterator it = ui_vars.begin(); it != ui_vars.end(); ++it) {
			ui_root->set_value(it->first, *(it->second));
		}
	}
	UtilityFunctions::print("SpringCharacter: settings loaded from user://character_settings.cfg");
}

} // namespace godot
