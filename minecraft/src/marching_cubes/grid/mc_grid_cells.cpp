// MCGrid: the tiles of each chunk merged into one mesh + one collider (build, rebuild after an
// edit, clear).
//
// One MeshInstance3D per chunk instead of one per cell: far fewer nodes and render objects, and
// one ConcavePolygonShape3D per chunk on LAYER_TERRAIN so players, enemies and projectiles can
// stand on / hit the terrain. Tiles that share a material end up in the same surface, so a chunk
// is one draw per material (MCChunkBuilder). Edits mark chunks dirty; they are rebuilt once at the
// end of the frame (flush_dirty_chunks), so a brush stroke rebuilds each chunk once.
#include "marching_cubes/grid/mc_chunk_builder.h"
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/library/mc_library.h"
#include "marching_cubes/util/mc_physics.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <vector>

namespace godot {

// Queue the chunk's mesh node (and collider) for freeing and reset its counters.
void MCGrid::_free_chunk_mesh(Chunk &p_chunk) {
	if (p_chunk.mesh_node) {
		p_chunk.mesh_node->queue_free(); // its StaticBody3D child goes with it
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
	_free_chunk_mesh(p_chunk);
	if (!tile_library) {
		return;
	}

	// 1. Every tile, already placed in grid space, appended by material.
	MCChunkBuilder builder;
	int tiles = 0;
	Vector3i origin(p_chunk.loc_x * p_chunk.size_x, p_chunk.loc_y * p_chunk.size_y, p_chunk.loc_z * p_chunk.size_z);
	for (int ly = 0; ly < p_chunk.size_y; ly++) {
		for (int lz = 0; lz < p_chunk.size_z; lz++) {
			for (int lx = 0; lx < p_chunk.size_x; lx++) {
				uint8_t hash = p_chunk.get_cell_hash(lx, ly, lz);
				if (hash == 0) {
					continue;
				}
				const MCTile &conf = tile_library->get_tile(hash);
				if (conf.mesh.is_null()) {
					continue;
				}
				Vector3i cell = origin + Vector3i(lx, ly, lz);
				Transform3D cell_t;
				cell_t.origin = Vector3(cell) + Vector3(0.5f, 0.5f, 0.5f); // cell centre
				builder.add(conf.mesh_for_cell(cell), cell_t * conf.turn, surface_cache);
				tiles++;
			}
		}
	}
	if (tiles == 0 || builder.is_empty()) {
		return;
	}

	// 2. One surface per material.
	Ref<ArrayMesh> merged = builder.commit();

	// 3. The chunk node (grid space, identity transform) and its trimesh collider.
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_name(vformat("Chunk_%d_%d_%d", p_chunk.loc_x, p_chunk.loc_y, p_chunk.loc_z));
	mi->set_mesh(merged);
	mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(mi);

	StaticBody3D *body = memnew(StaticBody3D);
	body->set_collision_layer(toLayer(LAYER_TERRAIN));
	body->set_collision_mask(0);
	CollisionShape3D *shape = memnew(CollisionShape3D);
	Ref<ConcavePolygonShape3D> trimesh;
	trimesh.instantiate();
	trimesh->set_faces(builder.get_faces()); // same triangles, no read-back from the mesh
	shape->set_shape(trimesh);
	body->add_child(shape);
	mi->add_child(body);

	p_chunk.mesh_node = mi;
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
	for (Chunk &chunk : chunks) {
		if (chunk.dirty) {
			_build_chunk_mesh(chunk);
		}
	}
}

// Free chunk meshes and debug cubes (keeps the containers, hover quads and placed objects)
// and reset the counters.
void MCGrid::_free_spawned_nodes() {
	TypedArray<Node> children = get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		if (child && child != debug_corners_container && child != hover_root && child != placed_objects_container) {
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
