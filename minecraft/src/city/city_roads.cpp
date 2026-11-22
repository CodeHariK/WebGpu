/**
 * @file city_roads.cpp
 * @brief The roads as strips: at every point of a road's line, two points half its width either
 * side (square to the line there: the average of the pieces before and after, lengthened at a bend
 * so the edges stay parallel, up to twice). Lower kinds lie lower, so where roads meet the bigger
 * one shows on top. The motorway: a deck DECK_THICKNESS thick at its own height at each point, a
 * pier under it every PIER_GAP metres where it is up over the water, and a collider of its top and
 * sides.
 */
#include "city_roads.h"
#include "city_data.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/surface_tool.hpp>

namespace godot::city {

namespace {

constexpr float DECK_THICKNESS = 0.8f;
constexpr float PIER_GAP = 14.0f; // metres between piers
constexpr float PIER_FROM = 2.0f; // metres: piers only where the deck is this high

struct Kind {
	const char *name;
	float lift; // metres over the land
	Color colour;
};

// Highways (and the ring road, arterials) dark asphalt; a district's main roads a warm light grey;
// streets between.
const Kind KINDS[] = {
	{ "street", 0.04f, Color(0.541f, 0.518f, 0.482f) },
	{ "main", 0.05f, Color(0.761f, 0.702f, 0.604f) },
	{ "highway", 0.06f, Color(0.243f, 0.231f, 0.224f) },
	{ "ring", 0.06f, Color(0.243f, 0.231f, 0.224f) },
	{ "arterial", 0.07f, Color(0.180f, 0.169f, 0.165f) },
};

// The two edge points either side of point p_i of p_line, half p_width out.
void edges_at(
		const PackedVector2Array &p_line,
		int64_t p_i,
		float p_width,
		Vector2 *r_left,
		Vector2 *r_right
) {
	const int64_t n = p_line.size();
	const Vector2 before = p_i > 0 ? (p_line[p_i] - p_line[p_i - 1]).normalized() : Vector2();
	const Vector2 after = p_i < n - 1 ? (p_line[p_i + 1] - p_line[p_i]).normalized() : Vector2();
	Vector2 along = (before + after).normalized();
	if (along == Vector2()) {
		along = before != Vector2() ? before : after;
	}
	const Vector2 side = Vector2(-along.y, along.x);
	const Vector2 piece = after != Vector2() ? after : before;
	const float stretch = MIN(2.0f, 1.0f / MAX(0.5f, Math::abs(side.dot(Vector2(-piece.y, piece.x)))));
	*r_left = p_line[p_i] + side * (p_width * 0.5f * stretch);
	*r_right = p_line[p_i] - side * (p_width * 0.5f * stretch);
}

// A quad (a, b, c, d going round) into p_st, two triangles, with normal p_normal.
void add_quad(
		const Ref<SurfaceTool> &p_st,
		const Vector3 &p_a,
		const Vector3 &p_b,
		const Vector3 &p_c,
		const Vector3 &p_d,
		const Vector3 &p_normal,
		const Color &p_colour,
		PackedVector3Array *r_faces
) {
	add_face(p_st, p_a, p_b, p_c, p_normal, p_colour, r_faces);
	add_face(p_st, p_a, p_c, p_d, p_normal, p_colour, r_faces);
}

// A strip along p_line at the heights p_heights (one per point), p_width wide, facing p_normal,
// into p_st.
void add_strip(
		const Ref<SurfaceTool> &p_st,
		const PackedVector2Array &p_line,
		const PackedFloat32Array &p_heights,
		float p_width,
		const Color &p_colour,
		const Vector3 &p_normal,
		PackedVector3Array *r_faces
) {
	for (int64_t i = 0; i + 1 < p_line.size(); i++) {
		Vector2 l0, r0, l1, r1;
		edges_at(p_line, i, p_width, &l0, &r0);
		edges_at(p_line, i + 1, p_width, &l1, &r1);
		const float y0 = p_heights[i];
		const float y1 = p_heights[i + 1];
		add_quad(p_st, Vector3(l0.x, y0, l0.y), Vector3(l1.x, y1, l1.y), Vector3(r1.x, y1, r1.y), Vector3(r0.x, y0, r0.y), p_normal, p_colour, r_faces);
	}
}

// The motorway: its deck (top, both sides, bottom), the collider from the same faces, and piers.
void build_motorway(
		Node3D *p_parent,
		const Dictionary &p_road
) {
	const PackedVector2Array line = read_line(p_road["points"]);
	const Array heights_in = p_road.get("heights", Array());
	const float width = read_float(p_road, "width", 20.0f);
	if (line.size() < 2) {
		return;
	}
	PackedFloat32Array top;
	PackedFloat32Array bottom;
	top.resize(line.size());
	bottom.resize(line.size());
	for (int64_t i = 0; i < line.size(); i++) {
		const float h = i < heights_in.size() ? float(double(heights_in[i])) : 0.4f;
		top.set(i, h + 0.05f);
		bottom.set(i, h - DECK_THICKNESS);
	}
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);
	PackedVector3Array faces;
	const Color asphalt = Color::html("#4d4946");
	const Color concrete = Color::html("#bdb6ab");
	add_strip(st, line, top, width, asphalt, Vector3(0, 1, 0), &faces);
	add_strip(st, line, bottom, width, concrete, Vector3(0, -1, 0), nullptr);
	for (int64_t i = 0; i + 1 < line.size(); i++) { // the deck's two sides
		Vector2 l0, r0, l1, r1;
		edges_at(line, i, width, &l0, &r0);
		edges_at(line, i + 1, width, &l1, &r1);
		const Vector2 along = (line[i + 1] - line[i]).normalized();
		const Vector3 left_normal = Vector3(-along.y, 0, along.x);
		add_quad(st, Vector3(l0.x, bottom[i], l0.y), Vector3(l1.x, bottom[i + 1], l1.y), Vector3(l1.x, top[i + 1], l1.y), Vector3(l0.x, top[i], l0.y), left_normal, concrete, &faces);
		add_quad(st, Vector3(r0.x, top[i], r0.y), Vector3(r1.x, top[i + 1], r1.y), Vector3(r1.x, bottom[i + 1], r1.y), Vector3(r0.x, bottom[i], r0.y), -left_normal, concrete, &faces);
	}
	MeshInstance3D *deck = memnew(MeshInstance3D);
	deck->set_name("Motorway");
	deck->set_mesh(st->commit());
	deck->set_material_override(vertex_colour_material(0.95f));
	p_parent->add_child(deck);

	StaticBody3D *body = memnew(StaticBody3D);
	body->set_name("MotorwayBody");
	p_parent->add_child(body);
	Ref<ConcavePolygonShape3D> shape;
	shape.instantiate();
	shape->set_faces(faces);
	shape->set_backface_collision_enabled(true);
	CollisionShape3D *collider = memnew(CollisionShape3D);
	collider->set_shape(shape);
	body->add_child(collider);

	// piers: every PIER_GAP metres along it where the deck is up
	PackedVector3Array pier_at;
	PackedFloat32Array pier_angle;
	float walked = 0.0f;
	for (int64_t i = 0; i + 1 < line.size(); i++) {
		const float piece = line[i].distance_to(line[i + 1]);
		walked += piece;
		if (walked < PIER_GAP || top[i] < PIER_FROM) {
			continue;
		}
		walked = 0.0f;
		pier_at.push_back(Vector3(line[i].x, bottom[i], line[i].y));
		pier_angle.push_back((line[i + 1] - line[i]).angle());
	}
	if (pier_at.is_empty()) {
		return;
	}
	Ref<BoxMesh> box;
	box.instantiate();
	Ref<MultiMesh> multi;
	multi.instantiate();
	multi->set_transform_format(MultiMesh::TRANSFORM_3D);
	multi->set_mesh(box);
	multi->set_instance_count(pier_at.size());
	for (int64_t k = 0; k < pier_at.size(); k++) {
		const float height = pier_at[k].y + 4.0f; // down to the sea floor
		multi->set_instance_transform(k, standing(pier_at[k].x, pier_at[k].z, pier_angle[k], Vector3(2.0f, height, width * 0.6f), -4.0f));
	}
	MultiMeshInstance3D *piers = memnew(MultiMeshInstance3D);
	piers->set_name("Piers");
	piers->set_multimesh(multi);
	Ref<StandardMaterial3D> pier_material;
	pier_material.instantiate();
	pier_material->set_albedo(Color::html("#8d867c"));
	piers->set_material_override(pier_material);
	p_parent->add_child(piers);
}

} // namespace

void build_roads(
		Node3D *p_parent,
		const Array &p_roads
) {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);
	bool any = false;
	for (int64_t r = 0; r < p_roads.size(); r++) {
		const Dictionary road = p_roads[r];
		const String kind = road.get("kind", "street");
		if (kind == "motorway") {
			build_motorway(p_parent, road);
			continue;
		}
		const Kind *style = &KINDS[0];
		for (const Kind &k : KINDS) {
			if (kind == k.name) {
				style = &k;
			}
		}
		const PackedVector2Array line = read_line(road["points"]);
		if (line.size() < 2) {
			continue;
		}
		PackedFloat32Array heights;
		heights.resize(line.size());
		heights.fill(style->lift);
		add_strip(st, line, heights, read_float(road, "width", 4.0f), style->colour, Vector3(0, 1, 0), nullptr);
		any = true;
	}
	if (!any) {
		return;
	}
	MeshInstance3D *roads = memnew(MeshInstance3D);
	roads->set_name("Roads");
	roads->set_mesh(st->commit());
	roads->set_material_override(vertex_colour_material(0.95f));
	roads->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	p_parent->add_child(roads);
}

} // namespace godot::city
