/**
 * @file track_model.cpp
 * @brief TrackModel: placing the track on its start line and giving its meshes colliders.
 */
#include "track_model.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/world_boundary_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <vector>

namespace godot {

namespace {

constexpr float FLOOR = 3.0f; // metres below the road: the safety floor

void gather(
		Node *p_node,
		std::vector<MeshInstance3D *> *r_meshes
) {
	if (MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(p_node)) {
		r_meshes->push_back(mesh);
	}
	for (int64_t i = 0; i < p_node->get_child_count(); i++) {
		gather(p_node->get_child(i), r_meshes);
	}
}

// p_node's transform in p_top's space.
Transform3D relative_to(
		const Node *p_top,
		Node *p_node
) {
	Transform3D t;
	for (Node *walk = p_node; walk && walk != p_top; walk = walk->get_parent()) {
		if (Node3D *n = Object::cast_to<Node3D>(walk)) {
			t = n->get_transform() * t;
		}
	}
	return t;
}

} // namespace

void TrackModel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_model", "model"), &TrackModel::set_model);
	ClassDB::bind_method(D_METHOD("get_model"), &TrackModel::get_model);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "model", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_model", "get_model");
	ClassDB::bind_method(D_METHOD("set_model_scale", "scale"), &TrackModel::set_model_scale);
	ClassDB::bind_method(D_METHOD("get_model_scale"), &TrackModel::get_model_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "model_scale"), "set_model_scale", "get_model_scale");
	ClassDB::bind_method(D_METHOD("set_start_part", "part"), &TrackModel::set_start_part);
	ClassDB::bind_method(D_METHOD("get_start_part"), &TrackModel::get_start_part);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "start_part"), "set_start_part", "get_start_part");
}

void TrackModel::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build();
	}
}

void TrackModel::_build() {
	if (model.is_null()) {
		return;
	}
	Node3D *track = Object::cast_to<Node3D>(model->instantiate());
	if (!track) {
		return;
	}
	add_child(track);
	track->set_scale(Vector3(model_scale, model_scale, model_scale));
	std::vector<MeshInstance3D *> meshes;
	gather(track, &meshes);
	const String needle = start_part.to_lower();
	for (MeshInstance3D *mesh : meshes) { // the start line to the origin
		if (mesh->get_mesh().is_valid() && String(mesh->get_name()).to_lower().contains(needle)) {
			const Vector3 middle = relative_to(this, mesh).xform(mesh->get_mesh()->get_aabb()).get_center();
			track->set_position(-middle);
			break;
		}
	}
	if (Engine::get_singleton()->is_editor_hint()) {
		return; // colliders only when playing
	}
	for (MeshInstance3D *mesh : meshes) {
		if (mesh->get_mesh().is_valid()) {
			mesh->create_trimesh_collision();
		}
	}
	StaticBody3D *floor = memnew(StaticBody3D);
	floor->set_name("SafetyFloor");
	CollisionShape3D *shape = memnew(CollisionShape3D);
	Ref<WorldBoundaryShape3D> plane;
	plane.instantiate();
	shape->set_shape(plane);
	floor->add_child(shape);
	floor->set_position(Vector3(0, -FLOOR, 0));
	add_child(floor);
}

void TrackModel::set_model(const Ref<PackedScene> &p_model) {
	model = p_model;
}

Ref<PackedScene> TrackModel::get_model() const {
	return model;
}

void TrackModel::set_model_scale(float p_scale) {
	model_scale = p_scale;
}

float TrackModel::get_model_scale() const {
	return model_scale;
}

void TrackModel::set_start_part(const String &p_part) {
	start_part = p_part;
}

String TrackModel::get_start_part() const {
	return start_part;
}

} // namespace godot
