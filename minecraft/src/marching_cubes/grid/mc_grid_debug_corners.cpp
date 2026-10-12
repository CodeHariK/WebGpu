// MCGrid: debug cubes on surface corners (red = solid, blue = empty) and their picking colliders.
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/util/mc_physics.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// Inspector property: show the debug corner cubes.
void MCGrid::set_show_debug_corners(bool p_show) {
	if (show_debug_corners == p_show) {
		return;
	}
	show_debug_corners = p_show;
	set_debug_corners_visible(show_debug_corners);
}

// Inspector property: debug corner cubes shown?
bool MCGrid::get_show_debug_corners() const { return show_debug_corners; }

// Show / hide the debug corner container.
void MCGrid::set_debug_corners_visible(bool p_visible) {
	if (debug_corners_container) {
		debug_corners_container->set_visible(p_visible);
	}
}

// Is the debug corner container visible?
bool MCGrid::is_debug_corners_visible() const {
	if (debug_corners_container) {
		return debug_corners_container->is_visible();
	}
	return false;
}

// Turn the picking colliders of every debug cube on or off.
void MCGrid::set_corner_collision_enabled(bool p_enabled) {
	if (!debug_corners_container) {
		return;
	}

	uint32_t layer = p_enabled ? toLayer(LAYER_CORNERS) : 0;

	TypedArray<Node> children = debug_corners_container->get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		if (!child)
			continue;

		// Corners are MeshInstances with StaticBody3D children
		TypedArray<Node> sub_children = child->get_children();
		for (int j = 0; j < sub_children.size(); j++) {
			StaticBody3D *sb = Object::cast_to<StaticBody3D>(sub_children[j]);
			if (sb && (sb->get_collision_layer() == toLayer(LAYER_CORNERS) || sb->get_collision_layer() == 0)) {
				sb->set_collision_layer(layer);
			}
		}
	}
}

// Create the debug cubes for the corners this chunk owns: every solid corner (red, with a picking
// collider when it touches an empty corner) and every empty corner next to a solid one (blue).
// Returns how many.
int MCGrid::_spawn_debug_corners(Chunk &p_chunk) {
	int nx = p_chunk.size_x + 1;
	int ny = p_chunk.size_y + 1;
	int nz = p_chunk.size_z + 1;
	int count = 0;

	if (_debug_box_mesh.is_null()) {
		_debug_box_mesh.instantiate();
		_debug_box_mesh->set_size(Vector3(0.1, 0.1, 0.1));
	}

	if (_debug_mat_red.is_null()) {
		_debug_mat_red.instantiate();
		_debug_mat_red->set_albedo(Color(1, 0, 0));
		_debug_mat_red->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	}

	if (_debug_mat_blue.is_null()) {
		_debug_mat_blue.instantiate();
		_debug_mat_blue->set_albedo(Color(0, 0, 1));
		_debug_mat_blue->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	}

	for (int ly = 0; ly < ny; ly++) {
		for (int lz = 0; lz < nz; lz++) {
			for (int lx = 0; lx < nx; lx++) {
				// A corner on a shared chunk face is stored in both chunks: spawn it only from its owner.
				if (!_dims().owns_corner(Vector3i(p_chunk.loc_x, p_chunk.loc_y, p_chunk.loc_z), Vector3i(lx, ly, lz))) {
					continue;
				}
				Vector3i corner(
						(p_chunk.loc_x * p_chunk.size_x) + lx, (p_chunk.loc_y * p_chunk.size_y) + ly,
						(p_chunk.loc_z * p_chunk.size_z) + lz
				);
				bool state = p_chunk.get_corner(lx, ly, lz);
				bool has_active = _has_neighbor_with_state(corner, true);
				bool has_inactive = _has_neighbor_with_state(corner, false);

				if (!state && !has_active) {
					continue;
				}

				Vector3 world_pos(corner);

				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(_debug_box_mesh);
				mi->set_position(world_pos);

				if (!debug_corners_container) {
					debug_corners_container = memnew(Node3D);
					debug_corners_container->set_name("DebugCorners");
					add_child(debug_corners_container);
				}
				debug_corners_container->add_child(mi);

				// Store for granular updates
				int corner_idx = (ly * nx * nz) + (lz * nx) + lx;
				p_chunk.debug_visuals[corner_idx] = mi;

				if (state) {
					mi->set_material_override(_debug_mat_red);

					if (has_inactive) {
						MCPhysics::create_static_box_collider(mi, toLayer(LAYER_CORNERS), Vector3(1.0, 1.0, 1.0));
					}
				} else {
					mi->set_material_override(_debug_mat_blue);
				}

				count++;
				total_debug_corners++;
			}
		}
	}

	return count;
}

// Recreate the debug cube of one global corner after an edit (or remove it).
void MCGrid::_update_debug_corner(
		int gx,
		int gy,
		int gz
) {
	// The owning chunk is the one _spawn_debug_corners used, so this updates the only copy.
	int chunk_index = 0;
	Vector3i local;
	if (!_dims().corner_to_local(Vector3i(gx, gy, gz), chunk_index, local)) {
		return; // outside the grid (e.g. a neighbour of an edge corner)
	}

	Chunk &chunk = chunks[static_cast<size_t>(chunk_index)];
	int nx = chunk.size_x + 1;
	int nz = chunk.size_z + 1;
	int corner_idx = (local.y * nx * nz) + (local.z * nx) + local.x;
	MeshInstance3D *mi = chunk.debug_visuals[corner_idx];
	bool state = chunk.get_corner(local.x, local.y, local.z);
	bool has_active = _has_neighbor_with_state(Vector3i(gx, gy, gz), true);
	bool has_inactive = _has_neighbor_with_state(Vector3i(gx, gy, gz), false);

	if (!state && !has_active) {
		if (mi) {
			mi->queue_free();
			chunk.debug_visuals[corner_idx] = nullptr;
			total_debug_corners--;
		}
		return;
	}

	if (!mi) {
		// Spawn since it's now needed
		mi = memnew(MeshInstance3D);
		mi->set_mesh(_debug_box_mesh);
		mi->set_position(Vector3(gx, gy, gz));

		if (!debug_corners_container) {
			debug_corners_container = memnew(Node3D);
			debug_corners_container->set_name("DebugCorners");
			add_child(debug_corners_container);
		}
		debug_corners_container->add_child(mi);
		chunk.debug_visuals[corner_idx] = mi;
		total_debug_corners++;
	} else {
		// Remove old collisions if any (only for active corners with inactive neighbors)
		TypedArray<Node> col_children = mi->get_children();
		for (int i = 0; i < col_children.size(); i++) {
			Node *child = Object::cast_to<Node>(col_children[i]);
			if (Object::cast_to<StaticBody3D>(child)) {
				child->queue_free();
			}
		}
	}

	if (state) {
		mi->set_material_override(_debug_mat_red);
		if (has_inactive) {
			MCPhysics::create_static_box_collider(mi, toLayer(LAYER_CORNERS), Vector3(1.0, 1.0, 1.0));
		}
	} else {
		mi->set_material_override(_debug_mat_blue);
	}
}

// Does any of the 6 neighbours (inside the grid, across chunks) have state p_state?
bool MCGrid::_has_neighbor_with_state(
		const Vector3i &p_corner,
		bool p_state
) const {
	static const Vector3i NEIGHBORS[6] = { Vector3i(-1, 0, 0), Vector3i(1, 0, 0),  Vector3i(0, -1, 0),
										   Vector3i(0, 1, 0),  Vector3i(0, 0, -1), Vector3i(0, 0, 1) };
	for (const Vector3i &step : NEIGHBORS) {
		Vector3i n = p_corner + step;
		if (_dims().contains_corner(n) && is_corner_active(n) == p_state) {
			return true;
		}
	}
	return false;
}

} // namespace godot
