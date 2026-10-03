#include "spring_character.h"

#include "ai/character_states.h"


#include "../camera/camera.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "../utils/raycast/mc_raycast.h"
#include "../interaction/environment/moving_platform.h"
#include "bubble/bubble.h"
#include "../debug_draw/debug_manager.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/physics_material.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

namespace godot {

SpringCharacter::SpringCharacter() {}

void SpringCharacter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_speed", "v"), &SpringCharacter::set_max_speed);
	ClassDB::bind_method(D_METHOD("get_max_speed"), &SpringCharacter::get_max_speed);
	ClassDB::bind_method(D_METHOD("set_control_scheme", "v"), &SpringCharacter::set_control_scheme);
	ClassDB::bind_method(D_METHOD("get_control_scheme"), &SpringCharacter::get_control_scheme);
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

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_speed"), "set_max_speed", "get_max_speed");
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "control_scheme", PROPERTY_HINT_ENUM, "Steer,Camera Relative"),
			"set_control_scheme", "get_control_scheme"
	);
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

	BIND_ENUM_CONSTANT(CONTROL_STEER);
	BIND_ENUM_CONSTANT(CONTROL_CAMERA_RELATIVE);
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
	// Continuous collision detection: a physics-level backstop so a fast fall can never
	// tunnel straight through a thin floor between two steps.
	set_use_continuous_collision_detection(true);
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

	_find_skin();
	_audio.setup(this, "res://assets/character/sounds/");

	_setup_tuning();
}

void SpringCharacter::_exit_tree() {
	GameManager *gm = GameManager::get_singleton();
	if (gm && gm->get_spring_character() == this) {
		gm->register_spring_character(nullptr);
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
	// Only probe while we actually have ground under us (needs last frame's clearance).
	if (_has_support && hvel.length() > 0.5f) {
		Vector3 up(0.0f, 1.0f, 0.0f);
		Basis hb(up, _face_yaw);
		Vector3 fwd = -hb.get_column(2); // heading forward
		Vector3 sole = get_global_position() - Vector3(0.0f, capsule_height * 0.5f, 0.0f);
		Vector3 ground = sole - up * _ground_distance; // the sole HOVERS; measure from the real floor

		TypedArray<RID> exclude;
		exclude.push_back(get_rid());

		// LOW ray just above the floor catches any riser taller than ~0.1 m. HIGH ray at the
		// height the sole would clear after rising: clear there = short enough to float over.
		Vector3 low = ground + up * 0.1f;
		Vector3 high = ground + up * (max_ride_height + 0.1f);
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
	// Reach ride_height+extra below the sole, and further the faster we fall so a high-speed
	// landing is seen a frame early (45 m/s is 0.75 m per step: more than the base reach).
	float fall_speed = MAX(0.0f, -get_linear_velocity().y);
	float max_len = skin + ride_height + ride_ray_extra + fall_speed * _dt * 2.0f;
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
		Bubble *bubble = Object::cast_to<Bubble>(_ground_body);
		if (mp) {
			_platform_velocity = mp->get_velocity();
		} else if (bubble) {
			_platform_velocity = bubble->get_velocity(); // ride it as it floats
		} else {
			_platform_velocity = Vector3(0.0f, 0.0f, 0.0f);
		}
		_grounded = _ground_distance <= ride_height + 0.15f;
		if (bubble && _grounded) {
			bubble->notify_stood_on(); // it dips under her weight
		}
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
	if (_bubble_cd > 0.0f) {
		_bubble_cd -= p_dt;
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
	//    direction becomes a ground pound; otherwise a dash. Only ONE dash per
	//    airtime (A Hat in Time's dive rule), so the jump chain's reach stays bounded.
	if (_in_dash_pressed && _dash_cd_timer <= 0.0f) {
		if (!_grounded && _wish.length() < 0.1f) {
			change_state(pound_state);
			return;
		}
		if (_grounded || !_air_dash_used) {
			_air_dash_used = !_grounded;
			change_state(dash_state);
			return;
		}
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
		_air_dash_used = false;
		change_state(fall_state);
		return;
	}

	// 5. Grounded default (refills air jumps once settled).
	if (_grounded && _jump_lock <= 0.0f) {
		_air_jumps_left = air_jumps;
		_air_dash_used = false;
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
	// The "bottom spring": drive vertical velocity to close the gap between the sole's
	// clearance and ride_height. Proportional and clamped -> planted, no float.
	float error = ride_height - _ground_distance; // >0 = too low, must rise
	_vel.y = CLAMP(error * ride_follow, -ride_max_speed, ride_max_speed);

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
// Predictive landing clamp. While airborne with ground in reach, cap the downward
// speed so this physics step lands the sole exactly at ride_height instead of
// overshooting it (and the floor). Speed-independent, so a 9 m jump lands as
// cleanly as a hop; it replaces the old fixed-threshold land snap.
void SpringCharacter::_apply_landing_guard(float p_delta) {
	if (!_has_support || _vel.y >= 0.0f || p_delta <= 0.0f) {
		return;
	}
	float room = _ground_distance - ride_height; // how much further the sole may drop
	if (room <= 0.0f) {
		_vel.y = 0.0f; // already at / under ride height: stop sinking, let the servo take it
		return;
	}
	float max_down = room / p_delta;
	if (-_vel.y > max_down) {
		_vel.y = -max_down;
	}
}

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


// A skin is any Node3D child that speaks the intent API (idle/move/jump/fall). When one
// is present the placeholder capsule + nose are hidden and the skin takes over the look.
void SpringCharacter::_find_skin() {
	_skin = nullptr;
	int n = get_child_count();
	for (int i = 0; i < n; i++) {
		Node3D *c = Object::cast_to<Node3D>(get_child(i));
		if (c && c != _mesh && _animator.set_skin(c)) {
			_skin = c;
			break;
		}
	}
	if (_skin && _mesh) {
		_mesh->set_visible(false);
	}
	UtilityFunctions::print(
			_skin ? String("SpringCharacter: skin bound -> ") + _skin->get_name()
				  : String("SpringCharacter: no skin child, using placeholder capsule")
	);
	_prev_face_yaw = _face_yaw;
}

// Bubble Wand: blow a bubble out ahead along the heading. Over the cap, the oldest pops.
void SpringCharacter::_blow_bubble() {
	Node *parent = get_parent();
	if (!parent || !is_inside_tree()) {
		return;
	}
	TypedArray<Node> alive = get_tree()->get_nodes_in_group("bubbles");
	Bubble *oldest = nullptr;
	int live = 0;
	for (int i = 0; i < alive.size(); i++) {
		Bubble *b = Object::cast_to<Bubble>(alive[i]);
		if (b && !b->is_popping()) {
			live++;
			if (!oldest || b->get_age() > oldest->get_age()) {
				oldest = b;
			}
		}
	}
	if (live >= max_bubbles && oldest) {
		oldest->pop();
	}

	Vector3 fwd(-std::sin(_face_yaw), 0.0f, -std::cos(_face_yaw));
	// Far enough ahead that the bubble (r 0.8) clears the capsule (r 0.4), at chest height.
	Vector3 origin = get_global_position() + fwd * 1.6f + Vector3(0.0f, 0.4f, 0.0f);
	Bubble *bubble = memnew(Bubble);
	parent->add_child(bubble);
	bubble->launch(origin, fwd);
	_bubble_cd = bubble_cooldown;
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

	// Live toggle for experimenting: V flips between the two control schemes
	// (Steer + heading-locked cam  <->  Camera-relative + free Odyssey cam).
	{
		Input *in = Input::get_singleton();
		bool v_down = active && in && in->is_physical_key_pressed(KEY_V);
		if (v_down && !_scheme_key_was_down) {
			control_scheme = (control_scheme == CONTROL_STEER) ? CONTROL_CAMERA_RELATIVE : CONTROL_STEER;
			UtilityFunctions::print(
					String("SpringCharacter: control scheme -> ") +
					(control_scheme == CONTROL_STEER ? "STEER (heading cam)" : "CAMERA-RELATIVE (platformer cam)")
			);
		}
		_scheme_key_was_down = v_down;
	}

	GameCamera *cam = (active && gm) ? gm->get_camera() : nullptr;
	float sprint_factor = (move_axis.length() > 0.01f) ? (_move_strength / move_axis.length()) : 1.0f;

	if (control_scheme == CONTROL_STEER) {
		// Steering movement: rotate the heading, Up/Down drive along it. The MOUSE steers
		// with priority: on any frame it moves, it rotates the heading (character + camera
		// together) and A/D are ignored; A/D only steer when the mouse is still.
		//
		// Back input (S, S+A, S+D) is the exception: like the platformer scheme the character
		// turns round and runs TOWARD the camera (diagonally with A/D), while the camera holds
		// the yaw it had when S went down. Locking the camera behind the heading here would
		// swing it round with the character and S would point somewhere new every frame.
		if (!_steer_target_valid) {
			_steer_yaw_target = _face_yaw; // (re)entering steer: start from the current heading
			_steer_target_valid = true;
		}
		float mouse_x = 0.0f;
		if (active && player_input) {
			mouse_x = player_input->get_state().camera.look_delta.x;
		}
		bool mouse_moved = std::abs(mouse_x) > 0.01f && cam;

		bool backing = move_axis.y > 0.3f; // S held (keyboard 1.0, analog stick pulled back)
		bool forward = move_axis.y < -0.1f; // W held
		if (backing && !_hold_cam) {
			_back_cam_yaw = cam ? cam->get_yaw() : _face_yaw; // freeze the view where it is
		}
		_steer_backing = backing;
		// The freeze outlives the S press: only driving forward again recenters the camera.
		if (backing) {
			_hold_cam = true;
		} else if (forward) {
			_hold_cam = false;
		}

		Vector3 back_dir(0.0f, 0.0f, 0.0f);
		float back_mag = 0.0f;
		if (backing) {
			if (mouse_moved) {
				_back_cam_yaw -= mouse_x * cam->get_orbit_sensitivity(); // mouse still looks around
			}
			// Camera-relative direction off the FROZEN camera yaw: S = toward the camera.
			Vector3 cam_fwd(-std::sin(_back_cam_yaw), 0.0f, -std::cos(_back_cam_yaw));
			Vector3 cam_right(std::cos(_back_cam_yaw), 0.0f, -std::sin(_back_cam_yaw));
			back_dir = cam_right * move_axis.x + cam_fwd * (-move_axis.y);
			back_mag = CLAMP(back_dir.length(), 0.0f, 1.0f);
			if (back_mag > 0.001f) {
				back_dir = back_dir / back_dir.length();
				_steer_yaw_target = std::atan2(-back_dir.x, -back_dir.z); // face where we run
			}
		} else if (mouse_moved && _hold_cam) {
			_back_cam_yaw -= mouse_x * cam->get_orbit_sensitivity(); // camera frozen: mouse just looks
		} else if (mouse_moved) {
			_steer_yaw_target -= mouse_x * cam->get_orbit_sensitivity(); // mouse wins
		} else {
			_steer_yaw_target -= move_axis.x * steer_rate * dt; // keys: right = clockwise
		}
		// Ease the heading toward the target. Mouse deltas are bursty integer pixels relative
		// to the physics tick; this low-pass turns them into a smooth turn (same rate the
		// camera-relative scheme uses for facing, so face_turn_rate governs both).
		{
			float diff = UtilityFunctions::wrapf(_steer_yaw_target - _face_yaw, -(float)Math::PI, (float)Math::PI);
			_face_yaw += diff * (1.0f - std::exp(-face_turn_rate * dt));
			_steer_yaw_target = UtilityFunctions::wrapf(_steer_yaw_target, -(float)Math::PI, (float)Math::PI);
		}

		if (backing) {
			_move_target = back_dir * (back_mag * max_speed * sprint_factor);
			_wish = back_dir;
		} else {
			float cy = std::cos(_face_yaw);
			float sy = std::sin(_face_yaw);
			Vector3 fwd(-sy, 0.0f, -cy); // body -Z at this yaw
			float throttle = -move_axis.y; // up = forward (+)
			float sp = max_speed * sprint_factor * (throttle >= 0.0f ? 1.0f : reverse_speed_mult);
			_move_target = fwd * (throttle * sp);
			_wish = (std::abs(throttle) > 0.1f) ? (throttle > 0.0f ? fwd : -fwd) : Vector3(0.0f, 0.0f, 0.0f);
		}
		_face_dir = Vector3(0.0f, 0.0f, 0.0f); // facing is driven directly by steering above

		// Lock the follow-cam behind the steered heading so it rotates WITH the character;
		// after a backward run it holds the frozen yaw until the player drives forward.
		if (cam) {
			cam->set_camera_mode(GameCamera::MODE_CHARACTER);
			cam->set_heading_follow(true, _hold_cam ? _back_cam_yaw : _face_yaw);
		}
	} else {
		// Camera-relative movement (Odyssey / A Hat in Time): the stick is a direction in
		// SCREEN space. Project it onto the camera's ground plane; the character turns to
		// face wherever it is moving. The camera is free and frames the scene itself.
		Vector3 cam_fwd(0.0f, 0.0f, -1.0f);
		Vector3 cam_right(1.0f, 0.0f, 0.0f);
		if (cam) {
			Basis cb = cam->get_global_transform().basis;
			cam_fwd = -cb.get_column(2);
			cam_right = cb.get_column(0);
		}
		cam_fwd.y = 0.0f;
		cam_right.y = 0.0f;
		if (cam_fwd.length() > 0.001f) {
			cam_fwd = cam_fwd.normalized();
		}
		if (cam_right.length() > 0.001f) {
			cam_right = cam_right.normalized();
		}
		Vector3 dir = cam_right * move_axis.x + cam_fwd * (-move_axis.y); // up on the stick = away from camera
		float mag = CLAMP(dir.length(), 0.0f, 1.0f);
		if (mag > 0.001f) {
			dir = dir / dir.length();
		}
		_move_target = dir * (mag * max_speed * sprint_factor);
		_wish = (mag > 0.1f) ? dir : Vector3(0.0f, 0.0f, 0.0f);
		_face_dir = _wish; // facing eases toward the move direction (face_turn_rate)
		_steer_target_valid = false; // so switching back to steer starts from the live heading
		_steer_backing = false;
		_hold_cam = false;

		if (cam) {
			cam->set_camera_mode(GameCamera::MODE_PLATFORMER);
			cam->set_heading_follow(false, 0.0f);
		}
	}

	// 2. Probe the world, seed the working velocity, advance timers.
	_update_ride_height(dt);
	_cast_ground();
	_cast_wall(_wish);
	_vel = get_linear_velocity() - _platform_velocity; // work in platform-relative space
	_tick_timers(dt);
	if (active && _bubble_cd <= 0.0f && Input::get_singleton()->is_action_just_pressed("bubble")) {
		_blow_bubble();
	}
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
	if (_skin) {
		// Local rotation only (the body's rotation is locked), so the skin's scale survives.
		_skin->set_rotation(Vector3(0.0f, _face_yaw + skin_yaw_offset, 0.0f));

		// Lean into turns: normalised yaw rate (right turn = +). Works for every scheme.
		float yaw_rate = (dt > 0.0f) ? UtilityFunctions::wrapf(_face_yaw - _prev_face_yaw, -Math::PI, Math::PI) / dt : 0.0f;
		float lean = (steer_rate > 0.001f) ? CLAMP(-yaw_rate / steer_rate, -1.0f, 1.0f) : 0.0f;
		Vector3 hv(_vel.x, 0.0f, _vel.z);
		_animator.update(_grounded, _vel.y, hv.length(), lean);
	}
	_prev_face_yaw = _face_yaw;

	// Sounds: a jump is the frame _jump_lock gets armed (ground, wall or air jump alike).
	{
		bool jumped = (_prev_jump_lock <= 0.0f) && (_jump_lock > 0.0f);
		_prev_jump_lock = _jump_lock;
		Vector3 hv(_vel.x, 0.0f, _vel.z);
		_audio.update(_grounded, _vel.y, hv.length(), jumped, dt);
	}

	_debug_draw_trajectory(dt);

	{
		Vector3 hv = get_linear_velocity();
		hv.y = 0.0f;
		tuning.push_graph(hv.length());
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

/**
 * @brief Describe this character's tunables for the shared TuningPanel, apply saved
 * values (user://character_settings.cfg) and add the "Spring Character" tab.
 */
void SpringCharacter::_setup_tuning() {
	tuning.begin("Spring Character", "user://character_settings.cfg", "character", [this]() { _recompute_jump(); });
	tuning.header("Movement");
	tuning.slider("Max Speed", "max_speed", &max_speed, 1.0f, 40.0f, 0.5f);
	tuning.slider("Acceleration", "acceleration", &acceleration, 10.0f, 300.0f, 5.0f);
	tuning.slider("Sprint Mult", "sprint_multiplier", &sprint_multiplier, 1.0f, 3.0f, 0.1f);
	tuning.header("Steering / Turn");
	tuning.slider("Steer Rate", "steer_rate", &steer_rate, 0.5f, 8.0f, 0.1f);
	tuning.slider("Face Turn Rate", "face_turn_rate", &face_turn_rate, 2.0f, 30.0f, 0.5f);
	tuning.header("Jump / Gravity");
	tuning.slider("Jump Height", "jump_height", &jump_height, 0.5f, 12.0f, 0.1f);
	tuning.slider("Time to Peak", "jump_time_to_peak", &jump_time_to_peak, 0.1f, 1.0f, 0.01f);
	tuning.slider("Time to Descent", "jump_time_to_descent", &jump_time_to_descent, 0.1f, 1.0f, 0.01f);
	tuning.slider("Low Jump Mult", "low_jump_mult", &low_jump_mult, 1.0f, 5.0f, 0.1f);
	tuning.slider("Terminal Velocity", "terminal_velocity", &terminal_velocity, 10.0f, 100.0f, 1.0f);
	tuning.slider("Coyote Time", "coyote_time", &coyote_time, 0.0f, 0.5f, 0.01f);
	tuning.slider("Jump Buffer", "jump_buffer", &jump_buffer, 0.0f, 0.5f, 0.01f);
	tuning.header("Ride Servo");
	tuning.slider("Min Ride Height", "min_ride_height", &min_ride_height, 0.0f, 0.6f, 0.05f);
	tuning.slider("Max Ride Height", "max_ride_height", &max_ride_height, 0.2f, 1.2f, 0.05f);
	tuning.slider("Ride Adjust Speed", "ride_height_speed", &ride_height_speed, 1.0f, 20.0f, 0.5f);
	tuning.slider("Ride Follow", "ride_follow", &ride_follow, 2.0f, 40.0f, 1.0f);
	tuning.header("Dash / Pound");
	tuning.slider("Dash Speed", "dash_speed", &dash_speed, 5.0f, 60.0f, 1.0f);
	tuning.slider("Dash Cooldown", "dash_cooldown", &dash_cooldown, 0.1f, 1.5f, 0.05f);
	tuning.slider("Pound Speed", "pound_speed", &pound_speed, 5.0f, 60.0f, 1.0f);
	tuning.header("Wall");
	tuning.slider("Wall Jump Up", "wall_jump_up", &wall_jump_up, 2.0f, 30.0f, 0.5f);
	tuning.slider("Wall Jump Out", "wall_jump_out", &wall_jump_out, 0.0f, 20.0f, 0.5f);
	tuning.graph("Speed", 0.0f, 40.0f);
	tuning.load();
	tuning.attach();
}

} // namespace godot
