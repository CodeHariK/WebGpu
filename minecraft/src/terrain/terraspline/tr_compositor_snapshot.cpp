/**
 * @file tr_compositor_snapshot.cpp
 * @brief Read-only streaming state for the debug map: resident/queued/in-flight chunks, recent
 * evictions, radii, player and camera headings. Everything is reported in logical world metres.
 */
#include "../../camera/camera.h"
#include "../../game_manager/game_manager.h"
#include "tr_compositor.h"
#include "tr_deformer.h"
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>

namespace godot {

/// XZ heading of a Node3D (its -Z axis), or zero when unavailable.
static Vector2 heading_xz(const Node3D *p_node) {
	if (!p_node || !p_node->is_inside_tree()) {
		return Vector2();
	}
	Vector3 f = -p_node->get_global_transform().basis.get_column(2);
	Vector2 h(f.x, f.z);
	return h.length_squared() > 1e-8f ? h.normalized() : Vector2();
}

Rect2 TerrainSplineCompositor::_logical_chunk_rect(const Vector2i &p_physical_chunk) const {
	Vector2 origin = Vector2(p_physical_chunk.x * chunk_size, p_physical_chunk.y * chunk_size) + global_world_offset;
	return Rect2(origin, Vector2(chunk_size, chunk_size));
}

void TerrainSplineCompositor::_log_eviction(const Vector2i &p_physical_chunk) {
	StreamSnapshot::Evicted e;
	e.rect = _logical_chunk_rect(p_physical_chunk);
	e.time_msec = Time::get_singleton()->get_ticks_msec();
	_evicted_recent.push_back(e);
	if (_evicted_recent.size() > EVICTED_LOG_MAX) {
		_evicted_recent.erase(_evicted_recent.begin());
	}
}

void TerrainSplineCompositor::set_thumbnail_size(int p_size) {
	_thumbnail_size = MAX(0, p_size);
	// Backfill resident chunks so the map is complete immediately, not only after the next stream-in.
	for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
		if (E.value.is_valid()) {
			E.value->build_thumbnail(_thumbnail_size);
		}
	}
}

void TerrainSplineCompositor::get_stream_snapshot(StreamSnapshot &r_out) const {
	r_out.chunk_size = chunk_size;
	r_out.thumbnail_size = _thumbnail_size;
	r_out.render_radius = max_render_radius;
	r_out.physics_radius = max_physics_radius;
	r_out.player = _logical_player_position_2d();
	r_out.player_forward = Vector2();
	r_out.camera_forward = Vector2();
	if (GameManager::get_singleton()) {
		r_out.player_forward = heading_xz(Object::cast_to<Node3D>(GameManager::get_singleton()->get_active_target()));
		r_out.camera_forward = heading_xz(GameManager::get_singleton()->get_camera());
	}
	if (r_out.camera_forward == Vector2() && is_inside_tree() && get_viewport()) {
		r_out.camera_forward = heading_xz(get_viewport()->get_camera_3d());
	}

	r_out.resident.clear();
	r_out.resident.reserve(chunk_buffers.size());
	for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
		if (E.value.is_null()) {
			continue;
		}
		StreamSnapshot::Chunk c;
		c.rect = _logical_chunk_rect(E.key);
		c.state = (int)E.value->get_state();
		c.thumbnail = E.value->get_thumbnail_size() == _thumbnail_size && _thumbnail_size > 0
				? &E.value->get_thumbnail()
				: nullptr;
		r_out.resident.push_back(c);
	}

	r_out.queued.clear();
	for (const Vector2i &c : _gen_queue) {
		r_out.queued.push_back(_logical_chunk_rect(c));
	}
	r_out.in_flight.clear();
	for (const Ref<ChunkJob> &job : _jobs_in_flight) {
		if (job.is_valid()) {
			r_out.in_flight.push_back(_logical_chunk_rect(job->chunk_pos));
		}
	}
	r_out.evicted = _evicted_recent;

	r_out.spline_bounds.clear();
	for (ProceduralSpline3D *spline : _gather_splines()) {
		Rect2 aabb = spline->get_padded_aabb();
		aabb.position += global_world_offset;
		r_out.spline_bounds.push_back(aabb);
	}
}

Ref<TerrainHeightmap> TerrainSplineCompositor::make_profile_context() const {
	Ref<TerrainHeightmap> hm;
	hm.instantiate();
	hm->initialize(1, 1, default_elevation);
	hm->set_base_terrain(global_terrain_noise, global_terrain_amplitude);
	std::vector<TerrainHeightmap::BaseDeformer> base;
	for (ProceduralSpline3D *spline : _gather_splines()) {
		spline->ensure_baked_cache();
		TypedArray<Node> children = spline->get_children();
		for (int i = 0; i < children.size(); ++i) {
			TerrainSplineDeformer *d = Object::cast_to<TerrainSplineDeformer>(children[i]);
			if (d && d->get_height_source() == TerrainSplineDeformer::HEIGHT_SPLINE) {
				base.push_back({ spline, d });
			}
		}
	}
	hm->set_base_deformers(base);
	return hm;
}

} // namespace godot
