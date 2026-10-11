// MCGrid: one tile mesh per non-empty cell (spawn all, update one after an edit, clear).
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/library/mc_library.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

int MCGrid::_spawn_marching_cubes(
		const Chunk &p_chunk,
		MCNode *p_mc_node
) {
	int count = 0;

	for (int ly = 0; ly < p_chunk.size_y; ly++) {
		for (int lz = 0; lz < p_chunk.size_z; lz++) {
			for (int lx = 0; lx < p_chunk.size_x; lx++) {
				uint8_t hash = p_chunk.get_cell_hash(lx, ly, lz);
				total_cells++;

				if (hash == 0) {
					continue;
				}

				MeshConfig conf = p_mc_node->get_mesh_config(hash);
				if (conf.mesh.is_null()) {
					continue;
				}

				// Spawn at center of the 8 corners (cell center)
				Vector3 world_pos =
						Vector3(static_cast<float>((p_chunk.loc_x * p_chunk.size_x) + lx) + 0.5f,
								static_cast<float>((p_chunk.loc_y * p_chunk.size_y) + ly) + 0.5f,
								static_cast<float>((p_chunk.loc_z * p_chunk.size_z) + lz) + 0.5f);

				Vector3i cell(
						(p_chunk.loc_x * p_chunk.size_x) + lx, (p_chunk.loc_y * p_chunk.size_y) + ly,
						(p_chunk.loc_z * p_chunk.size_z) + lz
				);
				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(conf.mesh_for_cell(cell));
				Transform3D cell_t;
				cell_t.origin = world_pos;
				mi->set_transform(cell_t * conf.transform);
				mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
				add_child(mi);

				// Store the pointer for granular updates
				int cell_idx = (ly * p_chunk.size_x * p_chunk.size_z) + (lz * p_chunk.size_x) + lx;
				const_cast<Chunk &>(p_chunk).cell_visuals[cell_idx] = mi;

				count++;
				total_mc_meshes++;
			}
		}
	}
	return count;
}

void MCGrid::_clear_children() {
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
		std::fill(chunk.cell_visuals.begin(), chunk.cell_visuals.end(), nullptr);
		std::fill(chunk.debug_visuals.begin(), chunk.debug_visuals.end(), nullptr);
	}

	total_mc_meshes = 0;
	total_debug_corners = 0;
	total_cells = 0;
}

void MCGrid::_update_visual_at(
		int gx,
		int gy,
		int gz
) {
	int cx = (gx < 0) ? -1 : gx / chunk_size.x;
	int cy = (gy < 0) ? -1 : gy / chunk_size.y;
	int cz = (gz < 0) ? -1 : gz / chunk_size.z;

	if (cx < 0 || cx >= grid_size.x || cy < 0 || cy >= grid_size.y || cz < 0 || cz >= grid_size.z) {
		return;
	}

	int lx = gx - (cx * chunk_size.x);
	int ly = gy - (cy * chunk_size.y);
	int lz = gz - (cz * chunk_size.z);

	Chunk &chunk = chunks[_get_chunk_index(cx, cy, cz)];
	int cell_idx = (ly * chunk.size_x * chunk.size_z) + (lz * chunk.size_x) + lx;

	// Remove old visual if it exists
	if (chunk.cell_visuals[cell_idx]) {
		chunk.cell_visuals[cell_idx]->queue_free();
		chunk.cell_visuals[cell_idx] = nullptr;
		total_mc_meshes--;
	}

	uint8_t hash = chunk.get_cell_hash(lx, ly, lz);
	if (hash == 0 || !mc_node) {
		return;
	}

	MeshConfig conf = mc_node->get_mesh_config(hash);
	if (conf.mesh.is_null()) {
		return;
	}

	Vector3 world_pos =
			Vector3(static_cast<float>(gx) + 0.5f, static_cast<float>(gy) + 0.5f, static_cast<float>(gz) + 0.5f);

	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(conf.mesh_for_cell(Vector3i(gx, gy, gz)));
	Transform3D cell_t;
	cell_t.origin = world_pos;
	mi->set_transform(cell_t * conf.transform);
	mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(mi);

	chunk.cell_visuals[cell_idx] = mi;
	total_mc_meshes++;
}

} // namespace godot
