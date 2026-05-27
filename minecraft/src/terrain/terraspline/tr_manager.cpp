#include "terraspline.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// =============================================================================
// TerrainManager Implementation
// =============================================================================

void TerrainManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_chunk_size", "chunk_size"), &TerrainManager::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &TerrainManager::get_chunk_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_size"), "set_chunk_size", "get_chunk_size");

	ClassDB::bind_method(D_METHOD("get_chunk", "coords"), &TerrainManager::get_chunk);
	ClassDB::bind_method(D_METHOD("create_chunk", "coords"), &TerrainManager::create_chunk);
	ClassDB::bind_method(D_METHOD("delete_chunk", "coords"), &TerrainManager::delete_chunk);
	ClassDB::bind_method(D_METHOD("clear_chunks"), &TerrainManager::clear_chunks);
	ClassDB::bind_method(D_METHOD("get_active_chunks"), &TerrainManager::get_active_chunks);
}

TerrainManager::TerrainManager() {
	chunk_size = 64;
}

TerrainManager::~TerrainManager() {
	clear_chunks();
}

void TerrainManager::set_chunk_size(int p_size) {
	if (p_size <= 0) {
		return;
	}
	chunk_size = p_size;
}

int TerrainManager::get_chunk_size() const {
	return chunk_size;
}

Ref<TerrainHeightmap> TerrainManager::get_chunk(const Vector2i &p_coords) {
	if (chunks.has(p_coords)) {
		return chunks[p_coords];
	}
	return Ref<TerrainHeightmap>();
}

Ref<TerrainHeightmap> TerrainManager::create_chunk(const Vector2i &p_coords) {
	if (chunks.has(p_coords)) {
		return chunks[p_coords];
	}
	Ref<TerrainHeightmap> chunk;
	chunk.instantiate();
	chunk->initialize(chunk_size + 1, chunk_size + 1, 0.0f);
	chunks[p_coords] = chunk;
	return chunk;
}

void TerrainManager::delete_chunk(const Vector2i &p_coords) {
	if (chunks.has(p_coords)) {
		chunks.erase(p_coords);
	}
}

void TerrainManager::clear_chunks() {
	chunks.clear();
}

TypedArray<Vector2i> TerrainManager::get_active_chunks() const {
	TypedArray<Vector2i> active;
	for (const KeyValue<Vector2i, Ref<TerrainHeightmap>> &E : chunks) {
		active.push_back(E.key);
	}
	return active;
}

} // namespace godot
