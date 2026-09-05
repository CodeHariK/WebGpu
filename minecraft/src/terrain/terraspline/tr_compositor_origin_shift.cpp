/**
 * @file tr_compositor_origin_shift.cpp
 * @brief Floating origin: when the player passes +-4096 m on X or Z, the whole world is moved back by
 * 4096 m so single-precision floats stay accurate. `global_world_offset` records the accumulated
 * shift, so logical (design) coordinates = physical (scene) coordinates + offset.
 */
#include "tr_compositor.h"
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static constexpr float ORIGIN_SHIFT_THRESHOLD = 4096.0f;

void TerrainSplineCompositor::_check_origin_shift() {
	if (!terrain) {
		return;
	}
	Vector3 shift;
	if (_compute_origin_shift(_get_player_position(), shift)) {
		_apply_origin_shift(shift);
	}
}

/// Returns true and the (axis-aligned, +-4096 per axis) shift if the player is past the threshold.
bool TerrainSplineCompositor::_compute_origin_shift(
		const Vector3 &p_target_pos,
		Vector3 &r_shift
) const {
	if (Math::abs(p_target_pos.x) <= ORIGIN_SHIFT_THRESHOLD && Math::abs(p_target_pos.z) <= ORIGIN_SHIFT_THRESHOLD) {
		return false;
	}
	r_shift = Vector3(0.0f, 0.0f, 0.0f);
	if (p_target_pos.x > ORIGIN_SHIFT_THRESHOLD) {
		r_shift.x = ORIGIN_SHIFT_THRESHOLD;
	} else if (p_target_pos.x < -ORIGIN_SHIFT_THRESHOLD) {
		r_shift.x = -ORIGIN_SHIFT_THRESHOLD;
	}
	if (p_target_pos.z > ORIGIN_SHIFT_THRESHOLD) {
		r_shift.z = ORIGIN_SHIFT_THRESHOLD;
	} else if (p_target_pos.z < -ORIGIN_SHIFT_THRESHOLD) {
		r_shift.z = -ORIGIN_SHIFT_THRESHOLD;
	}
	return true;
}

/// Moves every physical thing by -shift and records +shift in global_world_offset.
void TerrainSplineCompositor::_apply_origin_shift(const Vector3 &p_shift) {
#if DEBUG
	UtilityFunctions::print("[Compositor] Origin shift triggered! Shifting world by: ", p_shift);
#endif
	_shift_terrain_targets(p_shift);
	global_world_offset += Vector2(p_shift.x, p_shift.z);
	_shift_spline_nodes(p_shift);

	Node3D *terrain_3d = Object::cast_to<Node3D>(terrain);
	if (terrain_3d) {
		terrain_3d->set_global_position(terrain_3d->get_global_position() - p_shift);
	}
	// The scattered MultiMeshInstance3Ds live under scatter_container in world space; shift the
	// container so visuals stay aligned with the shifted physics caches.
	if (scatter_container) {
		scatter_container->set_global_position(scatter_container->get_global_position() - p_shift);
	}

	_shift_chunk_buffers(p_shift);

	// Reset Terrain3D's target snapshot so the clipmap doesn't lag a frame behind.
	terrain->call("snap");
}

/// Moves the nodes Terrain3D follows (collision target, clipmap target, camera), each once.
void TerrainSplineCompositor::_shift_terrain_targets(const Vector3 &p_shift) {
	Node3D *col_target = Object::cast_to<Node3D>(terrain->call("get_collision_target"));
	if (col_target) {
		col_target->set_global_position(col_target->get_global_position() - p_shift);
	}
	Node3D *clip_target = Object::cast_to<Node3D>(terrain->call("get_clipmap_target"));
	if (clip_target && clip_target != col_target) {
		clip_target->set_global_position(clip_target->get_global_position() - p_shift);
	}
	Node3D *cam_target = Object::cast_to<Node3D>(terrain->call("get_camera"));
	if (cam_target && cam_target != col_target && cam_target != clip_target) {
		cam_target->set_global_position(cam_target->get_global_position() - p_shift);
	}
}

void TerrainSplineCompositor::_shift_spline_nodes(const Vector3 &p_shift) {
	for (ProceduralSpline3D *spline : _gather_splines()) {
		spline->set_global_position(spline->get_global_position() - p_shift);
	}
}

/// Re-keys chunk_buffers by whole chunks and moves cached and live physics transforms.
void TerrainSplineCompositor::_shift_chunk_buffers(const Vector3 &p_shift) {
	Vector2i chunk_shift(p_shift.x / chunk_size, p_shift.z / chunk_size);
	if (chunk_shift != Vector2i(0, 0)) {
		HashMap<Vector2i, Ref<TerrainChunk>> new_chunk_buffers;
		for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
			new_chunk_buffers[E.key - chunk_shift] = E.value;
		}
		chunk_buffers = new_chunk_buffers;
	}

	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
	for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
		if (E.value.is_null()) {
			continue;
		}
		for (TerrainChunk::PhysicsCache &pc : E.value->get_physics_caches()) {
			for (Transform3D &t : pc.transforms) {
				t.origin -= p_shift;
			}
		}
		RID body_rid = E.value->get_physics_body_rid();
		if (body_rid.is_valid()) {
			Transform3D body_transform = ps->body_get_state(body_rid, PhysicsServer3D::BODY_STATE_TRANSFORM);
			body_transform.origin -= p_shift;
			ps->body_set_state(body_rid, PhysicsServer3D::BODY_STATE_TRANSFORM, body_transform);
		}
	}
}

} // namespace godot
