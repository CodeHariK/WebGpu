/**
 * @file tr_chunk.cpp
 * @brief TerrainChunk implementation.
 */
#include "tr_chunk.h"
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>

namespace godot {

void TerrainChunk::_bind_methods() {}

TerrainChunk::TerrainChunk() {
	current_state = STATE_UNLOADED;
	physics_body_rid = RID();
}

TerrainChunk::~TerrainChunk() { release_visuals_and_physics(); }

void TerrainChunk::release_visuals_and_physics() {
	// Resolve through ObjectDB: the scene tree may already have freed a node.
	for (uint64_t id : visual_nodes) {
		Object *obj = ObjectDB::get_instance(id);
		MultiMeshInstance3D *node = obj ? Object::cast_to<MultiMeshInstance3D>(obj) : nullptr;
		if (!node) {
			continue;
		}
		if (node->is_inside_tree()) {
			node->queue_free();
		} else {
			memdelete(node);
		}
	}
	visual_nodes.clear();

	if (physics_body_rid.is_valid()) {
		PhysicsServer3D::get_singleton()->free_rid(physics_body_rid);
		physics_body_rid = RID();
	}
}

} // namespace godot
