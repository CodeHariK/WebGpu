/**
 * @file drive_debug_hud.cpp
 * @brief DriveDebugHud: the name label, the check box, and the wireframes: a MeshInstance3D of each
 * CollisionShape3D's debug lines (the line surfaces of Shape3D::get_debug_mesh), parented to that
 * shape so it follows it, made once and then only shown or hidden; and a wireframe sphere of each
 * vehicle wheel's radius on its WheelCollider marker (which the vehicle moves to the wheel each
 * frame). New shapes (vehicles added later, colliders built on a vehicle's first frame) are picked
 * up by a look every RESCAN seconds.
 */
#include "drive_debug_hud.h"
#include "game_manager/game_manager.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

namespace {

// p_shape's debug mesh with only its line surfaces (Godot also gives it filled faces).
Ref<ArrayMesh> lines_of(const Ref<Shape3D> &p_shape) {
	const Ref<ArrayMesh> debug = p_shape->get_debug_mesh();
	Ref<ArrayMesh> lines;
	lines.instantiate();
	for (int32_t s = 0; debug.is_valid() && s < debug->get_surface_count(); s++) {
		if (debug->surface_get_primitive_type(s) == Mesh::PRIMITIVE_LINES) {
			lines->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, debug->surface_get_arrays(s));
		}
	}
	return lines;
}

constexpr double RESCAN = 1.0; // seconds between looks for new collision shapes
constexpr int FONT_SIZE = 22;
const Color WIRE_COLOUR(0.2f, 1.0f, 0.35f);
// the classes whose collision shapes get wireframes
const char *ACTORS[] = { "ArcadeVehicle", "CelesteController", "SpringCharacter" };

} // namespace

void DriveDebugHud::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_wire_toggled", "on"), &DriveDebugHud::_on_wire_toggled);
}

void DriveDebugHud::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) {
		_build();
		set_process(true);
	}
}

void DriveDebugHud::_build() {
	VBoxContainer *box = memnew(VBoxContainer);
	box->set_anchors_preset(Control::PRESET_CENTER_TOP);
	box->set_position(Vector2(-140, 8));
	box->set_custom_minimum_size(Vector2(280, 0));
	add_child(box);
	name_label = memnew(Label);
	name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	name_label->add_theme_font_size_override("font_size", FONT_SIZE);
	name_label->add_theme_color_override("font_outline_color", Color(0, 0, 0));
	name_label->add_theme_constant_override("outline_size", 6);
	box->add_child(name_label);
	wire_box = memnew(CheckBox);
	wire_box->set_text("Collision shapes");
	wire_box->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	wire_box->set_focus_mode(Control::FOCUS_NONE); // keys stay with the game
	wire_box->connect("toggled", Callable(this, "_on_wire_toggled"));
	box->add_child(wire_box);
	wire_material.instantiate();
	wire_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	wire_material->set_albedo(WIRE_COLOUR);
	wire_material->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true); // seen through the car
	wire_material->set_render_priority(Material::RENDER_PRIORITY_MAX);
}

// A wireframe for every collision shape under p_node that has none yet.
void DriveDebugHud::_wire_shapes_under(Node *p_node) {
	const TypedArray<Node> shapes = p_node->find_children("*", "CollisionShape3D", true, false);
	for (int64_t i = 0; i < shapes.size(); i++) {
		CollisionShape3D *shape = Object::cast_to<CollisionShape3D>(shapes[i]);
		if (!shape || shape->get_shape().is_null() || wired.count(shape->get_instance_id())) {
			continue;
		}
		wired.insert(shape->get_instance_id());
		shape->add_child(_make_wire(shape->get_shape()));
	}
}

// A wireframe sphere on every wheel collider marker of p_vehicle (ArcadeVehicle's "WheelCollider"
// children, their radius in their metadata) that has none yet.
void DriveDebugHud::_wire_wheels_of(Node *p_vehicle) {
	for (int64_t i = 0; i < p_vehicle->get_child_count(); i++) {
		Node3D *marker = Object::cast_to<Node3D>(p_vehicle->get_child(i));
		if (!marker || !String(marker->get_name()).begins_with("WheelCollider") || !marker->has_meta("radius") || wired.count(marker->get_instance_id())) {
			continue;
		}
		wired.insert(marker->get_instance_id());
		Ref<SphereShape3D> sphere;
		sphere.instantiate();
		sphere->set_radius(float(double(marker->get_meta("radius"))));
		marker->add_child(_make_wire(sphere));
	}
}

// The wireframe of p_shape: hidden or shown as the box is, remembered to show or hide later.
MeshInstance3D *DriveDebugHud::_make_wire(const Ref<Shape3D> &p_shape) {
	MeshInstance3D *wire = memnew(MeshInstance3D);
	wire->set_name("CollisionWire");
	wire->set_mesh(lines_of(p_shape));
	wire->set_material_override(wire_material);
	wire->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	wire->set_visible(wires_shown);
	wires.push_back(ObjectID(wire->get_instance_id()));
	return wire;
}

void DriveDebugHud::_scan() {
	Node *root = get_tree() ? get_tree()->get_current_scene() : nullptr;
	if (!root) {
		return;
	}
	for (const char *type : ACTORS) {
		const TypedArray<Node> actors = root->find_children("*", type, true, false);
		for (int64_t i = 0; i < actors.size(); i++) {
			Node *actor = Object::cast_to<Node>(actors[i]);
			_wire_shapes_under(actor);
			_wire_wheels_of(actor);
		}
	}
}

void DriveDebugHud::_show_wires(bool p_shown) {
	wires_shown = p_shown;
	for (const ObjectID &id : wires) {
		if (MeshInstance3D *wire = Object::cast_to<MeshInstance3D>(ObjectDB::get_instance(id))) {
			wire->set_visible(p_shown);
		}
	}
}

void DriveDebugHud::_on_wire_toggled(bool p_on) {
	if (p_on) {
		_scan();
	}
	_show_wires(p_on);
}

void DriveDebugHud::_process(double p_delta) {
	GameManager *gm = GameManager::get_singleton();
	const Node *target = gm ? gm->get_active_target() : nullptr;
	if (name_label) {
		name_label->set_text(target ? String(target->get_name()) : String("(nothing selected)"));
	}
	rescan -= p_delta;
	if (wires_shown && rescan <= 0.0) {
		rescan = RESCAN;
		_scan();
	}
}

} // namespace godot
