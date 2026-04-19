#include "camera.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"

#include "states/car_state.h"
#include "states/fly_state.h"
#include "states/tps_state.h"
#include "states/fixed_state.h"

#include "../utils/raycast/mc_raycast.h"
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

namespace godot {

void GameCamera::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_camera_mode", "mode"), &GameCamera::set_camera_mode);
	ClassDB::bind_method(D_METHOD("get_camera_mode"), &GameCamera::get_camera_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "camera_mode", PROPERTY_HINT_ENUM, "Fly,Car,TPS,Fixed"), "set_camera_mode", "get_camera_mode");

	ClassDB::bind_method(D_METHOD("set_follow_target_path", "path"), &GameCamera::set_follow_target_path);
	ClassDB::bind_method(D_METHOD("get_follow_target_path"), &GameCamera::get_follow_target_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "follow_target_path"), "set_follow_target_path", "get_follow_target_path");

	ClassDB::bind_method(D_METHOD("set_frequency", "frequency"), &GameCamera::set_frequency);
	ClassDB::bind_method(D_METHOD("get_frequency"), &GameCamera::get_frequency);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frequency"), "set_frequency", "get_frequency");

	ClassDB::bind_method(D_METHOD("set_damping", "damping"), &GameCamera::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &GameCamera::get_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping"), "set_damping", "get_damping");

	ClassDB::bind_method(D_METHOD("set_pan_speed", "speed"), &GameCamera::set_pan_speed);
	ClassDB::bind_method(D_METHOD("get_pan_speed"), &GameCamera::get_pan_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pan_speed"), "set_pan_speed", "get_pan_speed");

	ClassDB::bind_method(D_METHOD("set_zoom_speed", "speed"), &GameCamera::set_zoom_speed);
	ClassDB::bind_method(D_METHOD("get_zoom_speed"), &GameCamera::get_zoom_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "zoom_speed"), "set_zoom_speed", "get_zoom_speed");

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

	ClassDB::bind_method(D_METHOD("set_stability_lock_enabled", "enabled"), &GameCamera::set_stability_lock_enabled);
	ClassDB::bind_method(D_METHOD("is_stability_lock_enabled"), &GameCamera::is_stability_lock_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stability_lock_enabled"), "set_stability_lock_enabled", "is_stability_lock_enabled");

	ClassDB::bind_method(D_METHOD("set_stability_threshold", "threshold"), &GameCamera::set_stability_threshold);
	ClassDB::bind_method(D_METHOD("get_stability_threshold"), &GameCamera::get_stability_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stability_threshold"), "set_stability_threshold", "get_stability_threshold");

	BIND_ENUM_CONSTANT(MODE_FLY);
	BIND_ENUM_CONSTANT(MODE_CAR);
	BIND_ENUM_CONSTANT(MODE_TPS);
	BIND_ENUM_CONSTANT(MODE_FIXED);
}

GameCamera::GameCamera() {
	frequency = 3.0f;
	damping = 1.0f;
	response = 0.0f;
}

GameCamera::~GameCamera() {
	if (current_mode_instance) {
		delete current_mode_instance;
	}
}

void GameCamera::_ready() {
	if (Engine::get_singleton()->is_editor_hint())
		return;
	_update_follow_node();

	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_camera(this);
	}

	// Initialize springs
	pos_spring.reset(get_global_position());
	yaw_spring.reset(get_rotation().y);
	pitch_spring.reset(get_rotation().x);

	if (follow_offset.length_squared() > 0.001f) {
		target_distance = follow_offset.length();
	}
	dist_spring.reset(target_distance);

	// Set initial mode
	set_camera_mode(camera_mode);
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
}

void GameCamera::set_camera_mode(Mode p_mode) {
	if (current_mode_instance && camera_mode == p_mode) {
		return;
	}

	// Exit current mode
	if (current_mode_instance) {
		current_mode_instance->exit(this);
		delete current_mode_instance;
		current_mode_instance = nullptr;
	}

	camera_mode = p_mode;

	// Enter new mode
	switch (camera_mode) {
		case MODE_FLY:
			current_mode_instance = new CameraStateFly();
			break;
		case MODE_CAR:
			current_mode_instance = new CameraStateCar();
			break;
		case MODE_TPS:
			current_mode_instance = new CameraStateTPS();
			break;
		case MODE_FIXED:
			current_mode_instance = new CameraStateFixed();
			break;
	}

	if (current_mode_instance) {
		current_mode_instance->enter(this);
	}
}

GameCamera::Mode GameCamera::get_camera_mode() const { return camera_mode; }

void GameCamera::_update_follow_node() {
	if (follow_target_path.is_empty()) {
		return;
	}
	follow_target_node = Object::cast_to<Node3D>(get_node_or_null(follow_target_path));
}

Vector3 GameCamera::_calculate_ideal_position() {
	if (camera_mode == MODE_FLY) {
		return pos_spring.target;
	}

	if (!follow_target_node) {
		return get_global_position();
	}

	Vector3 target_origin = follow_target_node->get_global_position();
	Basis rot_basis = Basis::from_euler(Vector3(pitch, yaw, 0));
	Vector3 base_dir = follow_offset.length_squared() > 0.001f ? follow_offset.normalized() : Vector3(0, 0, 1);
	return target_origin + rot_basis.xform(base_dir * target_distance);
}

float GameCamera::_solve_collision(const Vector3 &p_from, const Vector3 &p_to) {
	TypedArray<RID> exclude;
	if (follow_target_node) {
		CollisionObject3D *co = Object::cast_to<CollisionObject3D>(follow_target_node);
		if (co) {
			exclude.push_back(co->get_rid());
		}
	}

	float baseline_dist = p_from.distance_to(p_to);
	MCRaycastHit hit = raycast_3d(this, p_from, p_to, collision_mask, exclude);
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
}

MCRaycastHit GameCamera::get_center_raycast_hit(uint32_t p_mask, float p_dist) {
	Vector3 from = get_global_position();
	Vector3 to = from - get_global_transform().basis.get_column(2).normalized() * p_dist;

	TypedArray<RID> exclude;
	if (follow_target_node) {
		CollisionObject3D *co = Object::cast_to<CollisionObject3D>(follow_target_node);
		if (co)
			exclude.push_back(co->get_rid());
	}

	return raycast_3d(this, from, to, p_mask, exclude);
}

} // namespace godot
