#include "camera.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"

#include "modes/fly_mode.h"
#include "modes/car_mode.h"
#include "modes/tps_mode.h"

#include "../utils/raycast/mc_raycast.h"
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

namespace godot {

void MCCamera::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &MCCamera::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &MCCamera::get_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Fly,Car,TPS"), "set_mode", "get_mode");

	ClassDB::bind_method(D_METHOD("set_follow_target_path", "path"), &MCCamera::set_follow_target_path);
	ClassDB::bind_method(D_METHOD("get_follow_target_path"), &MCCamera::get_follow_target_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "follow_target_path"), "set_follow_target_path", "get_follow_target_path");

	ClassDB::bind_method(D_METHOD("set_frequency", "frequency"), &MCCamera::set_frequency);
	ClassDB::bind_method(D_METHOD("get_frequency"), &MCCamera::get_frequency);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frequency"), "set_frequency", "get_frequency");

	ClassDB::bind_method(D_METHOD("set_damping", "damping"), &MCCamera::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &MCCamera::get_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping"), "set_damping", "get_damping");

	ClassDB::bind_method(D_METHOD("set_pan_speed", "speed"), &MCCamera::set_pan_speed);
	ClassDB::bind_method(D_METHOD("get_pan_speed"), &MCCamera::get_pan_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pan_speed"), "set_pan_speed", "get_pan_speed");

	ClassDB::bind_method(D_METHOD("set_zoom_speed", "speed"), &MCCamera::set_zoom_speed);
	ClassDB::bind_method(D_METHOD("get_zoom_speed"), &MCCamera::get_zoom_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "zoom_speed"), "set_zoom_speed", "get_zoom_speed");

	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &MCCamera::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("is_collision_enabled"), &MCCamera::is_collision_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "is_collision_enabled");

	ClassDB::bind_method(D_METHOD("set_follow_offset", "offset"), &MCCamera::set_follow_offset);
	ClassDB::bind_method(D_METHOD("get_follow_offset"), &MCCamera::get_follow_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "follow_offset"), "set_follow_offset", "get_follow_offset");

	ClassDB::bind_method(D_METHOD("set_min_distance", "distance"), &MCCamera::set_min_distance);
	ClassDB::bind_method(D_METHOD("get_min_distance"), &MCCamera::get_min_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_distance"), "set_min_distance", "get_min_distance");

	ClassDB::bind_method(D_METHOD("set_max_distance", "distance"), &MCCamera::set_max_distance);
	ClassDB::bind_method(D_METHOD("get_max_distance"), &MCCamera::get_max_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_distance"), "set_max_distance", "get_max_distance");

	ClassDB::bind_method(D_METHOD("set_stability_lock_enabled", "enabled"), &MCCamera::set_stability_lock_enabled);
	ClassDB::bind_method(D_METHOD("is_stability_lock_enabled"), &MCCamera::is_stability_lock_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stability_lock_enabled"), "set_stability_lock_enabled", "is_stability_lock_enabled");

	ClassDB::bind_method(D_METHOD("set_stability_threshold", "threshold"), &MCCamera::set_stability_threshold);
	ClassDB::bind_method(D_METHOD("get_stability_threshold"), &MCCamera::get_stability_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stability_threshold"), "set_stability_threshold", "get_stability_threshold");

	BIND_ENUM_CONSTANT(MODE_FLY);
	BIND_ENUM_CONSTANT(MODE_CAR);
	BIND_ENUM_CONSTANT(MODE_TPS);
}

MCCamera::MCCamera() {
	frequency = 3.0f;
	damping = 1.0f;
	response = 0.0f;
}

MCCamera::~MCCamera() {
	if (current_mode_instance) {
		delete current_mode_instance;
	}
}

void MCCamera::_ready() {
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
	set_mode(mode);
}

void MCCamera::_physics_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint() || !current_mode_instance)
		return;

	float delta = static_cast<float>(p_delta);

	if (follow_target_node == nullptr) {
		_update_follow_node();
	}

	// Delegate to current mode
	current_mode_instance->update(this, delta);
}

void MCCamera::set_mode(CameraState p_mode) {
	if (current_mode_instance && mode == p_mode) {
		return;
	}

	// Exit current mode
	if (current_mode_instance) {
		current_mode_instance->exit(this);
		delete current_mode_instance;
		current_mode_instance = nullptr;
	}

	mode = p_mode;

	// Enter new mode
	switch (mode) {
		case MODE_FLY:
			current_mode_instance = new MCFlyCameraMode();
			break;
		case MODE_CAR:
			current_mode_instance = new MCCarCameraMode();
			break;
		case MODE_TPS:
			current_mode_instance = new MCTPSCameraMode();
			break;
	}

	if (current_mode_instance) {
		current_mode_instance->enter(this);
	}
}

MCCamera::CameraState MCCamera::get_mode() const { return mode; }

void MCCamera::_update_follow_node() {
	if (follow_target_path.is_empty()) {
		return;
	}
	follow_target_node = Object::cast_to<Node3D>(get_node_or_null(follow_target_path));
}

Vector3 MCCamera::_calculate_ideal_position() {
	if (mode == MODE_FLY) {
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

float MCCamera::_solve_collision(const Vector3 &p_from, const Vector3 &p_to) {
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

void MCCamera::set_follow_target_path(const NodePath &p_path) {
	follow_target_path = p_path;
	_update_follow_node();
}

void MCCamera::set_follow_offset(const Vector3 &p_offset) {
	follow_offset = p_offset;
	target_distance = follow_offset.length();
	if (target_distance > max_distance) {
		max_distance = target_distance * 1.5f;
	}
}

NodePath MCCamera::get_follow_target_path() const { return follow_target_path; }

void MCCamera::set_follow_target_node(Node3D *p_node) {
	follow_target_node = p_node;
	if (follow_target_node) {
		follow_target_path = follow_target_node->get_path();
	} else {
		follow_target_path = NodePath();
	}
}

MCRaycastHit MCCamera::get_center_raycast_hit(uint32_t p_mask, float p_dist) {
	Vector3 from = get_global_position();
	Vector3 to = from - get_global_transform().basis.get_column(2).normalized() * p_dist;
	
	TypedArray<RID> exclude;
	if (follow_target_node) {
		CollisionObject3D *co = Object::cast_to<CollisionObject3D>(follow_target_node);
		if (co) exclude.push_back(co->get_rid());
	}
	
	return raycast_3d(this, from, to, p_mask, exclude);
}

} // namespace godot
