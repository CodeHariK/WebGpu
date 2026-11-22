/**
 * @file city_ground.cpp
 * @brief The ground: each island's coast loop triangulated into flat land at y = 0, a sandy edge
 * from the coast down under the sea, lakes as water just over the land, a sea plane, and the
 * colliders (the land's triangles, a flat sea floor).
 */
#include "city_ground.h"
#include "city_data.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/geometry2d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/world_boundary_shape3d.hpp>

namespace godot::city {

namespace {

const Color LAND_COLOUR = Color(0.549f, 0.753f, 0.416f);
const Color SAND_COLOUR = Color(0.914f, 0.843f, 0.612f);
const Color LAKE_COLOUR = Color(0.353f, 0.651f, 0.839f);
const Color SEA_COLOUR = Color(0.247f, 0.525f, 0.741f);

// A loop's triangles (flat, at p_y, facing up) into p_st, and into p_faces for a collider; false if
// the loop could not be triangulated.
bool add_flat_loop(
		const Ref<SurfaceTool> &p_st,
		const PackedVector2Array &p_loop,
		float p_y,
		const Color &p_colour,
		PackedVector3Array *r_faces
) {
	PackedVector2Array loop = p_loop;
	PackedInt32Array triangles = Geometry2D::get_singleton()->triangulate_polygon(loop);
	if (triangles.is_empty()) {
		loop.reverse();
		triangles = Geometry2D::get_singleton()->triangulate_polygon(loop);
	}
	if (triangles.is_empty()) {
		return false;
	}
	for (int64_t t = 0; t + 2 < triangles.size(); t += 3) {
		Vector3 corners[3];
		for (int k = 0; k < 3; k++) {
			const Vector2 p = loop[triangles[t + k]];
			corners[k] = Vector3(p.x, p_y, p.y);
		}
		add_face(p_st, corners[0], corners[1], corners[2], Vector3(0, 1, 0), p_colour, r_faces);
	}
	return true;
}

// The sandy edge round an island: a wall from the coast at y = 0 down to EDGE_DEPTH under the sea,
// facing out.
void add_edge(
		const Ref<SurfaceTool> &p_st,
		const PackedVector2Array &p_loop
) {
	const int64_t n = p_loop.size();
	const float bottom = SEA_LEVEL - EDGE_DEPTH;
	for (int64_t i = 0; i < n; i++) {
		const Vector2 a = p_loop[i];
		const Vector2 b = p_loop[(i + 1) % n];
		const Vector2 along = (b - a).normalized();
		const Vector3 out = Vector3(along.y, 0, -along.x); // land on the left: out is to the right
		const Vector3 top_a(a.x, 0, a.y);
		const Vector3 top_b(b.x, 0, b.y);
		const Vector3 low_a(a.x, bottom, a.y);
		const Vector3 low_b(b.x, bottom, b.y);
		add_face(p_st, top_a, top_b, low_b, out, SAND_COLOUR, nullptr);
		add_face(p_st, top_a, low_b, low_a, out, SAND_COLOUR, nullptr);
	}
}

} // namespace

void build_ground(
		Node3D *p_parent,
		const Array &p_coastlines,
		float p_size
) {
	Ref<SurfaceTool> land;
	land.instantiate();
	land->begin(Mesh::PRIMITIVE_TRIANGLES);
	Ref<SurfaceTool> lakes;
	lakes.instantiate();
	lakes->begin(Mesh::PRIMITIVE_TRIANGLES);
	PackedVector3Array faces;
	bool any_lake = false;
	for (int64_t i = 0; i < p_coastlines.size(); i++) {
		const PackedVector2Array loop = read_line(p_coastlines[i]);
		if (loop.size() < 3) {
			continue;
		}
		if (signed_area(loop) < 0.0f) { // an island
			add_flat_loop(land, loop, 0.0f, LAND_COLOUR, &faces);
			add_edge(land, loop);
		} else { // a lake
			any_lake = add_flat_loop(lakes, loop, 0.03f, LAKE_COLOUR, nullptr) || any_lake;
		}
	}
	const Ref<StandardMaterial3D> material = vertex_colour_material(0.9f);
	MeshInstance3D *land_mesh = memnew(MeshInstance3D);
	land_mesh->set_name("Land");
	land_mesh->set_mesh(land->commit());
	land_mesh->set_material_override(material);
	p_parent->add_child(land_mesh);
	if (any_lake) {
		MeshInstance3D *lake_mesh = memnew(MeshInstance3D);
		lake_mesh->set_name("Lakes");
		lake_mesh->set_mesh(lakes->commit());
		lake_mesh->set_material_override(material);
		p_parent->add_child(lake_mesh);
	}

	Ref<PlaneMesh> sea_plane;
	sea_plane.instantiate();
	sea_plane->set_size(Vector2(p_size * 4.0f, p_size * 4.0f));
	Ref<StandardMaterial3D> sea_material;
	sea_material.instantiate();
	sea_material->set_albedo(SEA_COLOUR);
	sea_material->set_roughness(0.25f);
	MeshInstance3D *sea = memnew(MeshInstance3D);
	sea->set_name("Sea");
	sea->set_mesh(sea_plane);
	sea->set_material_override(sea_material);
	sea->set_position(Vector3(0, SEA_LEVEL, 0));
	p_parent->add_child(sea);

	StaticBody3D *body = memnew(StaticBody3D);
	body->set_name("GroundBody");
	p_parent->add_child(body);
	Ref<ConcavePolygonShape3D> land_shape;
	land_shape.instantiate();
	land_shape->set_faces(faces);
	land_shape->set_backface_collision_enabled(true); // either winding stops a car
	CollisionShape3D *land_collider = memnew(CollisionShape3D);
	land_collider->set_name("Land");
	land_collider->set_shape(land_shape);
	body->add_child(land_collider);
	Ref<WorldBoundaryShape3D> floor_shape;
	floor_shape.instantiate();
	CollisionShape3D *floor_collider = memnew(CollisionShape3D);
	floor_collider->set_name("SeaFloor");
	floor_collider->set_shape(floor_shape);
	floor_collider->set_position(Vector3(0, SEA_FLOOR, 0));
	body->add_child(floor_collider);
}

} // namespace godot::city
