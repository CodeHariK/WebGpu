/**
 * @file tr_chunk.h
 * @brief TerrainChunk: the resident state of one streamed terrain chunk.
 */
#ifndef TR_CHUNK_H
#define TR_CHUNK_H

#include "tr_heightmap.h"
#include <cstdint>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <vector>

namespace godot {

/**
 * @class TerrainChunk
 * @brief Heightmap, scattered visuals and physics state for one chunk coordinate.
 *
 * Lifecycle (see ChunkState): a chunk is UNLOADED until the compositor creates it, GENERATING while
 * a job is computing its heightmap, VISUAL_ONLY once its MultiMeshes are in the scene, and
 * VISUAL_AND_PHYSICS while the player is within the physics radius and its cached shapes are live in
 * the PhysicsServer. Visual nodes are referenced by instance id so a chunk can be destroyed safely
 * even if the scene tree freed them first.
 */
class TerrainChunk : public RefCounted {
	GDCLASS(TerrainChunk,
			RefCounted)

public:
	enum ChunkState {
		STATE_UNLOADED = 0,
		STATE_GENERATING = 1,
		STATE_VISUAL_ONLY = 2,
		STATE_VISUAL_AND_PHYSICS = 3,
		STATE_MARKED_FOR_EVICTION = 4
	};

	/// One scatterer's collision shape plus every instance transform that uses it.
	struct PhysicsCache {
		RID shape_rid;
		std::vector<Transform3D> transforms;
	};

private:
	Vector2i chunk_coords;
	ChunkState current_state = STATE_UNLOADED;
	Ref<TerrainHeightmap> heightmap;

	std::vector<uint64_t> visual_nodes; // MultiMeshInstance3D instance ids
	RID physics_body_rid; // Static body while VISUAL_AND_PHYSICS, invalid otherwise
	std::vector<PhysicsCache> physics_caches;
	std::vector<float> thumbnail; // Downsampled heights (thumbnail_size²) for the stream map; empty if unused
	int thumbnail_size = 0;

protected:
	static void _bind_methods();

public:
	TerrainChunk();
	~TerrainChunk();

	Vector2i get_chunk_coords() const { return chunk_coords; }
	void set_chunk_coords(const Vector2i &p_coords) { chunk_coords = p_coords; }

	ChunkState get_state() const { return current_state; }
	void set_state(ChunkState p_state) { current_state = p_state; }

	Ref<TerrainHeightmap> get_heightmap() const { return heightmap; }
	void set_heightmap(const Ref<TerrainHeightmap> &p_heightmap) { heightmap = p_heightmap; }

	std::vector<uint64_t> &get_visual_nodes() { return visual_nodes; }
	const std::vector<uint64_t> &get_visual_nodes() const { return visual_nodes; }

	RID get_physics_body_rid() const { return physics_body_rid; }
	void set_physics_body_rid(const RID &p_rid) { physics_body_rid = p_rid; }

	std::vector<PhysicsCache> &get_physics_caches() { return physics_caches; }
	const std::vector<PhysicsCache> &get_physics_caches() const { return physics_caches; }

	/// Point-samples the heightmap into a p_size² grid (row-major, z then x) for the stream map.
	void build_thumbnail(int p_size);
	const std::vector<float> &get_thumbnail() const { return thumbnail; }
	int get_thumbnail_size() const { return thumbnail_size; }

	/// Frees the MultiMeshInstance3Ds (queue_free if in tree) and the physics body, if any.
	void release_visuals_and_physics();
};

} // namespace godot

#endif // TR_CHUNK_H
