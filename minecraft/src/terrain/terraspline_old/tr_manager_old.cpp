#include "terraspline_old.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// =============================================================================
// TerrainManagerOld Implementation
// =============================================================================

void TerrainManagerOld::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_chunk_size", "chunk_size"), &TerrainManagerOld::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &TerrainManagerOld::get_chunk_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_size"), "set_chunk_size", "get_chunk_size");

	ClassDB::bind_method(D_METHOD("get_chunk", "coords"), &TerrainManagerOld::get_chunk);
	ClassDB::bind_method(D_METHOD("create_chunk", "coords"), &TerrainManagerOld::create_chunk);
	ClassDB::bind_method(D_METHOD("delete_chunk", "coords"), &TerrainManagerOld::delete_chunk);
	ClassDB::bind_method(D_METHOD("clear_chunks"), &TerrainManagerOld::clear_chunks);
	ClassDB::bind_method(D_METHOD("get_active_chunks"), &TerrainManagerOld::get_active_chunks);
}

TerrainManagerOld::TerrainManagerOld() {
	chunk_size = 64;
}

TerrainManagerOld::~TerrainManagerOld() {
	clear_chunks();
}

void TerrainManagerOld::set_chunk_size(int p_size) {
	if (p_size <= 0) {
		return;
	}
	chunk_size = p_size;
}

int TerrainManagerOld::get_chunk_size() const {
	return chunk_size;
}

Ref<TerrainHeightmapOld> TerrainManagerOld::get_chunk(const Vector2i &p_coords) {
	if (chunks.has(p_coords)) {
		return chunks[p_coords];
	}
	return Ref<TerrainHeightmapOld>();
}

Ref<TerrainHeightmapOld> TerrainManagerOld::create_chunk(const Vector2i &p_coords) {
	if (chunks.has(p_coords)) {
		return chunks[p_coords];
	}
	Ref<TerrainHeightmapOld> chunk;
	chunk.instantiate();
	chunk->initialize(chunk_size + 1, chunk_size + 1, 0.0f);
	chunk->set_manager(this, p_coords);
	chunks[p_coords] = chunk;
	return chunk;
}

void TerrainManagerOld::delete_chunk(const Vector2i &p_coords) {
	if (chunks.has(p_coords)) {
		chunks.erase(p_coords);
	}
}

void TerrainManagerOld::clear_chunks() {
	chunks.clear();
}

TypedArray<Vector2i> TerrainManagerOld::get_active_chunks() const {
	TypedArray<Vector2i> active;
	for (const KeyValue<Vector2i, Ref<TerrainHeightmapOld>> &E : chunks) {
		active.push_back(E.key);
	}
	return active;
}

} // namespace godot
