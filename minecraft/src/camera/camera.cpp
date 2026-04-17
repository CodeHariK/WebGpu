#include "camera.h"
#include "../game_manager/game_manager.h"

#include "../utils/raycast/mc_raycast.h"
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

namespace godot {

void MCCamera::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &MCCamera::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &MCCamera::get_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Fly,Car,Character,Orbit"), "set_mode", "get_mode");

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
	BIND_ENUM_CONSTANT(MODE_CHARACTER);
	BIND_ENUM_CONSTANT(MODE_ORBIT);
}

MCCamera::MCCamera() {
	// Defaults
	frequency = 3.0f;
	damping = 1.0f;
	response = 0.0f;
}

MCCamera::~MCCamera() {}

void MCCamera::_ready() {
	if (Engine::get_singleton()->is_editor_hint())
		return;
	_update_follow_node();

	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_camera(this);
	}

	// Initialize springs to current state
	pos_spring.reset(get_global_position());
	yaw_spring.reset(get_rotation().y);
	pitch_spring.reset(get_rotation().x);

	if (follow_offset.length_squared() > 0.001f) {
		target_distance = follow_offset.length();
	}
	dist_spring.reset(target_distance);
}

void MCCamera::_physics_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint())
		return;
	float delta = static_cast<float>(p_delta);

	if (follow_target_node == nullptr) {
		_update_follow_node();
	}

	switch (mode) {
		case MODE_FLY:
			_process_fly_mode(delta);
			break;
		case MODE_CAR:
			_process_car_mode(delta);
			_process_follow_modes(delta);
			break;
		case MODE_CHARACTER:
		case MODE_ORBIT:
			_process_follow_modes(delta);
			break;
	}
}

void MCCamera::_process_fly_mode(float p_delta) {
	// Free look update
	yaw_spring.target = yaw;
	pitch_spring.target = pitch;

	yaw_spring.step(p_delta, frequency * 2.0f, damping, response);
	pitch_spring.step(p_delta, frequency * 2.0f, damping, response);

	// Position update
	Vector3 ideal_pos = _calculate_ideal_position();
	pos_spring.target = ideal_pos;
	pos_spring.step(p_delta, frequency, damping, response);

	set_global_position(pos_spring.current);
	set_rotation(Vector3(pitch_spring.current, yaw_spring.current, 0));
}

void MCCamera::_process_car_mode(float p_delta) {
	bool is_mmb = Input::get_singleton()->is_mouse_button_pressed(MOUSE_BUTTON_MIDDLE);
	if (is_mmb)
		return;

	// 1. Calculate the ideal FOLLOW state (where we WANT to be)
	float target_yaw = 0.0f;

	RigidBody3D *rb = Object::cast_to<RigidBody3D>(follow_target_node);
	if (rb) {
		Vector3 linear_vel = rb->get_linear_velocity();
		Vector3 horizontal_vel = Vector3(linear_vel.x, 0, linear_vel.z);

		Vector3 target_forward = -follow_target_node->get_global_transform().basis.get_column(2).normalized();
		float forward_dot_vel = (horizontal_vel.length_squared() > 0.01f) ? horizontal_vel.normalized().dot(target_forward) : 1.0f;

		if (horizontal_vel.length_squared() > 1.0f && forward_dot_vel > 0.0f) {
			target_yaw = Math::atan2(-horizontal_vel.x, -horizontal_vel.z);
		} else {
			target_yaw = follow_target_node->get_global_rotation().y;
		}
	} else {
		target_yaw = (follow_target_node) ? follow_target_node->get_global_rotation().y : yaw;
	}

	// Ideal Pitch to look at target center
	float h_dist = Vector2(follow_offset.x, follow_offset.z).length();
	float target_pitch = (h_dist > 0.01f) ? -Math::atan2(follow_offset.y, h_dist) : pitch;

	// 2. Calculate the dynamic Stability benchmark
	Basis ideal_basis = Basis::from_euler(Vector3(target_pitch, target_yaw, 0));
	Vector3 base_dir = follow_offset.length_squared() > 0.001f ? follow_offset.normalized() : Vector3(0, 0, 1);
	Vector3 ideal_rel_pos = ideal_basis.xform(base_dir * target_distance);
	float ideal_y_diff = ideal_rel_pos.y;

	// 3. Stability Check
	bool lock_orientation = false;
	if (stability_lock_enabled && follow_target_node) {
		float current_y_diff = get_global_position().y - follow_target_node->get_global_position().y;
		if (std::abs(current_y_diff - ideal_y_diff) > stability_threshold) {
			lock_orientation = true;
		}
	}

	if (!lock_orientation) {
		// Snap: Update yaw/pitch targets
		float diff = target_yaw - yaw;
		while (diff > Math_PI)
			diff -= Math_TAU;
		while (diff < -Math_PI)
			diff += Math_TAU;
		yaw += diff;
		pitch = target_pitch;
	}
}

void MCCamera::_process_follow_modes(float p_delta) {
	// 1. Handle Orientation
	yaw_spring.target = yaw;
	pitch_spring.target = pitch;

	yaw_spring.step(p_delta, frequency * 2.0f, damping, response);
	pitch_spring.step(p_delta, frequency * 2.0f, damping, response);

	// 2. Handle Position & Collision
	Vector3 ideal_pos = _calculate_ideal_position();
	Vector3 pivot = (follow_target_node) ? follow_target_node->get_global_position() : get_global_position();

	float actual_dist = target_distance;
	if (collision_enabled && follow_target_node) {
		actual_dist = _solve_collision(pivot, ideal_pos);
	}

	dist_spring.target = actual_dist;
	dist_spring.step(p_delta, frequency * 1.5f, damping, response);

	// Final Follow Placement
	Basis rot_basis = Basis::from_euler(Vector3(pitch_spring.current, yaw_spring.current, 0));
	Vector3 base_dir = follow_offset.length_squared() > 0.001f ? follow_offset.normalized() : Vector3(0, 0, 1);
	Vector3 offset = rot_basis.xform(base_dir * dist_spring.current);

	set_global_position(pivot + offset);
	set_rotation(Vector3(pitch_spring.current, yaw_spring.current, 0));
}

void MCCamera::_unhandled_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		bool is_mmb = Input::get_singleton()->is_mouse_button_pressed(MOUSE_BUTTON_MIDDLE);

		if (mode == MODE_FLY) {
			if (is_mmb) {
				if (Input::get_singleton()->is_key_pressed(KEY_SHIFT)) {
					// Panning
					Transform3D t = get_global_transform();
					Vector3 right = t.basis.get_column(0);
					Vector3 up = t.basis.get_column(1);
					pos_spring.target += right * (-mm->get_relative().x * pan_speed) + up * (mm->get_relative().y * pan_speed);
				} else {
					// Orbiting (Free look in Fly mode)
					yaw -= mm->get_relative().x * orbit_sensitivity;
					pitch -= mm->get_relative().y * orbit_sensitivity;
					pitch = CLAMP(pitch, -Math_PI * 0.49f, Math_PI * 0.49f);
				}
			}
		} else if (mode == MODE_ORBIT || is_mmb) {
			yaw -= mm->get_relative().x * orbit_sensitivity;
			pitch -= mm->get_relative().y * orbit_sensitivity;
			pitch = CLAMP(pitch, -Math_PI * 0.49f, Math_PI * 0.49f);
		}
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_UP) {
			if (mode == MODE_FLY) {
				Transform3D t = get_global_transform();
				Vector3 forward = -t.basis.get_column(2);
				pos_spring.target += forward * zoom_speed;
			} else {
				target_distance = CLAMP(target_distance - zoom_speed, min_distance, max_distance);
			}
		} else if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN) {
			if (mode == MODE_FLY) {
				Transform3D t = get_global_transform();
				Vector3 forward = -t.basis.get_column(2);
				pos_spring.target -= forward * zoom_speed;
			} else {
				target_distance = CLAMP(target_distance + zoom_speed, min_distance, max_distance);
			}
		}
	}
}

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

	// Default base vector (Back direction)
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

void MCCamera::set_mode(CameraMode p_mode) {
	mode = p_mode;
	if (mode == MODE_FLY) {
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
	} else {
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	}
}

MCCamera::CameraMode MCCamera::get_mode() const { return mode; }

void MCCamera::set_follow_target_path(const NodePath &p_path) {
	follow_target_path = p_path;
	_update_follow_node();
}

void MCCamera::set_follow_offset(const Vector3 &p_offset) {
	follow_offset = p_offset;
	target_distance = follow_offset.length();
	// Automatically expand max_distance if offset is larger
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

} // namespace godot
