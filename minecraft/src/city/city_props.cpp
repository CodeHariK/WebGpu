/**
 * @file city_props.cpp
 * @brief Flat bits as one mesh (vertex colours): car parks (boxes), dock yards (polygons,
 * triangulated) and ponds (discs), each a few centimetres over the land; piers as boxes from just
 * over the sea up to the land's height, with box colliders.
 */
#include "city_props.h"
#include "city_data.h"
#include "city_ground.h"
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/geometry2d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/surface_tool.hpp>

namespace godot::city {

namespace {

constexpr int DISC_SIDES = 24;
const Color CAR_PARK_COLOUR = Color(0.616f, 0.604f, 0.580f);
const Color YARD_COLOUR = Color(0.765f, 0.749f, 0.714f);
const Color POND_COLOUR = Color(0.353f, 0.651f, 0.839f);
const Color PIER_COLOUR = Color(0.608f, 0.482f, 0.353f);

void add_triangle(
		const Ref<SurfaceTool> &p_st,
		const Vector3 &p_a,
		const Vector3 &p_b,
		const Vector3 &p_c,
		const Color &p_colour
) {
	add_face(p_st, p_a, p_b, p_c, Vector3(0, 1, 0), p_colour, nullptr);
}

// The four corners (x, z) of a box from the export.
void corners_of(
		const Dictionary &p_box,
		Vector2 r_corners[4]
) {
	const Vector2 centre(read_float(p_box, "x", 0.0f), read_float(p_box, "z", 0.0f));
	const float angle = read_float(p_box, "angle", 0.0f);
	const Vector2 u(Math::cos(angle), Math::sin(angle));
	const Vector2 v(-u.y, u.x);
	const float w = read_float(p_box, "width", 1.0f) * 0.5f;
	const float d = read_float(p_box, "depth", 1.0f) * 0.5f;
	r_corners[0] = centre - u * w - v * d;
	r_corners[1] = centre + u * w - v * d;
	r_corners[2] = centre + u * w + v * d;
	r_corners[3] = centre - u * w + v * d;
}

void add_flat_box(
		const Ref<SurfaceTool> &p_st,
		const Dictionary &p_box,
		float p_y,
		const Color &p_colour
) {
	Vector2 c[4];
	corners_of(p_box, c);
	add_triangle(p_st, Vector3(c[0].x, p_y, c[0].y), Vector3(c[1].x, p_y, c[1].y), Vector3(c[2].x, p_y, c[2].y), p_colour);
	add_triangle(p_st, Vector3(c[0].x, p_y, c[0].y), Vector3(c[2].x, p_y, c[2].y), Vector3(c[3].x, p_y, c[3].y), p_colour);
}

void add_polygon(
		const Ref<SurfaceTool> &p_st,
		const PackedVector2Array &p_points,
		float p_y,
		const Color &p_colour
) {
	const PackedInt32Array t = Geometry2D::get_singleton()->triangulate_polygon(p_points);
	for (int64_t k = 0; k + 2 < t.size(); k += 3) {
		const Vector2 a = p_points[t[k]];
		const Vector2 b = p_points[t[k + 1]];
		const Vector2 c = p_points[t[k + 2]];
		add_triangle(p_st, Vector3(a.x, p_y, a.y), Vector3(b.x, p_y, b.y), Vector3(c.x, p_y, c.y), p_colour);
	}
}

void add_disc(
		const Ref<SurfaceTool> &p_st,
		const Dictionary &p_circle,
		float p_y,
		const Color &p_colour
) {
	const Vector3 centre(read_float(p_circle, "x", 0.0f), p_y, read_float(p_circle, "z", 0.0f));
	const float r = read_float(p_circle, "radius", 1.0f);
	for (int k = 0; k < DISC_SIDES; k++) {
		const float a0 = Math::TAU * k / DISC_SIDES;
		const float a1 = Math::TAU * (k + 1) / DISC_SIDES;
		add_triangle(p_st, centre, centre + Vector3(Math::cos(a0), 0, Math::sin(a0)) * r, centre + Vector3(Math::cos(a1), 0, Math::sin(a1)) * r, p_colour);
	}
}

} // namespace

void build_props(
		Node3D *p_parent,
		const Dictionary &p_map
) {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);
	const Array car_parks = p_map.get("carParks", Array());
	for (int64_t i = 0; i < car_parks.size(); i++) {
		add_flat_box(st, car_parks[i], 0.03f, CAR_PARK_COLOUR);
	}
	const Array yards = p_map.get("dockYards", Array());
	for (int64_t i = 0; i < yards.size(); i++) {
		add_polygon(st, read_line(yards[i]), 0.02f, YARD_COLOUR);
	}
	const Array ponds = p_map.get("ponds", Array());
	for (int64_t i = 0; i < ponds.size(); i++) {
		add_disc(st, ponds[i], 0.03f, POND_COLOUR);
	}
	const Array piers = p_map.get("piers", Array());
	StaticBody3D *body = memnew(StaticBody3D);
	body->set_name("PiersBody");
	p_parent->add_child(body);
	const float pier_depth = -SEA_LEVEL + 0.3f; // from just over the sea up to the land's height
	for (int64_t i = 0; i < piers.size(); i++) {
		const Dictionary pier = piers[i];
		add_flat_box(st, pier, 0.0f, PIER_COLOUR);
		const Transform3D t = standing(read_float(pier, "x", 0.0f), read_float(pier, "z", 0.0f), read_float(pier, "angle", 0.0f), Vector3(read_float(pier, "width", 1.0f), pier_depth, read_float(pier, "depth", 1.0f)), -pier_depth);
		Ref<BoxShape3D> shape;
		shape.instantiate();
		shape->set_size(t.basis.get_scale());
		CollisionShape3D *collider = memnew(CollisionShape3D);
		collider->set_shape(shape);
		collider->set_transform(Transform3D(t.basis.orthonormalized(), t.origin));
		body->add_child(collider);
	}
	const Ref<StandardMaterial3D> material = vertex_colour_material(0.9f);
	MeshInstance3D *props = memnew(MeshInstance3D);
	props->set_name("Props");
	props->set_mesh(st->commit());
	props->set_material_override(material);
	props->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	p_parent->add_child(props);
}

} // namespace godot::city
