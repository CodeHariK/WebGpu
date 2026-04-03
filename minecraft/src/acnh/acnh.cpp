#include "acnh.h"

#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void ACNHNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_ground_mesh_path", "p_path"), &ACNHNode::set_ground_mesh_path);
	ClassDB::bind_method(D_METHOD("get_ground_mesh_path"), &ACNHNode::get_ground_mesh_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "ground_mesh_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D"), "set_ground_mesh_path", "get_ground_mesh_path");
}

ACNHNode::ACNHNode() {
	set_process_mode(Node::ProcessMode::PROCESS_MODE_INHERIT);
}

ACNHNode::~ACNHNode() {
	if (Engine::get_singleton()->is_editor_hint()) {
		set_process_mode(Node::ProcessMode::PROCESS_MODE_DISABLED);
	}
}

void ACNHNode::_ready() {
	// 1. Resolve ground mesh path
	if (!ground_mesh_path.is_empty()) {
		ground_mesh = Object::cast_to<Node3D>(get_node_or_null(ground_mesh_path));
	}

	// 2. Discover Camera under Camera_X_Pivot/SpringArm3D/Camera
	Node *pivot = get_parent()->get_node_or_null("Camera_X_Pivot");
	if (pivot) {
		camera = Object::cast_to<Camera3D>(pivot->get_node_or_null("SpringArm3D/Camera"));
	}

	if (camera) {
		UtilityFunctions::print("ACNHNode: Found camera: ", camera->get_name());
	} else {
		UtilityFunctions::print("ACNHNode: Warning - Could not find camera under Camera_X_Pivot.");
	}
}

void ACNHNode::_input(const Ref<InputEvent> &p_event) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::MOUSE_BUTTON_LEFT) {
		_perform_raycast(mb->get_position());
	}
}

void ACNHNode::_perform_raycast(const Vector2 &p_mouse_pos) {
	if (!camera) {
		return;
	}

	Vector3 ray_origin = camera->project_ray_origin(p_mouse_pos);
	Vector3 ray_normal = camera->project_ray_normal(p_mouse_pos);
	Vector3 ray_end = ray_origin + ray_normal * 2000.0f;

	Ref<World3D> world = get_world_3d();
	if (world.is_null()) {
		return;
	}

	PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
	if (!space_state) {
		return;
	}

	Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(ray_origin, ray_end);
	Dictionary result = space_state->intersect_ray(query);

	if (!result.is_empty()) {
		Vector3 hit_pos = result["position"];
		UtilityFunctions::print("Raycast Hit at: ", hit_pos);
		
		// If we want to confirm it hit the ground
		Object *collider = result["collider"];
		if (collider) {
			Node3D *hit_node = Object::cast_to<Node3D>(collider);
			if (hit_node) {
				UtilityFunctions::print("Hit Node: ", hit_node->get_name());
			}
		}
	} else {
		UtilityFunctions::print("Raycast: No hit.");
	}
}

void ACNHNode::set_ground_mesh_path(const NodePath &p_path) {
	ground_mesh_path = p_path;
}

NodePath ACNHNode::get_ground_mesh_path() const {
	return ground_mesh_path;
}
