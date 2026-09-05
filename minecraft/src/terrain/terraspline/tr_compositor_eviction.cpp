/**
 * @file tr_compositor_eviction.cpp
 * @brief Keeping the resident set matched to the player: evicting far chunks and Terrain3D regions,
 * discovering chunks that entered the render radius, and waking/sleeping scatter physics.
 *
 * Coordinates: chunk keys and `global_world_offset` are "physical"; the player and radii are compared
 * in "logical" space (physical + offset) so an origin shift does not change what is in range.
 */
#include "tr_compositor.h"
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// ---------------------------------------------------------------------------------------------
// Periodic eviction / discovery (every DISCOVERY_INTERVAL_MS)
// ---------------------------------------------------------------------------------------------

void TerrainSplineCompositor::_check_and_evict_far_chunks() {
	if (!terrain) {
		return;
	}
	Object *target_api = _get_terrain_data_api();
	if (!target_api) {
		return;
	}
	Vector2 logical_player = _logical_player_position_2d();

	if (_bench) {
		// Benchmark: no eviction; just make sure every bench chunk exists.
		std::vector<Vector2i> missing;
		for (const Vector2i &c : _bench_chunks) {
			if (!chunk_buffers.has(c)) {
				missing.push_back(c);
			}
		}
		_enqueue_chunks(missing, /*allow_existing=*/false);
		return;
	}

	_evict_far_chunks(logical_player);
	_evict_far_regions(target_api, logical_player);
	_discover_missing_chunks(logical_player);
}

/// Drops resident chunks whose centre is beyond max_render_radius (TerrainChunk frees its visuals).
void TerrainSplineCompositor::_evict_far_chunks(const Vector2 &p_logical_player) {
	std::vector<Vector2i> chunks_to_erase;
	for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
		Vector2i cpos = E.key;
		Vector2 c_center((cpos.x + 0.5f) * chunk_size, (cpos.y + 0.5f) * chunk_size);
		Vector2 logical_c_center = c_center + global_world_offset;
		if (p_logical_player.distance_to(logical_c_center) > max_render_radius) {
			chunks_to_erase.push_back(cpos);
		}
	}
	for (Vector2i cpos : chunks_to_erase) {
		chunk_buffers.erase(cpos);
	}
}

/// Unloads Terrain3D regions beyond the render radius (plus half a region of margin) that no
/// resident chunk still lives in.
void TerrainSplineCompositor::_evict_far_regions(
		Object *p_target_api,
		const Vector2 &p_logical_player
) {
	TypedArray<Vector2i> region_locations = p_target_api->call("get_region_locations");
	int region_size = terrain->call("get_region_size");
	float vertex_spacing = terrain->call("get_vertex_spacing");
	float region_world_size = region_size * vertex_spacing;

	for (int i = 0; i < region_locations.size(); ++i) {
		Vector2i rloc = region_locations[i];
		Vector2 r_center((rloc.x + 0.5f) * region_world_size, (rloc.y + 0.5f) * region_world_size);
		Vector2 logical_r_center = r_center + global_world_offset;
		if (p_logical_player.distance_to(logical_r_center) <= max_render_radius + region_world_size * 0.5f) {
			continue;
		}

		bool region_is_used = false;
		for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
			Vector2i cpos = E.key;
			Vector3 chunk_pos_3d(cpos.x * chunk_size, 0.0f, cpos.y * chunk_size);
			Vector2i chunk_rloc = p_target_api->call("get_region_location", chunk_pos_3d);
			if (chunk_rloc == rloc) {
				region_is_used = true;
				break;
			}
		}
		if (!region_is_used) {
#if DEBUG
			UtilityFunctions::print("[Compositor] Evicting far region from GPU VRAM: ", rloc);
#endif
			p_target_api->call("remove_regionl", rloc, true);
		}
	}
}

/// Enqueues every chunk within the render radius that is not resident yet.
void TerrainSplineCompositor::_discover_missing_chunks(const Vector2 &p_logical_player) {
	int min_cx = (int)Math::floor((p_logical_player.x - max_render_radius) / chunk_size);
	int max_cx = (int)Math::floor((p_logical_player.x + max_render_radius) / chunk_size);
	int min_cz = (int)Math::floor((p_logical_player.y - max_render_radius) / chunk_size);
	int max_cz = (int)Math::floor((p_logical_player.y + max_render_radius) / chunk_size);
	Vector2i offset_chunks(global_world_offset.x / chunk_size, global_world_offset.y / chunk_size);

	std::vector<Vector2i> missing;
	for (int cx = min_cx; cx <= max_cx; ++cx) {
		for (int cz = min_cz; cz <= max_cz; ++cz) {
			Vector2i physical_cpos = Vector2i(cx, cz) - offset_chunks;
			if (chunk_buffers.has(physical_cpos)) {
				continue;
			}
			Vector2 logical_c_center((cx + 0.5f) * chunk_size, (cz + 0.5f) * chunk_size);
			if (p_logical_player.distance_to(logical_c_center) > max_render_radius) {
				continue;
			}
			missing.push_back(physical_cpos);
		}
	}

	if (!missing.empty()) {
#if DEBUG
		UtilityFunctions::print(
				"[Compositor] Discovered ", (int)missing.size(), " new chunks inside render radius; queued."
		);
#endif
		_enqueue_chunks(missing, /*allow_existing=*/false);
	}
}

// ---------------------------------------------------------------------------------------------
// Physics culling (every frame)
// ---------------------------------------------------------------------------------------------

/// Distance from the player to the nearest point of the chunk's footprint (not its centre): with
/// 256 m chunks and a 150 m radius, centre-distance left neighbouring chunks' collision asleep while
/// their scattered meshes were plainly visible.
float TerrainSplineCompositor::_distance_to_chunk_edge(
		const Vector2i &p_chunk,
		const Vector2 &p_logical_player
) const {
	Rect2 logical_rect(
			Vector2(p_chunk.x * chunk_size, p_chunk.y * chunk_size) + global_world_offset,
			Vector2(chunk_size, chunk_size)
	);
	Vector2 nearest(
			Math::clamp(p_logical_player.x, logical_rect.position.x, logical_rect.position.x + logical_rect.size.x),
			Math::clamp(p_logical_player.y, logical_rect.position.y, logical_rect.position.y + logical_rect.size.y)
	);
	return p_logical_player.distance_to(nearest);
}

/// Wakes physics for chunks inside max_physics_radius and sleeps it for chunks outside.
void TerrainSplineCompositor::_check_chunk_physics_culling() {
	if (!terrain) {
		return;
	}
	Vector2 logical_player = _logical_player_position_2d();

	for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
		Ref<TerrainChunk> chunk = E.value;
		if (chunk.is_null()) {
			continue;
		}
		float dist = _distance_to_chunk_edge(chunk->get_chunk_coords(), logical_player);
		if (dist <= max_physics_radius) {
			if (chunk->get_state() == TerrainChunk::STATE_VISUAL_ONLY) {
				_update_chunk_physics(chunk);
			}
		} else if (chunk->get_state() == TerrainChunk::STATE_VISUAL_AND_PHYSICS) {
			_update_chunk_physics(chunk);
		}
	}
}

/**
 * @brief Toggles a chunk between VISUAL_ONLY and VISUAL_AND_PHYSICS.
 * Waking creates one static body and adds every cached shape transform; sleeping frees the body.
 * A chunk with no physics caches is marked VISUAL_AND_PHYSICS without creating a body.
 */
void TerrainSplineCompositor::_update_chunk_physics(const Ref<TerrainChunk> &p_chunk) {
	if (!terrain) {
		return;
	}
	Node3D *terrain_3d = Object::cast_to<Node3D>(terrain);
	if (!terrain_3d) {
		return;
	}
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();

	if (p_chunk->get_state() == TerrainChunk::STATE_VISUAL_ONLY) {
		if (p_chunk->get_physics_caches().empty()) {
			p_chunk->set_state(TerrainChunk::STATE_VISUAL_AND_PHYSICS);
			return;
		}
		RID space_rid = terrain_3d->get_world_3d()->get_space();
		RID body_rid = ps->body_create();
		ps->body_set_mode(body_rid, PhysicsServer3D::BODY_MODE_STATIC);
		ps->body_set_space(body_rid, space_rid);
		for (const TerrainChunk::PhysicsCache &pc : p_chunk->get_physics_caches()) {
			for (const Transform3D &t : pc.transforms) {
				ps->body_add_shape(body_rid, pc.shape_rid, t);
			}
		}
		p_chunk->set_physics_body_rid(body_rid);
		p_chunk->set_state(TerrainChunk::STATE_VISUAL_AND_PHYSICS);
	} else if (p_chunk->get_state() == TerrainChunk::STATE_VISUAL_AND_PHYSICS) {
		RID body_rid = p_chunk->get_physics_body_rid();
		if (body_rid.is_valid()) {
			ps->free_rid(body_rid);
			p_chunk->set_physics_body_rid(RID());
		}
		p_chunk->set_state(TerrainChunk::STATE_VISUAL_ONLY);
	}
}

} // namespace godot
