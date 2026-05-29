#include "terraspline.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/world3d.hpp>

namespace godot {

// =========================================================
// TerrainChunk Implementation
// =========================================================

/**
 * @brief Binds TerrainChunk methods to Godot's ClassDB (currently empty).
 */
void TerrainChunk::_bind_methods() {}

/**
 * @brief Default Constructor. Initializes default chunk state (UNLOADED) and default physics body RID.
 */
TerrainChunk::TerrainChunk() {
	current_state = STATE_UNLOADED;
	physics_body_rid = RID();
}

/**
 * @brief Default Destructor. Safely frees all visual nodes and physics bodies associated with this chunk.
 */
TerrainChunk::~TerrainChunk() {
	// Clean up visuals safely using ObjectDB to prevent use-after-free
	for (uint64_t id : visual_nodes) {
		Object *obj = ObjectDB::get_instance(id);
		if (obj) {
			MultiMeshInstance3D *node = Object::cast_to<MultiMeshInstance3D>(obj);
			if (node) {
				if (node->is_inside_tree()) {
					node->queue_free();
				} else {
					memdelete(node);
				}
			}
		}
	}
	visual_nodes.clear();

	// Clean up physics
	if (physics_body_rid.is_valid()) {
		PhysicsServer3D::get_singleton()->free_rid(physics_body_rid);
		physics_body_rid = RID();
	}
}

// =========================================================
// TerrainHeightmap Implementation
// =========================================================

/**
 * @brief Binds TerrainHeightmap methods to Godot's ClassDB.
 * Exposes heightmap initialization, clear, dimension queries, and image conversion.
 */
void TerrainHeightmap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize", "width", "height", "default_value"), &TerrainHeightmap::initialize, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("clear", "default_value"), &TerrainHeightmap::clear, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("get_width"), &TerrainHeightmap::get_width);
	ClassDB::bind_method(D_METHOD("get_height"), &TerrainHeightmap::get_height);
	ClassDB::bind_method(D_METHOD("get_image"), &TerrainHeightmap::get_image);
}

/**
 * @brief Default Constructor.
 */
TerrainHeightmap::TerrainHeightmap() {}

/**
 * @brief Default Destructor.
 */
TerrainHeightmap::~TerrainHeightmap() {}

/**
 * @brief Allocates and initializes memory size for the heightmap float array.
 */
void TerrainHeightmap::initialize(int p_width, int p_height, float p_default_value) {
	width = p_width;
	height = p_height;
	data.resize(width * height);
	clear(p_default_value);
}

/**
 * @brief Clears/resets all heights in the heightmap to the specified default elevation.
 */
void TerrainHeightmap::clear(float p_default_value) {
	int sz = width * height;
	float *ptr = data.ptrw();
	for (int i = 0; i < sz; ++i) {
		ptr[i] = p_default_value;
	}
}

/**
 * @brief Converts the raw float data into a Godot Image object.
 * Re-packs the float array into a byte format appropriate for Terrain3D GPU import (Image::FORMAT_RF).
 */
Ref<Image> TerrainHeightmap::get_image() const {
	uint64_t t_start = Time::get_singleton()->get_ticks_usec();

	Ref<Image> img = Image::create_empty(width, height, false, Image::FORMAT_RF);
	if (width > 0 && height > 0) {
		PackedByteArray byte_data;
		byte_data.resize(data.size() * sizeof(float));
		memcpy(byte_data.ptrw(), data.ptr(), byte_data.size());
		img->set_data(width, height, false, Image::FORMAT_RF, byte_data);
	}

#if DEBUG
	uint64_t t_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("    [TerrainHeightmap] get_image() created in: ", (t_end - t_start) / 1000.0, " ms.");
#endif
	return img;
}

// =========================================================
// TerrainSplineCompositor Chunk Management Methods
// =========================================================

/**
 * @brief Periodic cleanup system. Evicts chunks and Terrain3D GPU regions that are outside the player's render radius.
 * Periodically removes out-of-range chunks from compositor memory and unloads them from Terrain3D VRAM.
 */
void TerrainSplineCompositor::_check_and_evict_far_chunks() {
	if (!terrain) {
		return;
	}

	Variant api_var = terrain->get("data");
	if (api_var.get_type() == Variant::NIL || (api_var.get_type() == Variant::OBJECT && (Object *)api_var == nullptr)) {
		api_var = terrain->get("storage");
	}
	Object *target_api = (api_var.get_type() == Variant::OBJECT) ? (Object *)api_var : nullptr;
	if (!target_api) {
		return;
	}

	// 1. Get player position
	Vector3 target_pos = terrain->call("get_collision_target_position");
	Vector2 player_pos_2d(target_pos.x, target_pos.z);
	Vector2 logical_player_pos_2d = player_pos_2d + global_world_offset;

	// 2. Erase far-away chunks from compositor's chunk_buffers
	std::vector<Vector2i> chunks_to_erase;
	for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
		Vector2i cpos = E.key;
		Vector2 c_center((cpos.x + 0.5f) * chunk_size, (cpos.y + 0.5f) * chunk_size);
		Vector2 logical_c_center = c_center + global_world_offset;
		if (logical_player_pos_2d.distance_to(logical_c_center) > max_render_radius) {
			chunks_to_erase.push_back(cpos);
		}
	}
	for (Vector2i cpos : chunks_to_erase) {
		chunk_buffers.erase(cpos);
	}

	// 3. Find and evict far-away regions from Terrain3D
	TypedArray<Vector2i> region_locations = target_api->call("get_region_locations");
	int region_size = terrain->call("get_region_size");
	float vertex_spacing = terrain->call("get_vertex_spacing");
	float region_world_size = region_size * vertex_spacing;

	for (int i = 0; i < region_locations.size(); ++i) {
		Vector2i rloc = region_locations[i];
		Vector2 r_center((rloc.x + 0.5f) * region_world_size, (rloc.y + 0.5f) * region_world_size);
		Vector2 logical_r_center = r_center + global_world_offset;

		// If region center is beyond player's max render radius (with extra margin for boundary safety)
		if (logical_player_pos_2d.distance_to(logical_r_center) > max_render_radius + region_world_size * 0.5f) {
			// Ensure no active compositor chunk is inside this region
			bool region_is_used = false;
			for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
				Vector2i cpos = E.key;
				Vector3 chunk_pos_3d(cpos.x * chunk_size, 0.0f, cpos.y * chunk_size);
				Vector2i chunk_rloc = target_api->call("get_region_location", chunk_pos_3d);
				if (chunk_rloc == rloc) {
					region_is_used = true;
					break;
				}
			}
			if (!region_is_used) {
#if DEBUG
				UtilityFunctions::print("[Compositor] Evicting far region from GPU VRAM: ", rloc);
#endif
				target_api->call("remove_regionl", rloc, true);
			}
		}
	}

	// 4. Generate new chunks that have entered the render radius
	TypedArray<Node> children = get_children();
	std::vector<ProceduralSpline3D *> splines;
	for (int i = 0; i < children.size(); ++i) {
		ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(children[i]);
		if (spline) {
			splines.push_back(spline);
		}
	}

	int min_cx = (int)Math::floor((logical_player_pos_2d.x - max_render_radius) / chunk_size);
	int max_cx = (int)Math::floor((logical_player_pos_2d.x + max_render_radius) / chunk_size);
	int min_cz = (int)Math::floor((logical_player_pos_2d.y - max_render_radius) / chunk_size);
	int max_cz = (int)Math::floor((logical_player_pos_2d.y + max_render_radius) / chunk_size);

	std::vector<Vector2i> chunks_to_generate_physical;
	for (int cx = min_cx; cx <= max_cx; ++cx) {
		for (int cz = min_cz; cz <= max_cz; ++cz) {
			Vector2i logical_cpos(cx, cz);
			Vector2i physical_cpos = logical_cpos - Vector2i(global_world_offset.x / chunk_size, global_world_offset.y / chunk_size);

			if (chunk_buffers.has(physical_cpos)) {
				continue; // Already active/generated
			}

			Vector2 logical_c_center((cx + 0.5f) * chunk_size, (cz + 0.5f) * chunk_size);
			if (logical_player_pos_2d.distance_to(logical_c_center) > max_render_radius) {
				continue; // Outside range
			}

			Vector2 offset(physical_cpos.x * chunk_size, physical_cpos.y * chunk_size);
			Rect2 chunk_rect(offset, Vector2(chunk_size, chunk_size));

			bool has_splines = false;
			for (ProceduralSpline3D *spline : splines) {
				if (spline->get_padded_aabb().intersects(chunk_rect)) {
					has_splines = true;
					break;
				}
			}

			if (has_splines) {
				chunks_to_generate_physical.push_back(physical_cpos);
			}
		}
	}

	if (!chunks_to_generate_physical.empty()) {
#if DEBUG
		UtilityFunctions::print("[Compositor] Dynamic generation of ", (int)chunks_to_generate_physical.size(), " new chunks inside render radius...");
#endif
		_generate_chunks(chunks_to_generate_physical, splines, target_api);
	}
}

/**
 * @brief Registers/unregisters chunk colliders from Godot's PhysicsServer3D depending on state.
 * Spawns static bodies and inserts cached shape transforms when waking up. Removes bodies when sleeping.
 */
void TerrainSplineCompositor::_update_chunk_physics(const Ref<TerrainChunk> &p_chunk) {
	if (!terrain) {
		return;
	}
	Node3D *terrain_3d = Object::cast_to<Node3D>(terrain);
	if (!terrain_3d) {
		return;
	}
	RID space_rid = terrain_3d->get_world_3d()->get_space();

	// Wake up physics
	if (p_chunk->get_state() == TerrainChunk::STATE_VISUAL_ONLY) {
		if (p_chunk->get_physics_caches().empty()) {
			p_chunk->set_state(TerrainChunk::STATE_VISUAL_AND_PHYSICS);
			return;
		}

		RID body_rid = PhysicsServer3D::get_singleton()->body_create();
		PhysicsServer3D::get_singleton()->body_set_mode(body_rid, PhysicsServer3D::BODY_MODE_STATIC);
		PhysicsServer3D::get_singleton()->body_set_space(body_rid, space_rid);

		for (const TerrainChunk::PhysicsCache &pc : p_chunk->get_physics_caches()) {
			for (const Transform3D &t : pc.transforms) {
				PhysicsServer3D::get_singleton()->body_add_shape(body_rid, pc.shape_rid, t);
			}
		}

		p_chunk->set_physics_body_rid(body_rid);
		p_chunk->set_state(TerrainChunk::STATE_VISUAL_AND_PHYSICS);
	}
	// Sleep physics
	else if (p_chunk->get_state() == TerrainChunk::STATE_VISUAL_AND_PHYSICS) {
		RID body_rid = p_chunk->get_physics_body_rid();
		if (body_rid.is_valid()) {
			PhysicsServer3D::get_singleton()->free_rid(body_rid);
			p_chunk->set_physics_body_rid(RID());
		}
		p_chunk->set_state(TerrainChunk::STATE_VISUAL_ONLY);
	}
}

/**
 * @brief Performs proximity checks to determine which chunks should have active physics collision.
 * Walks all active chunks; wakes up physics for those inside max_physics_radius, sleeps those outside.
 */
void TerrainSplineCompositor::_check_chunk_physics_culling() {
	if (!terrain) {
		return;
	}

	Vector3 target_pos = terrain->call("get_collision_target_position");
	Vector2 player_pos_2d(target_pos.x, target_pos.z);
	Vector2 logical_player_pos_2d = player_pos_2d + global_world_offset;

	int active_physics_count = 0;
	int visual_only_count = 0;

	for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
		Ref<TerrainChunk> chunk = E.value;
		if (chunk.is_null()) {
			continue;
		}

		Vector2i cpos = chunk->get_chunk_coords();
		Vector2 c_center((cpos.x + 0.5f) * chunk_size, (cpos.y + 0.5f) * chunk_size);
		Vector2 logical_c_center = c_center + global_world_offset;

		float dist = logical_player_pos_2d.distance_to(logical_c_center);

		// If inside physics radius and not yet awake, wake it up!
		if (dist <= max_physics_radius) {
			if (chunk->get_state() == TerrainChunk::STATE_VISUAL_ONLY) {
				_update_chunk_physics(chunk);
			}
		} else {
			// Outside physics radius and awake, sleep it!
			if (chunk->get_state() == TerrainChunk::STATE_VISUAL_AND_PHYSICS) {
				_update_chunk_physics(chunk);
			}
		}

		if (chunk->get_state() == TerrainChunk::STATE_VISUAL_AND_PHYSICS) {
			active_physics_count++;
		} else if (chunk->get_state() == TerrainChunk::STATE_VISUAL_ONLY) {
			visual_only_count++;
		}
	}
}

} // namespace godot
