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

void TerrainChunk::build_thumbnail(int p_size) {
	thumbnail.clear();
	thumbnail_size = 0;
	if (heightmap.is_null() || p_size <= 0) {
		return;
	}
	const int w = heightmap->get_width();
	const int h = heightmap->get_height();
	if (w <= 0 || h <= 0) {
		return;
	}
	const float *src = heightmap->get_data_ptrw();
	thumbnail_size = p_size;
	thumbnail.resize((size_t)p_size * p_size);
	for (int tz = 0; tz < p_size; ++tz) {
		const int z = Math::min(h - 1, (tz * h) / p_size);
		for (int tx = 0; tx < p_size; ++tx) {
			const int x = Math::min(w - 1, (tx * w) / p_size);
			thumbnail[(size_t)tz * p_size + tx] = src[z * w + x];
		}
	}
}

} // namespace godot
