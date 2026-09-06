/**
 * @file tr_compositor_rebuild.cpp
 * @brief Deciding which chunks a rebuild touches, and running or enqueuing that work.
 *
 * apply_all_splines() is the single entry point for "something changed". Two modes:
 *  - full rebuild (compositor_full_rebuild): every chunk in range (game) or under any spline (editor)
 *  - dirty-rect update: only chunks under the merged dirty rectangles of the changed splines
 * In the editor the chunks are generated synchronously for immediate feedback; in the game they are
 * enqueued and streamed under the frame budget (tr_compositor_stream.cpp).
 */
#include "tr_compositor.h"
#include "tr_deformer.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void TerrainSplineCompositor::apply_all_splines() {
	uint64_t t_start = Time::get_singleton()->get_ticks_usec();

	if (!terrain) {
		UtilityFunctions::printerr("[Compositor] ABORT: Terrain3D node not assigned.");
		return;
	}
	Object *target_api = _get_terrain_data_api();
	if (!target_api) {
		UtilityFunctions::printerr("[Compositor] ABORT: Terrain3D Data/Storage is not initialized!");
		return;
	}
	if (!_is_terrain_ready()) {
		return; // Retried from _on_process().
	}

	// Workers read the splines' baked caches; finish them before anything below can rebake.
	_wait_for_jobs_in_flight();
	_warn_if_vertex_spacing_mismatch();

	std::vector<ProceduralSpline3D *> splines = _gather_splines();
	uint64_t t_gather = Time::get_singleton()->get_ticks_usec();

	// Always consume the splines' dirty rects, even on a full rebuild, so they don't linger.
	Rect2 master_dirty_rect;
	bool has_master_dirty = _collect_dirty_rect(splines, master_dirty_rect);

	Vector2 logical_player_pos_2d = _logical_player_position_2d();
	HashMap<Vector2i, bool> active_grid_chunks;

	if (compositor_full_rebuild) {
#if DEBUG
		UtilityFunctions::print("\n=== [Terrain Master Grid] GLOBAL REBUILD STARTED ===");
		UtilityFunctions::print(
				"[Compositor] Chunks in buffer: ", (int)chunk_buffers.size(),
				" | Player logical pos: ", logical_player_pos_2d, " | Render radius: ", max_render_radius, "m"
		);
#endif
		_select_full_rebuild_chunks(splines, logical_player_pos_2d, active_grid_chunks);
	} else {
		if (!has_master_dirty) {
			return;
		}
#if DEBUG
		UtilityFunctions::print("\n=== [Terrain Master Grid] LOCAL DIRTY RECT UPDATE ===");
		UtilityFunctions::print(
				"[Compositor] Chunks in buffer: ", (int)chunk_buffers.size(),
				" | Player logical pos: ", logical_player_pos_2d, " | Dirty Rect: ", master_dirty_rect
		);
#endif
		_select_dirty_rect_chunks(master_dirty_rect, logical_player_pos_2d, active_grid_chunks);
	}

	compositor_full_rebuild = false;

#if DEBUG
	uint64_t t_grid_calc = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("[Compositor] Grid chunk calculation time: ", (t_grid_calc - t_gather) / 1000.0, " ms.");
#endif

	if (active_grid_chunks.size() == 0) {
#if DEBUG
		UtilityFunctions::print("[Compositor] 0 active chunks to update. Aborting.");
#endif
		return;
	}

#if DEBUG
	UtilityFunctions::print(" -> Recalculating exactly ", (int)active_grid_chunks.size(), " isolated chunks...");
#endif

	std::vector<Vector2i> chunks_to_generate;
	for (const KeyValue<Vector2i, bool> &E : active_grid_chunks) {
		chunks_to_generate.push_back(E.key);
	}
	_run_rebuild(chunks_to_generate, splines, target_api);

	// Activate physics inside the radius right away.
	_check_chunk_physics_culling();

#if DEBUG
	uint64_t t_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("=== [Terrain Master Grid] TOTAL UPDATE TIME: ", (t_end - t_start) / 1000.0, " ms ===\n");
#endif
}

/**
 * @brief Terrain3DData is only usable once the Terrain3D node has entered the tree and initialized
 * its data. Calling into it earlier yields NaN/INT_MIN region locations and can hang inside Terrain3D
 * (region_size == 0) - this happens while the editor is still instantiating the scene. Terrain3D
 * initializes on ENTER_TREE, so "both in tree" is the readiness condition. (region_size lives on the
 * Terrain3D node, not on Terrain3DData.) When not ready, flags a retry from _on_process().
 */
bool TerrainSplineCompositor::_is_terrain_ready() {
	if (!is_inside_tree() || !terrain->is_inside_tree() || (int)terrain->call("get_region_size") <= 0) {
		if (!_rebuild_retry_pending) {
			UtilityFunctions::print("[Compositor] Terrain3D not ready yet; will retry on next process frame.");
		}
		_rebuild_retry_pending = true;
		compositor_full_rebuild = true;
		return false;
	}
	_rebuild_retry_pending = false;
	_rebuild_retry_frames = 0;
	return true;
}

/// Heightmaps are stamped at 1 pixel per metre; Terrain3D must agree or chunks import at the wrong scale.
void TerrainSplineCompositor::_warn_if_vertex_spacing_mismatch() {
	float vertex_spacing = terrain->call("get_vertex_spacing");
	if (!Math::is_equal_approx(vertex_spacing, 1.0f) && !_warned_vertex_spacing) {
		_warned_vertex_spacing = true;
		UtilityFunctions::printerr(
				"[Compositor] Terrain3D vertex_spacing is ", vertex_spacing,
				" but the compositor assumes 1.0. Chunks will import at the wrong scale."
		);
	}
}

/// Merges (and consumes) the dirty rects of all changed splines. Returns false if none was dirty.
bool TerrainSplineCompositor::_collect_dirty_rect(
		const std::vector<ProceduralSpline3D *> &p_splines,
		Rect2 &r_rect
) const {
	bool has_any = false;
	for (ProceduralSpline3D *spline : p_splines) {
		if (!spline->get_is_dirty()) {
			continue;
		}
		Rect2 sd = spline->consume_dirty_rect();
		if (!sd.has_area()) {
			continue;
		}
		r_rect = has_any ? r_rect.merge(sd) : sd;
		has_any = true;
	}
	if (!has_any) {
		return false;
	}
	// A terrain-following road samples the ground through every other spline's deformers, so any
	// change anywhere can move its profile: include the footprint of every such spline.
	for (ProceduralSpline3D *spline : p_splines) {
		TypedArray<Node> children = spline->get_children();
		for (int i = 0; i < children.size(); ++i) {
			TerrainSplineDeformer *d = Object::cast_to<TerrainSplineDeformer>(children[i]);
			if (d && d->get_height_source() == TerrainSplineDeformer::HEIGHT_TERRAIN) {
				r_rect = r_rect.merge(spline->get_padded_aabb());
				break;
			}
		}
	}
	return true;
}

/**
 * @brief Full rebuild chunk set. Editor: every chunk under any spline's padded AABB (the whole
 * design is visible). Game: every chunk whose centre is within max_render_radius of the player
 * (benchmark mode substitutes its fixed block).
 */
void TerrainSplineCompositor::_select_full_rebuild_chunks(
		const std::vector<ProceduralSpline3D *> &p_splines,
		const Vector2 &p_logical_player,
		HashMap<Vector2i,
				bool> &r_chunks
) const {
	if (Engine::get_singleton()->is_editor_hint()) {
		for (ProceduralSpline3D *spline : p_splines) {
			Rect2 sd = spline->get_padded_aabb();
			int s_min_cx = (int)Math::floor(sd.position.x / chunk_size);
			int s_max_cx = (int)Math::floor((sd.position.x + sd.size.x) / chunk_size);
			int s_min_cz = (int)Math::floor(sd.position.y / chunk_size);
			int s_max_cz = (int)Math::floor((sd.position.y + sd.size.y) / chunk_size);
			for (int cx = s_min_cx; cx <= s_max_cx; ++cx) {
				for (int cz = s_min_cz; cz <= s_max_cz; ++cz) {
					r_chunks[Vector2i(cx, cz)] = true;
				}
			}
		}
		return;
	}

	if (_bench) {
		for (const Vector2i &c : _bench_chunks) {
			r_chunks[c] = true;
		}
		return;
	}

	int player_cx = (int)Math::floor(p_logical_player.x / chunk_size);
	int player_cz = (int)Math::floor(p_logical_player.y / chunk_size);
	int radius_chunks = (int)Math::ceil(max_render_radius / chunk_size);
	for (int cx = player_cx - radius_chunks; cx <= player_cx + radius_chunks; ++cx) {
		for (int cz = player_cz - radius_chunks; cz <= player_cz + radius_chunks; ++cz) {
			Vector2 c_center((cx + 0.5f) * chunk_size, (cz + 0.5f) * chunk_size);
			Vector2 logical_c_center = c_center + global_world_offset;
			if (p_logical_player.distance_to(logical_c_center) <= max_render_radius) {
				r_chunks[Vector2i(cx, cz)] = true;
			}
		}
	}
}

/// Chunks under the dirty rect; in the game additionally limited to the render radius.
void TerrainSplineCompositor::_select_dirty_rect_chunks(
		const Rect2 &p_dirty_rect,
		const Vector2 &p_logical_player,
		HashMap<Vector2i,
				bool> &r_chunks
) const {
	bool is_editor = Engine::get_singleton()->is_editor_hint();
	int min_cx = (int)Math::floor(p_dirty_rect.position.x / chunk_size);
	int max_cx = (int)Math::floor((p_dirty_rect.position.x + p_dirty_rect.size.x) / chunk_size);
	int min_cz = (int)Math::floor(p_dirty_rect.position.y / chunk_size);
	int max_cz = (int)Math::floor((p_dirty_rect.position.y + p_dirty_rect.size.y) / chunk_size);

	for (int cx = min_cx; cx <= max_cx; ++cx) {
		for (int cz = min_cz; cz <= max_cz; ++cz) {
			if (is_editor) {
				r_chunks[Vector2i(cx, cz)] = true;
				continue;
			}
			Vector2 c_center((cx + 0.5f) * chunk_size, (cz + 0.5f) * chunk_size);
			Vector2 logical_c_center = c_center + global_world_offset;
			if (p_logical_player.distance_to(logical_c_center) <= max_render_radius) {
				r_chunks[Vector2i(cx, cz)] = true;
			}
		}
	}
}

/// Editor: generate now (immediate feedback is worth the stall). Game: enqueue for streaming.
void TerrainSplineCompositor::_run_rebuild(
		const std::vector<Vector2i> &p_chunks,
		const std::vector<ProceduralSpline3D *> &p_splines,
		Object *p_target_api
) {
	if (p_chunks.empty()) {
		return;
	}
	if (Engine::get_singleton()->is_editor_hint()) {
		_generate_chunks(p_chunks, p_splines, p_target_api);
		_flush_terrain_maps(p_target_api);
		_refresh_terrain_collision();
	} else {
		_enqueue_chunks(p_chunks, /*allow_existing=*/true);
	}
}

} // namespace godot
