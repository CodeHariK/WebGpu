/**
 * @file model_vehicle_visual.cpp
 * @brief ModelVehicleVisual: building the model with its wheels on pivots, then moving the wheels
 * each frame from the ArcadeVehicle's suspension, speed and steering.
 */
#include "model_vehicle_visual.h"
#include "vehicle/arcade_vehicle.h"
#include "vehicle/config/vehicle_config.h"
#include "vehicle/config/wheel_config.h"
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

namespace {

constexpr float CLEARANCE = 0.25f; // metres from the bottom of the tyres up to the body collider

struct Part {
	MeshInstance3D *mesh = nullptr;
	Transform3D transform; // in the visual's (vehicle) space
	AABB bounds; // likewise
};

// p_node's transform in p_top's space (p_node somewhere under p_top).
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

void gather(
		const Node *p_top,
		Node *p_node,
		std::vector<MeshInstance3D *> *r_meshes
) {
	if (MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(p_node)) {
		r_meshes->push_back(mesh);
	}
	for (int64_t i = 0; i < p_node->get_child_count(); i++) {
		gather(p_top, p_node->get_child(i), r_meshes);
	}
}

} // namespace

void ModelVehicleVisual::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_model", "model"), &ModelVehicleVisual::set_model);
	ClassDB::bind_method(D_METHOD("get_model"), &ModelVehicleVisual::get_model);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "model", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_model", "get_model");
	ClassDB::bind_method(D_METHOD("set_model_scale", "scale"), &ModelVehicleVisual::set_model_scale);
	ClassDB::bind_method(D_METHOD("get_model_scale"), &ModelVehicleVisual::get_model_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "model_scale"), "set_model_scale", "get_model_scale");
	ClassDB::bind_method(D_METHOD("set_model_yaw_degrees", "degrees"), &ModelVehicleVisual::set_model_yaw_degrees);
	ClassDB::bind_method(D_METHOD("get_model_yaw_degrees"), &ModelVehicleVisual::get_model_yaw_degrees);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "model_yaw_degrees"), "set_model_yaw_degrees", "get_model_yaw_degrees");
	ClassDB::bind_method(D_METHOD("set_wheel_parts", "parts"), &ModelVehicleVisual::set_wheel_parts);
	ClassDB::bind_method(D_METHOD("get_wheel_parts"), &ModelVehicleVisual::get_wheel_parts);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "wheel_parts"), "set_wheel_parts", "get_wheel_parts");
	ClassDB::bind_method(D_METHOD("set_body_collider", "enabled"), &ModelVehicleVisual::set_body_collider);
	ClassDB::bind_method(D_METHOD("get_body_collider"), &ModelVehicleVisual::get_body_collider);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "body_collider"), "set_body_collider", "get_body_collider");
	ClassDB::bind_method(D_METHOD("set_max_steer_degrees", "degrees"), &ModelVehicleVisual::set_max_steer_degrees);
	ClassDB::bind_method(D_METHOD("get_max_steer_degrees"), &ModelVehicleVisual::get_max_steer_degrees);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_steer_degrees"), "set_max_steer_degrees", "get_max_steer_degrees");
}

void ModelVehicleVisual::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) {
		set_process(true); // built on the first frame: the vehicle loads its preset after this is ready
	}
}

bool ModelVehicleVisual::_is_wheel_part(const String &p_name) const {
	const String name = p_name.to_lower();
	for (const String &part : wheel_parts) {
		if (name.contains(part.to_lower())) {
			return true;
		}
	}
	return false;
}

void ModelVehicleVisual::_build() {
	vehicle = Object::cast_to<ArcadeVehicle>(get_parent());
	if (model.is_null() || !vehicle || vehicle->get_config().is_null()) {
		return;
	}
	Node3D *body = Object::cast_to<Node3D>(model->instantiate());
	if (!body) {
		return;
	}
	add_child(body);
	body->set_transform(Transform3D(Basis(Vector3(0, 1, 0), Math::deg_to_rad(model_yaw_degrees)).scaled(Vector3(model_scale, model_scale, model_scale)), Vector3()));

	const TypedArray<WheelConfig> configs = vehicle->get_config()->get_wheel_configs();
	const float gravity = float(double(ProjectSettings::get_singleton()->get_setting("physics/3d/default_gravity", 9.8))) * vehicle->get_gravity_scale();
	const float weight = vehicle->get_config()->get_mass() * gravity + vehicle->get_config()->get_downforce();
	std::vector<Vector3> hardpoints;
	std::vector<float> rests;
	for (int64_t i = 0; i < configs.size(); i++) {
		const Ref<WheelConfig> wc = configs[i];
		if (wc.is_null()) {
			continue;
		}
		// where the wheel hangs with the car standing still: its rest length less the squash of
		// carrying its share of the car's weight (and the vehicle's downforce)
		const float squash = weight / (float(configs.size()) * MAX(wc->get_suspension_stiffness(), 1.0f));
		hardpoints.push_back(wc->get_hardpoint_offset());
		rests.push_back(MAX(0.0f, wc->get_suspension_rest_length() - squash));
		radius = wc->get_radius();
	}
	if (hardpoints.empty()) {
		return;
	}

	// the wheel meshes, each given to the nearest physics wheel (across the ground)
	std::vector<MeshInstance3D *> meshes;
	gather(this, body, &meshes);
	std::vector<std::vector<Part>> parts(hardpoints.size());
	for (MeshInstance3D *mesh : meshes) {
		if (!_is_wheel_part(mesh->get_name()) || mesh->get_mesh().is_null()) {
			continue;
		}
		Part part{ mesh, relative_to(this, mesh) };
		part.bounds = part.transform.xform(mesh->get_mesh()->get_aabb());
		const Vector3 middle = part.bounds.get_center();
		size_t nearest = 0;
		for (size_t w = 1; w < hardpoints.size(); w++) {
			const Vector2 a(middle.x - hardpoints[w].x, middle.z - hardpoints[w].z);
			const Vector2 b(middle.x - hardpoints[nearest].x, middle.z - hardpoints[nearest].z);
			nearest = a.length_squared() < b.length_squared() ? w : nearest;
		}
		parts[nearest].push_back(part);
	}

	// the body lifted so the wheels' middles hang where the suspension settles
	float lift = 0.0f;
	int counted = 0;
	std::vector<Vector3> middles(hardpoints.size());
	for (size_t w = 0; w < hardpoints.size(); w++) {
		if (parts[w].empty()) {
			continue;
		}
		AABB all = parts[w][0].bounds;
		for (const Part &p : parts[w]) {
			all = all.merge(p.bounds);
		}
		middles[w] = all.get_center();
		lift += hardpoints[w].y - rests[w] - middles[w].y;
		counted++;
	}
	lift = counted ? lift / float(counted) : 0.0f;
	body->set_position(Vector3(0, lift, 0));

	for (size_t w = 0; w < hardpoints.size(); w++) {
		if (parts[w].empty()) {
			continue;
		}
		Wheel wheel;
		wheel.middle = middles[w] + Vector3(0, lift, 0);
		wheel.physics = int(w);
		wheel.rest = rests[w];
		wheel.front = hardpoints[w].z < 0.0f; // forward is −z
		wheel.pivot = memnew(Node3D);
		wheel.pivot->set_name(String("Wheel") + String::num_int64(int64_t(w)));
		add_child(wheel.pivot);
		wheel.pivot->set_position(wheel.middle);
		for (const Part &p : parts[w]) { // onto the pivot, where it was
			p.mesh->get_parent()->remove_child(p.mesh);
			wheel.pivot->add_child(p.mesh);
			p.mesh->set_transform(Transform3D(Basis(), -wheel.middle) * Transform3D(Basis(), Vector3(0, lift, 0)) * p.transform);
		}
		wheels.push_back(wheel);
	}
	_add_body_collider(body, meshes);
}

// A box round the body (from CLEARANCE above the tyres' bottom to the roof) on the vehicle, so the
// whole car hits walls, not just the vehicle's chassis sphere.
void ModelVehicleVisual::_add_body_collider(
		Node3D *p_body,
		const std::vector<MeshInstance3D *> &p_meshes
) {
	if (!body_collider || p_meshes.empty() || wheels.empty()) {
		return;
	}
	bool first = true;
	AABB all;
	for (MeshInstance3D *mesh : p_meshes) {
		if (mesh->get_mesh().is_null()) {
			continue;
		}
		const AABB b = relative_to(this, mesh).xform(mesh->get_mesh()->get_aabb());
		all = first ? b : all.merge(b);
		first = false;
	}
	float bottom = 1e9f;
	for (const Wheel &w : wheels) {
		bottom = MIN(bottom, w.middle.y - radius + CLEARANCE);
	}
	const float top = all.position.y + all.size.y;
	if (first || top <= bottom) {
		return;
	}
	Ref<BoxShape3D> box;
	box.instantiate();
	box->set_size(Vector3(all.size.x, top - bottom, all.size.z));
	CollisionShape3D *shape = memnew(CollisionShape3D);
	shape->set_name("BodyCollider");
	shape->set_shape(box);
	shape->set_transform(get_transform() * Transform3D(Basis(), Vector3(all.get_center().x, (top + bottom) * 0.5f, all.get_center().z)));
	vehicle->add_child(shape);
}

void ModelVehicleVisual::_process(double p_delta) {
	if (!built) {
		built = true;
		_build();
	}
	if (!vehicle || wheels.empty()) {
		return;
	}
	const Vector3 forward = -vehicle->get_global_transform().basis.get_column(2).normalized();
	const float speed = vehicle->get_linear_velocity().dot(forward);
	spin -= speed / MAX(radius, 0.05f) * float(p_delta);
	spin = Math::fmod(spin, float(Math::TAU));
	const float steer = CLAMP(vehicle->get_input().steering, -1.0f, 1.0f) * Math::deg_to_rad(max_steer_degrees);
	for (const Wheel &w : wheels) {
		const float drop = vehicle->get_wheel_displacement(w.physics) - w.rest; // + when hanging lower than settled
		const Basis turn = Basis(Vector3(0, 1, 0), w.front ? -steer : 0.0f) * Basis(Vector3(1, 0, 0), spin);
		w.pivot->set_transform(Transform3D(turn, w.middle - Vector3(0, drop, 0)));
	}
}

void ModelVehicleVisual::set_model(const Ref<PackedScene> &p_model) {
	model = p_model;
}

Ref<PackedScene> ModelVehicleVisual::get_model() const {
	return model;
}

void ModelVehicleVisual::set_model_scale(float p_scale) {
	model_scale = p_scale;
}

float ModelVehicleVisual::get_model_scale() const {
	return model_scale;
}

void ModelVehicleVisual::set_model_yaw_degrees(float p_degrees) {
	model_yaw_degrees = p_degrees;
}

float ModelVehicleVisual::get_model_yaw_degrees() const {
	return model_yaw_degrees;
}

void ModelVehicleVisual::set_wheel_parts(const PackedStringArray &p_parts) {
	wheel_parts = p_parts;
}

PackedStringArray ModelVehicleVisual::get_wheel_parts() const {
	return wheel_parts;
}

void ModelVehicleVisual::set_max_steer_degrees(float p_degrees) {
	max_steer_degrees = p_degrees;
}

float ModelVehicleVisual::get_max_steer_degrees() const {
	return max_steer_degrees;
}

void ModelVehicleVisual::set_body_collider(bool p_enabled) {
	body_collider = p_enabled;
}

bool ModelVehicleVisual::get_body_collider() const {
	return body_collider;
}

} // namespace godot
