// MCGrid: the terrain's collision, per chunk (terrain_collision). Two ways:
//
//   COLLISION_SHARED   (default) one shape per tile mesh and turn, built once and kept
//                      (_shared_tile_shape); the chunk's body only refers to them, one shape owner per
//                      tile, placed at its cell. No triangles are rebuilt when a chunk changes: an edit
//                      costs about 1 ms of terrain where the chunk mesh costs 7 - 18 ms.
//   COLLISION_MESH     one ConcavePolygonShape3D from every tile triangle of the chunk (the render
//                      triangles). Adding it to the physics world builds a search structure over all
//                      of them: about 0.5 us a triangle, 6 - 17 ms for a chunk of 12 000 - 30 000.
// Both give the same ground (downward rays over the whole map hit the same heights). Every collider
// is a StaticBody3D child of the chunk's mesh node on LAYER_TERRAIN, so picking and physics see the
// same objects in both modes.
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/util/mc_physics.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <algorithm>
#include <cmath>

namespace godot {

namespace {
// A static body on the terrain layer.
StaticBody3D *terrain_body() {
	StaticBody3D *body = memnew(StaticBody3D);
	body->set_collision_layer(toLayer(LAYER_TERRAIN));
	body->set_collision_mask(0);
	return body;
}

// A trimesh collider of p_faces on p_body.
void add_trimesh(
		StaticBody3D *p_body,
		const PackedVector3Array &p_faces
) {
	CollisionShape3D *shape = memnew(CollisionShape3D);
	Ref<ConcavePolygonShape3D> trimesh;
	trimesh.instantiate();
	trimesh->set_faces(p_faces);
	shape->set_shape(trimesh);
	p_body->add_child(shape);
}
} // namespace

const char *MCGrid::collision_name(int p_mode) {
	return p_mode == COLLISION_MESH ? "chunk mesh" : "shared tile shapes";
}

void MCGrid::set_terrain_collision(int p_mode) {
	terrain_collision = std::clamp(p_mode, 0, static_cast<int>(COLLISION_COUNT) - 1);
	for (Chunk &chunk : chunks) {
		chunk.dirty = true;
	}
	if (!chunks.empty()) {
		flush_dirty_chunks();
	}
}

Ref<Shape3D> MCGrid::_shared_tile_shape(
		const Ref<Mesh> &p_mesh,
		const Basis &p_turn
) {
	// the turn as one of 8: a quarter turn about y, and whether it is mirrored
	Vector3 x_axis = p_turn.xform(Vector3(1, 0, 0));
	int quarter = static_cast<int>(std::lround(std::atan2(x_axis.z, x_axis.x) / 1.5707963f)) & 3;
	int mirrored = p_turn.determinant() < 0.0f ? 4 : 0;
	// keyed by the mesh object (its id is not reused when a tile library is swapped, a RID's can be)
	uint64_t turn = static_cast<uint64_t>(quarter | mirrored);
	uint64_t key = static_cast<uint64_t>(p_mesh->get_instance_id()) ^ ((turn + 1) * 0x9E3779B97F4A7C15ull);
	auto found = shared_tile_shapes.find(key);
	if (found != shared_tile_shapes.end()) {
		return found->second;
	}
	PackedVector3Array faces = p_mesh->get_faces();
	for (int64_t i = 0; i < faces.size(); i++) {
		faces.set(i, p_turn.xform(faces[i]));
	}
	Ref<ConcavePolygonShape3D> shape;
	shape.instantiate();
	shape->set_faces(faces);
	shared_tile_shapes.emplace(key, shape);
	return shape;
}

void MCGrid::_build_chunk_collider(
		MeshInstance3D *p_node,
		const PackedVector3Array &p_faces,
		const std::vector<std::pair<Ref<Mesh>, Transform3D>> &p_tiles
) {
	uint64_t t0 = Time::get_singleton()->get_ticks_usec();
	StaticBody3D *body = terrain_body();
	if (terrain_collision == COLLISION_SHARED) {
		for (const std::pair<Ref<Mesh>, Transform3D> &tile : p_tiles) {
			uint32_t owner = body->create_shape_owner(body);
			body->shape_owner_add_shape(owner, _shared_tile_shape(tile.first, tile.second.basis));
			body->shape_owner_set_transform(owner, Transform3D(Basis(), tile.second.origin));
		}
	} else {
		add_trimesh(body, p_faces);
	}
	uint64_t t1 = Time::get_singleton()->get_ticks_usec();
	p_node->add_child(body);
	uint64_t t2 = Time::get_singleton()->get_ticks_usec();
	chunk_part_usec[1] += t1 - t0; // building the collider's shape(s)
	chunk_part_usec[2] += t2 - t1; // adding it to the physics world
}

} // namespace godot
