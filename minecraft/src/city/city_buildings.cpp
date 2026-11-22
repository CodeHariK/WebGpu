/**
 * @file city_buildings.cpp
 * @brief Buildings as instances: one MultiMesh per shape (box, cylinder, roof pyramid, tree cone),
 * each instance moved, turned and stretched to its building, in its kind's colour (houses' roofs in
 * their roof colour). One static body holds a box or cylinder collider per building and a thin one
 * per tree trunk. Houses and downtown blocks are the building kit's models (city_kit) when it is
 * there, else placeholder boxes (houses with a pyramid roof).
 */
#include "city_buildings.h"
#include "city_data.h"
#include "city_kit.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <vector>

namespace godot::city {

namespace {

constexpr float ROOF_PITCH = 0.45f; // a house's roof is this times its narrower side high
constexpr float TRUNK_RADIUS = 0.4f; // metres: a tree's collider

struct Instance {
	Transform3D transform;
	Color colour;
};

const char *ROOF_COLOURS[] = { "#d9785f", "#c4655a", "#8e9bb5", "#e2b46c", "#b5836b", "#9db39d" };

// A kind's walls colour, picked by p_index among its shades.
Color colour_of(
		const String &p_kind,
		int64_t p_index
) {
	if (p_kind == "house") {
		return Color::html("#f3ead8");
	}
	if (p_kind == "downtown") {
		const char *shades[] = { "#b9b3d6", "#a7b4cf", "#c9c2b3", "#9fa9b8" };
		return Color::html(shades[p_index % 4]);
	}
	if (p_kind == "tower") {
		return Color::html("#e8d9a8");
	}
	if (p_kind == "mall") {
		return Color::html("#e9a39b");
	}
	if (p_kind == "warehouse") {
		return Color::html("#c98d6a");
	}
	if (p_kind == "ship") {
		return Color::html("#f4f1ea");
	}
	return Color::html("#a9adb3"); // industry
}

// One MultiMeshInstance3D of p_mesh for p_instances, coloured per instance.
void add_instances(
		Node3D *p_parent,
		const String &p_name,
		const Ref<Mesh> &p_mesh,
		const std::vector<Instance> &p_instances
) {
	if (p_instances.empty()) {
		return;
	}
	Ref<MultiMesh> multi;
	multi.instantiate();
	multi->set_transform_format(MultiMesh::TRANSFORM_3D);
	multi->set_use_colors(true);
	multi->set_mesh(p_mesh);
	multi->set_instance_count(int32_t(p_instances.size()));
	for (size_t k = 0; k < p_instances.size(); k++) {
		multi->set_instance_transform(int32_t(k), p_instances[k].transform);
		multi->set_instance_color(int32_t(k), p_instances[k].colour);
	}
	const Ref<StandardMaterial3D> material = vertex_colour_material(0.85f);
	MultiMeshInstance3D *node = memnew(MultiMeshInstance3D);
	node->set_name(p_name);
	node->set_multimesh(multi);
	node->set_material_override(material);
	p_parent->add_child(node);
}

// A collider for a box or cylinder at p_transform (a unit shape's transform: its scale is the size).
void add_collider(
		StaticBody3D *p_body,
		const Transform3D &p_transform,
		bool p_cylinder
) {
	const Vector3 size = p_transform.basis.get_scale();
	CollisionShape3D *collider = memnew(CollisionShape3D);
	if (p_cylinder) {
		Ref<CylinderShape3D> shape;
		shape.instantiate();
		shape->set_radius(size.x * 0.5f);
		shape->set_height(size.y);
		collider->set_shape(shape);
	} else {
		Ref<BoxShape3D> shape;
		shape.instantiate();
		shape->set_size(size);
		collider->set_shape(shape);
	}
	collider->set_transform(Transform3D(p_transform.basis.orthonormalized(), p_transform.origin));
	p_body->add_child(collider);
}

} // namespace

void build_buildings(
		Node3D *p_parent,
		const Array &p_buildings,
		const Array &p_trees
) {
	std::vector<Instance> boxes;
	std::vector<Instance> cylinders;
	std::vector<Instance> roofs;
	std::vector<Instance> trees;
	StaticBody3D *body = memnew(StaticBody3D);
	body->set_name("BuildingsBody");
	p_parent->add_child(body);
	BuildingKit kit;
	const bool has_kit = kit.load();
	for (int64_t i = 0; i < p_buildings.size(); i++) {
		const Dictionary b = p_buildings[i];
		const String kind = b.get("kind", "house");
		const float x = read_float(b, "x", 0.0f);
		const float z = read_float(b, "z", 0.0f);
		const float height = read_float(b, "height", 3.0f);
		const float base = kind == "ship" ? -0.9f : 0.0f; // ships sit in the water
		const Color colour = colour_of(kind, i);
		if (String(b.get("shape", "box")) == "cylinder") {
			const float diameter = read_float(b, "radius", 5.0f) * 2.0f;
			const Transform3D t = standing(x, z, 0.0f, Vector3(diameter, height, diameter), base);
			cylinders.push_back({ t, colour });
			add_collider(body, t, true);
			continue;
		}
		const float angle = read_float(b, "angle", 0.0f);
		const float width = read_float(b, "width", 5.0f);
		const float depth = read_float(b, "depth", 5.0f);
		if (has_kit && (kind == "house" || kind == "downtown")) { // the kit's models instead of a placeholder
			const Box box{ x, z, angle, width, depth };
			const int floors = int(b.get("floors", 1));
			const bool done = kind == "house" ? kit.place_house(box, floors, uint32_t(i), body) : kit.place_downtown(box, floors, uint32_t(i), body);
			if (done) {
				continue;
			}
		}
		const Transform3D t = standing(x, z, angle, Vector3(width, height, depth), base);
		boxes.push_back({ t, colour });
		add_collider(body, t, false);
		if (kind == "house") {
			const int64_t roof = int64_t(b.get("roof", 0)) % 6;
			const float pitch = MIN(width, depth) * ROOF_PITCH;
			roofs.push_back({ standing(x, z, angle, Vector3(width, pitch, depth), height), Color::html(ROOF_COLOURS[roof]) });
		}
	}
	for (int64_t i = 0; i < p_trees.size(); i++) {
		const Dictionary t = p_trees[i];
		const float r = read_float(t, "radius", 1.5f);
		const Transform3D at = standing(read_float(t, "x", 0.0f), read_float(t, "z", 0.0f), 0.0f, Vector3(r * 2.0f, r * 3.0f + 1.5f, r * 2.0f), 0.0f);
		trees.push_back({ at, Color::html(i % 2 ? "#3f7d33" : "#4f9a3c") });
		add_collider(body, Transform3D(Basis().scaled(Vector3(TRUNK_RADIUS * 2.0f, at.basis.get_scale().y, TRUNK_RADIUS * 2.0f)), at.origin), true);
	}

	Ref<BoxMesh> box;
	box.instantiate();
	Ref<CylinderMesh> cylinder;
	cylinder.instantiate();
	cylinder->set_top_radius(0.5f);
	cylinder->set_bottom_radius(0.5f);
	cylinder->set_height(1.0f);
	cylinder->set_radial_segments(24);
	Ref<CylinderMesh> pyramid; // a 4-sided cone, turned below so its base is the unit square
	pyramid.instantiate();
	pyramid->set_top_radius(0.0f);
	pyramid->set_bottom_radius(Math::SQRT12);
	pyramid->set_height(1.0f);
	pyramid->set_radial_segments(4);
	pyramid->set_rings(0);
	Ref<CylinderMesh> cone;
	cone.instantiate();
	cone->set_top_radius(0.0f);
	cone->set_bottom_radius(0.5f);
	cone->set_height(1.0f);
	cone->set_radial_segments(8);
	cone->set_rings(0);
	for (Instance &roof : roofs) { // the pyramid's corners onto the box's corners
		roof.transform.basis = roof.transform.basis * Basis(Vector3(0, 1, 0), Math::PI / 4.0);
	}
	add_instances(p_parent, "Buildings", box, boxes);
	if (has_kit) {
		Node3D *kit_node = memnew(Node3D);
		kit_node->set_name("Kit");
		p_parent->add_child(kit_node);
		kit.build(kit_node);
	}
	add_instances(p_parent, "Towers", cylinder, cylinders);
	add_instances(p_parent, "Roofs", pyramid, roofs);
	add_instances(p_parent, "Trees", cone, trees);
}

} // namespace godot::city
