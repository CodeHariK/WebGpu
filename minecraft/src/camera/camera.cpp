#include "camera.h"

#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
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
	if (Engine::get_singleton()->is_editor_hint()) return;
	_update_follow_node();

	// Initialize springs to current state
	pos_spring.reset(get_global_position());
	yaw_spring.reset(get_rotation().y);
	pitch_spring.reset(get_rotation().x);
	dist_spring.reset(target_distance);
}

void MCCamera::_physics_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint()) return;

	float delta = static_cast<float>(p_delta);

	// 1. Identify Target Node
	if (follow_target_node == nullptr) {
		_update_follow_node();
	}

	// 2. Handle Rotation (Inputs modify yaw/pitch targets)
	yaw_spring.target = yaw;
	pitch_spring.target = pitch;

	yaw_spring.step(delta, frequency * 2.0f, damping, response);
	pitch_spring.step(delta, frequency * 2.0f, damping, response);

	// 3. Handle Position & Collision
	Vector3 ideal_pos = _calculate_ideal_position();
	Vector3 pivot = (follow_target_node) ? follow_target_node->get_global_position() : get_global_position();

	float actual_dist = target_distance;
	if (collision_enabled && follow_target_node) {
		actual_dist = _solve_collision(pivot, ideal_pos);
	}

	dist_spring.target = actual_dist;
	dist_spring.step(delta, frequency * 1.5f, damping, response);

	// Final Position Solver
	if (mode == MODE_FLY) {
		pos_spring.target = ideal_pos;
		pos_spring.step(delta, frequency, damping, response);
		set_global_position(pos_spring.current);
	} else {
		// In Follow modes, pivot around target
		Basis rot_basis = Basis::from_euler(Vector3(pitch_spring.current, yaw_spring.current, 0));
		Vector3 offset = rot_basis.xform(Vector3(0, 0, dist_spring.current));
		set_global_position(pivot + offset);
	}

	// 4. Update Rotation
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
	if (follow_target_path.is_empty()) return;
	follow_target_node = Object::cast_to<Node3D>(get_node_or_null(follow_target_path));
}

Vector3 MCCamera::_calculate_ideal_position() {
	if (mode == MODE_FLY) {
		return pos_spring.target;
	}

	if (!follow_target_node) return get_global_position();

	Vector3 target_origin = follow_target_node->get_global_position();
	Basis rot_basis = Basis::from_euler(Vector3(pitch, yaw, 0));
	
	// Default chase position (behind target)
	return target_origin + rot_basis.xform(Vector3(0, 0, target_distance));
}

float MCCamera::_solve_collision(const Vector3 &p_from, const Vector3 &p_to) {
	Ref<World3D> world = get_world_3d();
	if (world.is_null()) return target_distance;

	PhysicsDirectSpaceState3D *space = world->get_direct_space_state();
	if (!space) return target_distance;

	Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(p_from, p_to, collision_mask);
	
	// Exclude the target itself if it has collision
	TypedArray<RID> exclude;
	if (follow_target_node) {
		// Check for collision object
		CollisionObject3D *co = Object::cast_to<CollisionObject3D>(follow_target_node);
		if (co) exclude.push_back(co->get_rid());
	}
	query->set_exclude(exclude);

	Dictionary result = space->intersect_ray(query);
	if (!result.is_empty()) {
		Vector3 hit_pos = result["position"];
		float hit_dist = p_from.distance_to(hit_pos);
		return MAX(min_distance, hit_dist - collision_margin);
	}

	return target_distance;
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

NodePath MCCamera::get_follow_target_path() const { return follow_target_path; }

} // namespace godot
