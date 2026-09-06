/**
 * @file tr_compositor_terrain3d.cpp
 * @brief Every call the compositor makes into the Terrain3D GDExtension, in one place.
 *
 * Terrain3D is reached through Object::call/get so the compositor does not link against it.
 * Verified against Terrain3D 1.0.2:
 *  - Terrain3D.data                     Terrain3DData
 *  - Terrain3DData.add_region(r, update) update=false defers the GPU rebuild
 *  - Terrain3DData.update_maps()        rebuilds the texture arrays from every region (~0.35 ms/region)
 *  - Terrain3D.collision.update(true)   re-reads heights into the collision shapes
 */
#include "tr_compositor.h"
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

/// Terrain3DData (the "data" property; older builds called it "storage"), or null.
Object *TerrainSplineCompositor::_get_terrain_data_api() const {
	if (!terrain) {
		return nullptr;
	}
	Variant api_var = terrain->get("data");
	if (api_var.get_type() == Variant::NIL || (api_var.get_type() == Variant::OBJECT && (Object *)api_var == nullptr)) {
		api_var = terrain->get("storage");
	}
	return (api_var.get_type() == Variant::OBJECT) ? (Object *)api_var : nullptr;
}

/**
 * @brief Hands a finished chunk heightmap (and its control map, when a painter produced one) to Terrain3D.
 * Fast path (chunk size == region size): build the Terrain3DRegion ourselves and add it with
 * update=false; the GPU texture arrays are rebuilt once per batch by _flush_terrain_maps(). This is
 * what import_images does internally, minus its per-call update_maps.
 * Generic path (sizes differ): let import_images slice and upload per call.
 */
void TerrainSplineCompositor::_write_chunk_heights_to_terrain(
		Object *p_target_api,
		const Ref<TerrainChunk> &p_chunk,
		const Vector2 &p_offset,
		const Ref<Image> &p_control
) {
	Ref<Image> height_image = p_chunk->get_heightmap()->get_image();
	Vector3 stamp_position(p_offset.x, 0.0f, p_offset.y);
	int rsize = terrain ? (int)terrain->call("get_region_size") : 0;

	if (rsize == chunk_size) {
		Ref<RefCounted> region = ClassDB::instantiate("Terrain3DRegion");
		if (region.is_valid()) {
			region->set("region_size", rsize);
			region->set("location", p_target_api->call("get_region_location", stamp_position));
			TypedArray<Image> maps;
			maps.resize(3); // height, control, color; nulls are sanitized to defaults by add_region
			maps[0] = height_image;
			if (p_control.is_valid()) {
				maps[1] = p_control;
			}
			region->call("set_maps", maps);
			p_target_api->call("add_region", region, false);
			_terrain_maps_dirty = true;
		}
		return;
	}

	Ref<Image> empty_map;
	empty_map.instantiate();
	Array images;
	images.push_back(height_image);
	images.push_back(p_control.is_valid() ? p_control : empty_map);
	images.push_back(empty_map);
	bool has_region = p_target_api->call("has_regionp", stamp_position);
	if (!has_region) {
		Ref<RefCounted> new_region = ClassDB::instantiate("Terrain3DRegion");
		if (new_region.is_valid()) {
			new_region->set("region_size", rsize > 0 ? rsize : 1024);
			new_region->set("location", p_target_api->call("get_region_location", stamp_position));
			p_target_api->call("add_region", new_region);
		}
	}
	p_target_api->call("import_images", images, stamp_position, 0.0f, 1.0f);
}

/// Uploads all regions added with update=false since the last flush (one GPU rebuild per batch).
void TerrainSplineCompositor::_flush_terrain_maps(Object *p_target_api) {
	if (!_terrain_maps_dirty || !p_target_api) {
		return;
	}
	uint64_t t0 = Time::get_singleton()->get_ticks_usec();
	p_target_api->call("update_maps"); // TYPE_MAX, all_regions=true, generate_mipmaps=false
	_terrain_maps_dirty = false;
	_frames_since_flush = 0;
	if (_bench) {
		_bench_flush_ms += (Time::get_singleton()->get_ticks_usec() - t0) / 1000.0;
		_bench_flushes++;
	}
}

/**
 * @brief Forces Terrain3D to re-read heights into its collision shapes.
 * Terrain3D rebuilds collision on `region_map_changed` (i.e. when add_region runs), which is BEFORE
 * any heights are written. In dynamic mode it then only re-reads once the collision target has moved
 * >= shape_size. Call this after a batch of chunk writes.
 */
void TerrainSplineCompositor::_refresh_terrain_collision() {
	if (!terrain) {
		return;
	}
	Variant col_var = terrain->get("collision");
	Object *collision = (col_var.get_type() == Variant::OBJECT) ? (Object *)col_var : nullptr;
	if (collision && collision->has_method("update")) {
		uint64_t t0 = Time::get_singleton()->get_ticks_usec();
		collision->call("update", true);
		if (_bench) {
			_bench_collision_ms += (Time::get_singleton()->get_ticks_usec() - t0) / 1000.0;
		}
	}
}

} // namespace godot
