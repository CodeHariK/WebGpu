// GameCamera implementation — see camera.h for the rig overview and the doc
// comments on every member and method. This file holds the Godot bindings, the
// mode-switching state machine, and the shared follow maths the states call.
//
// Layout:
//   _bind_methods           - expose properties / methods / Mode enum to Godot
//   lifecycle               - _ready / _exit_tree / _physics_process
//   set_camera_mode         - swap the active CameraState (exit old, enter new)
//   shared follow helpers   - rebase_springs / base_offset_dir /
//                             smooth_look_angles / orbit_position /
//                             resolve_follow_distance / apply_position
//   collision + targeting   - _solve_collision / follow-target setters /
//                             get_center_raycast_hit / get_current_target_distance

#include "camera.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "effects/speed_lines.h"

#include "states/car_state.h"
#include "states/fixed_state.h"
#include "states/fly_state.h"
#include "states/tps_state.h"

#include "../utils/raycast/mc_raycast.h"
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

namespace godot {

void GameCamera::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_camera_mode", "mode"), &GameCamera::set_camera_mode);
	ClassDB::bind_method(D_METHOD("get_camera_mode"), &GameCamera::get_camera_mode);
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "camera_mode", PROPERTY_HINT_ENUM, "Fly,Car,TPS,Fixed"), "set_camera_mode",
			"get_camera_mode"
	);

	ClassDB::bind_method(D_METHOD("set_follow_target_path", "path"), &GameCamera::set_follow_target_path);
	ClassDB::bind_method(D_METHOD("get_follow_target_path"), &GameCamera::get_follow_target_path);
	ADD_PROPERTY(
			PropertyInfo(Variant::NODE_PATH, "follow_target_path"), "set_follow_target_path", "get_follow_target_path"
	);

	ClassDB::bind_method(D_METHOD("set_pos_smoothing_enabled", "enabled"), &GameCamera::set_pos_smoothing_enabled);
	ClassDB::bind_method(D_METHOD("is_pos_smoothing_enabled"), &GameCamera::is_pos_smoothing_enabled);
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "pos_smoothing_enabled"), "set_pos_smoothing_enabled",
			"is_pos_smoothing_enabled"
	);

	ClassDB::bind_method(D_METHOD("set_frequency", "frequency"), &GameCamera::set_frequency);
	ClassDB::bind_method(D_METHOD("get_frequency"), &GameCamera::get_frequency);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frequency"), "set_frequency", "get_frequency");

	ClassDB::bind_method(D_METHOD("set_damping", "damping"), &GameCamera::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &GameCamera::get_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping"), "set_damping", "get_damping");

	ClassDB::bind_method(D_METHOD("set_response", "response"), &GameCamera::set_response);
	ClassDB::bind_method(D_METHOD("get_response"), &GameCamera::get_response);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "response"), "set_response", "get_response");

	ClassDB::bind_method(D_METHOD("set_pan_speed", "speed"), &GameCamera::set_pan_speed);
	ClassDB::bind_method(D_METHOD("get_pan_speed"), &GameCamera::get_pan_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pan_speed"), "set_pan_speed", "get_pan_speed");

	ClassDB::bind_method(D_METHOD("set_zoom_speed", "speed"), &GameCamera::set_zoom_speed);
	ClassDB::bind_method(D_METHOD("get_zoom_speed"), &GameCamera::get_zoom_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "zoom_speed"), "set_zoom_speed", "get_zoom_speed");

	ClassDB::bind_method(D_METHOD("set_orbit_sensitivity", "sensitivity"), &GameCamera::set_orbit_sensitivity);
	ClassDB::bind_method(D_METHOD("get_orbit_sensitivity"), &GameCamera::get_orbit_sensitivity);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "orbit_sensitivity"), "set_orbit_sensitivity", "get_orbit_sensitivity");

	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &GameCamera::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("is_collision_enabled"), &GameCamera::is_collision_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "is_collision_enabled");

	ClassDB::bind_method(D_METHOD("set_follow_offset", "offset"), &GameCamera::set_follow_offset);
	ClassDB::bind_method(D_METHOD("get_follow_offset"), &GameCamera::get_follow_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "follow_offset"), "set_follow_offset", "get_follow_offset");

	ClassDB::bind_method(D_METHOD("set_min_distance", "distance"), &GameCamera::set_min_distance);
	ClassDB::bind_method(D_METHOD("get_min_distance"), &GameCamera::get_min_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_distance"), "set_min_distance", "get_min_distance");

	ClassDB::bind_method(D_METHOD("set_max_distance", "distance"), &GameCamera::set_max_distance);
	ClassDB::bind_method(D_METHOD("get_max_distance"), &GameCamera::get_max_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_distance"), "set_max_distance", "get_max_distance");

	ClassDB::bind_method(D_METHOD("set_dynamic_zoom_enabled", "enabled"), &GameCamera::set_dynamic_zoom_enabled);
	ClassDB::bind_method(D_METHOD("is_dynamic_zoom_enabled"), &GameCamera::is_dynamic_zoom_enabled);
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "dynamic_zoom_enabled"), "set_dynamic_zoom_enabled", "is_dynamic_zoom_enabled"
	);

	ClassDB::bind_method(D_METHOD("set_speed_threshold", "threshold"), &GameCamera::set_speed_threshold);
	ClassDB::bind_method(D_METHOD("get_speed_threshold"), &GameCamera::get_speed_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_threshold"), "set_speed_threshold", "get_speed_threshold");

	ClassDB::bind_method(
			D_METHOD("set_dynamic_zoom_extra_distance", "distance"), &GameCamera::set_dynamic_zoom_extra_distance
	);
	ClassDB::bind_method(D_METHOD("get_dynamic_zoom_extra_distance"), &GameCamera::get_dynamic_zoom_extra_distance);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "dynamic_zoom_extra_distance"), "set_dynamic_zoom_extra_distance",
			"get_dynamic_zoom_extra_distance"
	);

	ClassDB::bind_method(D_METHOD("set_max_speed_for_zoom", "speed"), &GameCamera::set_max_speed_for_zoom);
	ClassDB::bind_method(D_METHOD("get_max_speed_for_zoom"), &GameCamera::get_max_speed_for_zoom);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "max_speed_for_zoom"), "set_max_speed_for_zoom", "get_max_speed_for_zoom"
	);

	ClassDB::bind_method(D_METHOD("set_car_look_ahead", "amount"), &GameCamera::set_car_look_ahead);
	ClassDB::bind_method(D_METHOD("get_car_look_ahead"), &GameCamera::get_car_look_ahead);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "car_look_ahead"), "set_car_look_ahead", "get_car_look_ahead");

	ClassDB::bind_method(D_METHOD("set_fixed_deadzone", "radius"), &GameCamera::set_fixed_deadzone);
	ClassDB::bind_method(D_METHOD("get_fixed_deadzone"), &GameCamera::get_fixed_deadzone);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fixed_deadzone"), "set_fixed_deadzone", "get_fixed_deadzone");

	ClassDB::bind_method(D_METHOD("add_trauma", "amount"), &GameCamera::add_trauma);

	ClassDB::bind_method(D_METHOD("set_car_fov_base", "fov"), &GameCamera::set_car_fov_base);
	ClassDB::bind_method(D_METHOD("get_car_fov_base"), &GameCamera::get_car_fov_base);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "car_fov_base"), "set_car_fov_base", "get_car_fov_base");

	ClassDB::bind_method(D_METHOD("set_car_fov_speed_add", "degrees"), &GameCamera::set_car_fov_speed_add);
	ClassDB::bind_method(D_METHOD("get_car_fov_speed_add"), &GameCamera::get_car_fov_speed_add);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "car_fov_speed_add"), "set_car_fov_speed_add", "get_car_fov_speed_add"
	);

	ClassDB::bind_method(D_METHOD("set_speed_lines_path", "path"), &GameCamera::set_speed_lines_path);
	ClassDB::bind_method(D_METHOD("get_speed_lines_path"), &GameCamera::get_speed_lines_path);
	ADD_PROPERTY(
			PropertyInfo(Variant::NODE_PATH, "speed_lines_path"), "set_speed_lines_path", "get_speed_lines_path"
	);

	BIND_ENUM_CONSTANT(MODE_FLY);
	BIND_ENUM_CONSTANT(MODE_CAR);
	BIND_ENUM_CONSTANT(MODE_TPS);
	BIND_ENUM_CONSTANT(MODE_FIXED);
}

GameCamera::GameCamera() {
	frequency = 3.0f;
	damping = 1.0f;
	response = 0.0f;
	shake.init();
}

GameCamera::~GameCamera() {}

// Resolve target, register with the GameManager, seat every spring on the
// current transform, then enter the initial mode.
void GameCamera::_ready() {
	if (Engine::get_singleton()->is_editor_hint())
		return;
	_update_follow_node();

	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_camera(this);
	}

	if (follow_offset.length_squared() > 0.001f) {
		target_distance = follow_offset.length();
	}

	// Seat every spring on the current transform before the first mode runs.
	rebase_springs();

	// Capture the scene-authored FOV as the resting FOV for speed-FOV, unless a
	// base was set explicitly.
	if (car_fov_base <= 0.0f) {
		car_fov_base = (float)get_fov();
	}

	// Resolve the optional speed-lines overlay.
	_update_speed_lines();

	// Debug test keys (opt-in): --fxtest enables K = camera shake.
	if (OS::get_singleton()->get_cmdline_user_args().has("--fxtest")) {
		debug_keys = true;
	}

	// Set initial mode
	set_camera_mode(camera_mode);
}

void GameCamera::_exit_tree() {
	GameManager *gm = GameManager::get_singleton();
	if (gm && gm->get_camera() == this) {
		gm->register_camera(nullptr);
	}
}

void GameCamera::_physics_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint() || !current_mode_instance)
		return;

	float delta = static_cast<float>(p_delta);

	if (follow_target_node == nullptr) {
		_update_follow_node();
	}

	// Delegate to current mode
	current_mode_instance->update(this, delta);

	// Additive shake on top of the solved transform (all modes).
	apply_shake(delta);
}

void GameCamera::_unhandled_key_input(const Ref<InputEvent> &p_event) {
	if (!debug_keys) {
		return;
	}
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
		if (key->get_keycode() == KEY_K) {
			add_trauma(0.6f); // debug: kick the camera
		}
	}
}

// Swap the active behaviour: exit + destroy the old state, construct + enter
// the new one. No-op if already in `p_mode`. Editor guarded.
void GameCamera::set_camera_mode(Mode p_mode) {
	if (current_mode_instance && camera_mode == p_mode) {
		return;
	}

	bool is_editor = Engine::get_singleton()->is_editor_hint();

	// Exit current mode
	if (current_mode_instance) {
		if (!is_editor) {
			current_mode_instance->exit(this);
		}
		current_mode_instance.reset();
	}

	camera_mode = p_mode;

	// Enter new mode
	switch (camera_mode) {
		case MODE_FLY:
			current_mode_instance = std::make_unique<CameraStateFly>();
			break;
		case MODE_CAR:
			current_mode_instance = std::make_unique<CameraStateCar>();
			break;
		case MODE_TPS:
			current_mode_instance = std::make_unique<CameraStateTPS>();
			break;
		case MODE_FIXED:
			current_mode_instance = std::make_unique<CameraStateFixed>();
			break;
	}

	if (current_mode_instance && !is_editor) {
		current_mode_instance->enter(this);
	}
}

GameCamera::Mode GameCamera::get_camera_mode() const { return camera_mode; }

void GameCamera::_update_follow_node() {
	if (follow_target_path.is_empty()) {
		return;
	}
	follow_target_node = Object::cast_to<Node3D>(get_node_or_null(follow_target_path));
	_refresh_follow_exclude();
}

// Cache the ray-exclude list so the per-frame collision cast (car / TPS) does
// not rebuild a TypedArray every tick. The target's RID is stable, so this only
// needs to run when the follow target changes.
void GameCamera::_refresh_follow_exclude() {
	follow_exclude.clear();
	if (follow_target_node) {
		CollisionObject3D *co = Object::cast_to<CollisionObject3D>(follow_target_node);
		if (co) {
			follow_exclude.push_back(co->get_rid());
		}
	}
}

void GameCamera::_update_speed_lines() {
	if (speed_lines_path.is_empty()) {
		speed_lines = nullptr;
		return;
	}
	speed_lines = Object::cast_to<SpeedLines>(get_node_or_null(speed_lines_path));
}

void GameCamera::set_speed_lines_path(const NodePath &p_path) {
	speed_lines_path = p_path;
	if (is_inside_tree()) {
		_update_speed_lines();
	}
}

// Forward a 0..1 speed ratio to the linked overlay (car cam calls this; other
// modes push 0 so the streaks fade out).
void GameCamera::drive_speed_lines(float p_ratio) {
	if (!speed_lines && !speed_lines_path.is_empty()) {
		_update_speed_lines();
	}
	if (speed_lines) {
		speed_lines->set_speed_ratio(p_ratio);
	}
}

// --- Shared follow helpers -------------------------------------------------

void GameCamera::rebase_springs() {
	Vector3 rot = get_rotation();
	yaw = rot.y;
	pitch = rot.x;

	pos_spring.reset(get_global_position());
	yaw_spring.reset(yaw);
	pitch_spring.reset(pitch);
	dist_spring.reset(target_distance);
}

Vector3 GameCamera::base_offset_dir() const {
	return follow_offset.length_squared() > 0.001f ? follow_offset.normalized() : Vector3(0, 0, 1);
}

void GameCamera::smooth_look_angles(float p_delta) {
	yaw = UtilityFunctions::wrapf(yaw, -Math::PI, Math::PI);

	// Follow the shortest arc so a +PI/-PI wrap never spins the camera.
	float yaw_diff = UtilityFunctions::wrapf(yaw - yaw_spring.current, -Math::PI, Math::PI);
	yaw_spring.target = yaw_spring.current + yaw_diff;
	pitch_spring.target = pitch;

	float rot_freq = frequency * rotation_freq_mult;
	yaw_spring.step(p_delta, rot_freq, damping, response);
	pitch_spring.step(p_delta, rot_freq, damping, response);

	yaw_spring.current = UtilityFunctions::wrapf(yaw_spring.current, -Math::PI, Math::PI);
}

Vector3 GameCamera::orbit_position(
		const Vector3 &p_pivot,
		float p_dist
) const {
	Basis rot = Basis::from_euler(Vector3(pitch_spring.current, yaw_spring.current, 0));
	return p_pivot + rot.xform(base_offset_dir() * p_dist);
}

float GameCamera::resolve_follow_distance(
		const Vector3 &p_pivot,
		const Vector3 &p_ideal_full,
		float p_desired,
		float p_delta
) {
	float actual = p_desired;
	if (collision_enabled && follow_target_node) {
		actual = _solve_collision(p_pivot, p_ideal_full);
	}

	dist_spring.target = actual;
	if (actual < dist_spring.current) {
		// Snap inward instantly so the camera never clips through the wall...
		dist_spring.current = actual;
		dist_spring.velocity = 0.0f;
	} else {
		// ...but ease back out once the obstruction clears.
		dist_spring.step(p_delta, frequency * distance_freq_mult, damping, response);
	}
	return dist_spring.current;
}

void GameCamera::apply_position(
		const Vector3 &p_ideal_pos,
		float p_delta
) {
	pos_spring.target = p_ideal_pos;
	if (pos_smoothing_enabled) {
		pos_spring.step(p_delta, frequency, damping, response);
		set_global_position(pos_spring.current);
	} else {
		set_global_position(p_ideal_pos);
	}
	set_rotation(Vector3(pitch_spring.current, yaw_spring.current, 0));
}

// Add a trauma impulse; the shake fades on its own.
void GameCamera::add_trauma(float p_amount) {
	shake.add_trauma(p_amount);
}

// Ease the FOV toward base + add*speed_frac (car mode). Disabled when add ~ 0.
void GameCamera::apply_speed_fov(
		float p_speed_frac,
		float p_delta
) {
	if (car_fov_speed_add <= 0.001f) {
		return;
	}
	float base = (car_fov_base > 0.0f) ? car_fov_base : (float)get_fov();
	float target = base + car_fov_speed_add * CLAMP(p_speed_frac, 0.0f, 1.0f);
	float current = Math::lerp((float)get_fov(), target, spring_damp_factor(fov_smooth_rate, p_delta));
	set_fov(current);
}

// Sample the trauma shake and add it on top of the transform the state solved.
// Because the state rewrites position + rotation from scratch every frame, the
// offset added here is naturally cleared next frame and never feeds the springs.
void GameCamera::apply_shake(float p_delta) {
	shake.update(p_delta);
	if (!shake.is_active()) {
		return;
	}
	Transform3D t = get_global_transform();
	Vector3 local_off = shake.local_position_offset();
	set_global_position(get_global_position() + t.basis.xform(local_off));
	set_rotation(get_rotation() + shake.rotation_offset());
}

float GameCamera::_solve_collision(
		const Vector3 &p_from,
		const Vector3 &p_to
) {
	float baseline_dist = p_from.distance_to(p_to);
	MCRaycastHit hit = raycast_3d(this, p_from, p_to, collision_mask, follow_exclude);
	if (hit.is_hit) {
		float hit_dist = p_from.distance_to(hit.position);
		return MAX(min_distance, hit_dist - collision_margin);
	}
	return baseline_dist;
}

void GameCamera::set_follow_target_path(const NodePath &p_path) {
	follow_target_path = p_path;
	_update_follow_node();
}

// Side effects by design: the offset's length becomes the resting distance,
// and max_distance grows to keep that distance reachable.
void GameCamera::set_follow_offset(const Vector3 &p_offset) {
	follow_offset = p_offset;
	target_distance = follow_offset.length();
	if (target_distance > max_distance) {
		max_distance = target_distance * 1.5f;
	}
}

NodePath GameCamera::get_follow_target_path() const { return follow_target_path; }

void GameCamera::set_follow_target_node(Node3D *p_node) {
	follow_target_node = p_node;
	if (follow_target_node) {
		follow_target_path = follow_target_node->get_path();
	} else {
		follow_target_path = NodePath();
	}
	_refresh_follow_exclude();
}

MCRaycastHit GameCamera::get_center_raycast_hit(
		uint32_t p_mask,
		float p_dist
) {
	Vector3 from = get_global_position();
	Vector3 to = from - get_global_transform().basis.get_column(2).normalized() * p_dist;

	return raycast_3d(this, from, to, p_mask, follow_exclude);
}

// Base distance plus car dynamic zoom: pull back from speed_threshold up to
// dynamic_zoom_extra_distance at max_speed_for_zoom (car mode + RigidBody only).
float GameCamera::get_current_target_distance() const {
	if (!dynamic_zoom_enabled || camera_mode != MODE_CAR || !follow_target_node) {
		return target_distance;
	}
	RigidBody3D *rb = Object::cast_to<RigidBody3D>(follow_target_node);
	if (!rb) {
		return target_distance;
	}
	float speed = rb->get_linear_velocity().length();
	if (speed > speed_threshold) {
		float factor = CLAMP((speed - speed_threshold) / (max_speed_for_zoom - speed_threshold), 0.0f, 1.0f);
		return target_distance + factor * dynamic_zoom_extra_distance;
	}
	return target_distance;
}

} // namespace godot
