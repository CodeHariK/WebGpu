// MCGrid: the tiles of each chunk merged into one mesh + one collider (build, rebuild after an
// edit, clear).
//
// One MeshInstance3D per chunk instead of one per cell: far fewer nodes and render objects, and
// one StaticBody3D per chunk on LAYER_TERRAIN so players, enemies and projectiles can stand on /
// hit the terrain (its shapes: mc_grid_collision.cpp). Tiles that share a material end up in the
// same surface, so a chunk is one draw per material (MCChunkBuilder). Edits mark chunks dirty; they
// are rebuilt once at the end of the frame (flush_dirty_chunks), so a brush stroke rebuilds each
// chunk once.
#include "marching_cubes/terrain/2_mesh/mc_chunk_builder.h"
#include "marching_cubes/terrain/mc_grid.h"
#include "marching_cubes/library/mc_library.h"
#include "marching_cubes/util/mc_physics.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <vector>

namespace godot {

// Queue the chunk's mesh node (and collider) for freeing and reset its counters.
void MCGrid::_free_chunk_mesh(Chunk &p_chunk) {
	if (p_chunk.mesh_node) {
		// out of the tree now (its collider leaves the physics world, its name is free for the new
		// node), freed at the end of the frame; its StaticBody3D child goes with it
		if (p_chunk.mesh_node->get_parent() == this) {
			remove_child(p_chunk.mesh_node);
		}
		p_chunk.mesh_node->queue_free();
		p_chunk.mesh_node = nullptr;
		total_chunk_meshes--;
	}
	total_tiles_drawn -= p_chunk.tile_count;
	p_chunk.tile_count = 0;
	p_chunk.dirty = false;
}

// Replace the chunk's mesh: every tile placed in grid space and merged per material, plus one
// trimesh collider on LAYER_TERRAIN. Nothing is created for an empty chunk.
void MCGrid::_build_chunk_mesh(Chunk &p_chunk) {
	uint64_t t_start = Time::get_singleton()->get_ticks_usec();
	_free_chunk_mesh(p_chunk);
	if (!tile_library) {
		return;
	}

	// 1. Every tile, already placed in grid space, appended by material.
	MCChunkBuilder builder;
	std::vector<std::pair<Ref<Mesh>, Transform3D>> collide_tiles; // each tile placed (not decor): for the collider
	int tiles = 0;
	Vector3i origin(p_chunk.loc_x * p_chunk.size_x, p_chunk.loc_y * p_chunk.size_y, p_chunk.loc_z * p_chunk.size_z);
	const Vector3i pinned_cell = _pinned_cell(); // once per chunk: it reads the pinned cell's hash
	for (int ly = 0; ly < p_chunk.size_y; ly++) {
		for (int lz = 0; lz < p_chunk.size_z; lz++) {
			for (int lx = 0; lx < p_chunk.size_x; lx++) {
				uint8_t hash = _drawn_hash(p_chunk.get_cell_hash(lx, ly, lz)); // squares mode: a straight piece
				if (hash == 0) {
					continue;
				}
				Vector3i cell = origin + Vector3i(lx, ly, lz);
				Transform3D cell_t;
				cell_t.origin = Vector3(cell) + _tile_offset(); // cell centre (squares mode: half a cell down)
				if (cell == pinned_cell) {
					builder.add(pin_mesh, cell_t, surface_cache); // the pinned tile itself, as modelled
					collide_tiles.push_back({ pin_mesh, cell_t });
					// its sockets: those of the variant whose mesh it is
					const MCTile &pinned = tile_library->get_level_tile(mc::LEVEL_LETTERS[pin.level], hash);
					for (size_t v = 0; v < pinned.variant_meshes.size() && v < pinned.variant_sockets.size(); v++) {
						if (pinned.variant_meshes[v] != pin_mesh) {
							continue;
						}
						for (size_t i = 0; i < pinned.variant_sockets[v].size(); i++) {
							const MCSocket &socket = pinned.variant_sockets[v][i];
							Ref<Mesh> piece = tile_library->decor_for_socket(socket, cell, static_cast<int>(i), static_cast<uint32_t>(decor_seed));
							if (piece.is_valid()) {
								builder.add(piece, cell_t * socket.transform, surface_cache, false);
							}
						}
					}
					tiles++;
					continue;
				}
				// The tile of the cell's group and level (L / M / H; M when that level has none for the
				// hash; group a when the group has neither).
				const MCTile *conf =
						&tile_library->get_group_tile(_group_at(cell), mc::LEVEL_LETTERS[_level_at(cell)], hash);
				if (conf->mesh.is_null()) {
					continue;
				}
				const Ref<Mesh> mesh = conf->mesh_for_cell(cell);
				const Transform3D tile_t = cell_t * conf->turn;
				builder.add(mesh, tile_t, surface_cache);
				collide_tiles.push_back({ mesh, tile_t });
				const std::vector<MCSocket> &sockets = conf->sockets_for_cell(cell);
				for (size_t i = 0; i < sockets.size(); i++) { // decor: drawn, not collided
					Ref<Mesh> piece = tile_library->decor_for_socket(sockets[i], cell, static_cast<int>(i), static_cast<uint32_t>(decor_seed));
					if (piece.is_valid()) {
						builder.add(piece, tile_t * sockets[i].transform, surface_cache, false);
					}
				}
				tiles++;
			}
		}
	}
	if (tiles == 0 || builder.is_empty()) {
		return;
	}

	// 2. One surface per material.
	uint64_t t_placed = Time::get_singleton()->get_ticks_usec();
	Ref<ArrayMesh> merged = builder.commit();
	uint64_t t_merged = Time::get_singleton()->get_ticks_usec();

	// 3. The chunk node (grid space, identity transform) and its collider (mc_grid_collision.cpp).
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_name(vformat("Chunk_%d_%d_%d", p_chunk.loc_x, p_chunk.loc_y, p_chunk.loc_z));
	mi->set_mesh(merged);
	mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(mi);
	if (debug_wireframe || debug_seams) {
		_apply_wireframe(mi);
	}
	uint64_t t_node = Time::get_singleton()->get_ticks_usec();
	chunk_part_usec[0] += t_node - t_merged; // the mesh node into the scene
	chunk_part_usec[3] += static_cast<uint64_t>(builder.get_faces().size() / 3); // triangles
	// its collider, the way terrain_collision says (mc_grid_collision.cpp)
	_build_chunk_collider(mi, builder.get_faces(), collide_tiles);

	p_chunk.mesh_node = mi;
	uint64_t t_done = Time::get_singleton()->get_ticks_usec();
	chunk_stage_usec[0] += t_placed - t_start; // placing the tiles (and their decor) into the builder
	chunk_stage_usec[1] += t_merged - t_placed; // merging them into one mesh
	chunk_stage_usec[2] += t_done - t_merged; // the node and its collider
	chunk_stage_usec[3]++; // chunks rebuilt
	p_chunk.tile_count = tiles;
	total_tiles_drawn += tiles;
	total_chunk_meshes++;
}

// Mark the chunk that owns this cell for a rebuild (cells outside the grid are ignored).
void MCGrid::_mark_cell_dirty(
		int gx,
		int gy,
		int gz
) {
	int chunk_index = 0;
	Vector3i local;
	if (_dims().cell_to_local(Vector3i(gx, gy, gz), chunk_index, local)) {
		chunks[static_cast<size_t>(chunk_index)].dirty = true;
	}
}

// Schedule flush_dirty_chunks once for the end of the frame.
void MCGrid::_queue_rebuild() {
	if (!rebuild_queued) {
		rebuild_queued = true;
		call_deferred("flush_dirty_chunks");
	}
}

// Rebuild every chunk marked dirty since the last flush.
void MCGrid::flush_dirty_chunks() {
	rebuild_queued = false;
	_update_levels(); // an edit can change the level of cells further away (a roof region)
	bool rebuilt = false;
	for (Chunk &chunk : chunks) {
		if (chunk.dirty) {
			_build_chunk_mesh(chunk);
			rebuilt = true;
		}
	}
	if (rebuilt && debug_edge_corners) {
		_rebuild_edge_corners();
	}
	if (water_dirty) {
		_rebuild_water();
	}
}

// Free chunk meshes and debug cubes (keeps the containers, hover quads and placed objects)
// and reset the counters.
void MCGrid::_free_spawned_nodes() {
	TypedArray<Node> children = get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		if (child && child != debug_corners_container && child != hover_root && child != placed_objects_container &&
				child != water_node && child != water_debug_node && child != inspect_root &&
				child != edge_corners_root) {
			remove_child(child);
			child->queue_free();
		}
	}

	if (debug_corners_container) {
		TypedArray<Node> debug_children = debug_corners_container->get_children();
		for (int i = 0; i < debug_children.size(); i++) {
			Node *child = Object::cast_to<Node>(debug_children[i]);
			if (child) {
				debug_corners_container->remove_child(child);
				child->queue_free();
			}
		}
	}

	for (Chunk &chunk : chunks) {
		chunk.mesh_node = nullptr; // freed above with the other children
		chunk.tile_count = 0;
		chunk.dirty = false;
		std::fill(chunk.debug_visuals.begin(), chunk.debug_visuals.end(), nullptr);
	}

	total_tiles_drawn = 0;
	total_chunk_meshes = 0;
	total_debug_corners = 0;
	total_cells = 0;
}

} // namespace godot
