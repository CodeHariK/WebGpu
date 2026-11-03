// MCManager: per-frame hover raycast, placement preview box, corner hover quads.
#include "marching_cubes/editor/mc_manager.h"
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/library/mc_library.h"
#include "marching_cubes/util/mc_physics.h"
#include "marching_cubes/util/mc_wire.h"
#include "utils/raycast/mc_raycast.h"

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// Show / hide the grid's debug corner cubes.
void MCManager::_on_toggle_visual_corners() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		bool current = grid_node->is_debug_corners_visible();
		grid_node->set_debug_corners_visible(!current);
		UtilityFunctions::print("MCManager: Visual Corners toggled to ", !current);
	}
}

// Terrain wireframe overlay on / off.
void MCManager::_on_toggle_wireframe() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->set_debug_wireframe(!grid_node->is_debug_wireframe());
	}
}

// Edge corners on / off: inactive corners joined by an edge to an active corner.
void MCManager::_on_toggle_edge_corners() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->set_debug_edge_corners(!grid_node->is_debug_edge_corners());
	}
}

// Water drop slider: how far held water sits below the bank.
void MCManager::_on_water_drop(double p_value) {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->set_water_drop(static_cast<float>(p_value));
	}
}

// Water mesh builder: sample grid + marching squares, or per cell.
void MCManager::_on_toggle_water_sampled() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->set_water_sampled(!grid_node->get_water_sampled());
	}
}

// Water samples slider: sample grid points per cell side (sampled water).
void MCManager::_on_water_samples(double p_value) {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->set_water_sample_rate(static_cast<int>(p_value));
	}
}

// Water expand slider: how far the water's edge grows on under the banks.
void MCManager::_on_water_expand(double p_value) {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->set_water_expand(static_cast<float>(p_value));
	}
}

// Water particles on / off: shore foam and fall splashes.
void MCManager::_on_toggle_water_particles() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->set_water_particles(!grid_node->get_water_particles());
	}
}

// Water view button: cycles normal -> wireframe -> UV -> UV + wireframe (MCGrid::WaterView).
void MCManager::_on_cycle_water_view() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid_node) {
		return;
	}
	int view = (grid_node->get_water_view() + 1) % MCGrid::WATER_VIEW_COUNT;
	grid_node->set_water_view(view);
	if (water_view_button) {
		water_view_button->set_text(String("Water view: ") + MCGrid::water_view_name(view));
	}
}

// Waterfall lip radius slider: how round the top of a fall is (0: a sharp edge).
void MCManager::_on_water_fall_lip(double p_value) {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->set_water_fall_lip(static_cast<float>(p_value));
	}
}

// Spill reach slider: columns a diagonal spill past a low corner wets (0: none).
void MCManager::_on_water_spill(double p_value) {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->set_water_spill_reach(static_cast<int>(p_value));
	}
}

// Water overlay on / off (held / stream / sea, flow arrows, framed / open falls, last pour).
void MCManager::_on_toggle_water_debug() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->set_debug_water(!grid_node->is_debug_water());
	}
}

// Replay the last pour's flood fill (turns the water overlay on).
void MCManager::_on_replay_pour() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->replay_water_debug();
	}
}

// Create the hover materials (yellow ok, red blocked: wire lines; cyan selected: an overlay tint) and
// the placement box (a wire box, scaled to the placement size).
void MCManager::_initialize_previews() {
	wire_mat_ok = mc_wire_material(Color(1.0f, 0.86f, 0.25f, 1.0f));
	wire_mat_blocked = mc_wire_material(Color(1.0f, 0.3f, 0.25f, 1.0f));

	hover_mat_yellow.instantiate();
	hover_mat_yellow->set_albedo(Color(1, 1, 0, 0.4));
	hover_mat_yellow->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	hover_mat_yellow->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	hover_mat_red.instantiate();
	hover_mat_red->set_albedo(Color(1, 0, 0, 0.6));
	hover_mat_red->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	hover_mat_red->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	hover_mat_cyan.instantiate();
	hover_mat_cyan->set_albedo(Color(0, 1, 1, 0.6));
	hover_mat_cyan->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	hover_mat_cyan->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	if (!hover_box_node) {
		hover_box_node = memnew(MeshInstance3D);
		hover_box_node->set_mesh(mc_wire_box(Vector3(1, 1, 1)));
		add_child(hover_box_node);
		hover_box_node->hide();
	}
}

// Size and place the placement box at a cell (grid space -> global), red when blocked.
void MCManager::_update_hover_box(
		const Vector3i &p_grid_pos,
		bool p_is_blocked
) {
	if (!hover_box_node)
		return;

	hover_box_node->set_scale(Vector3(current_placement_size)); // the wire box is 1 x 1 x 1
	// The box is a child of MCManager but the position is in MCGrid space: place it globally.
	Vector3 local_centre = Vector3(p_grid_pos) + (Vector3(current_placement_size) * 0.5f);
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		hover_box_node->set_global_position(grid_node->to_global(local_centre));
		hover_box_node->set_global_basis(grid_node->get_global_basis().scaled(Vector3(current_placement_size)));
	} else {
		hover_box_node->set_position(local_centre);
	}
	hover_box_node->set_material_override(p_is_blocked ? wire_mat_blocked : wire_mat_ok);
	hover_box_node->show();
}

// Every frame: raycast from the mouse, then update the preview for the current mode
// (placement box, dragged objects, or the hovered corner face).
void MCManager::_update_hover_raycast() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->hide_hover_preview();
	}
	if (hover_box_node) {
		hover_box_node->hide();
	}

	Viewport *viewport = get_viewport();
	if (!viewport) {
		return;
	}

	TypedArray<RID> exclude = _drag_group_exclusions();

	// Ctrl / Cmd / Shift (or inspect mode) hovers cells instead of corners, in the Terrain and the
	// Water tool
	Input *input = Input::get_singleton();
	bool cell_tool = interaction_mode == MODE_TERRAIN || interaction_mode == MODE_WATER;
	bool ctrl_held = cell_tool && (input->is_key_pressed(KEY_CTRL) || input->is_key_pressed(KEY_META) ||
										   input->is_key_pressed(KEY_SHIFT) || _inspect_active(grid_node));

	uint32_t mask = toLayer(LAYER_OBJECTS) | toLayer(LAYER_CORNERS);
	if (ctrl_held) {
		mask |= toLayer(LAYER_TERRAIN);
	}
	if (_inspect_active(grid_node)) {
		mask = toLayer(LAYER_TERRAIN); // the same cell the click pulls out (mc_manager_inspect.cpp)
	}

	MCRaycastHit hit = raycast_from_mouse(this, viewport->get_mouse_position(), mask, 1000.0f, exclude);
	if (hit.is_hit) {
		Node3D *collider_node = Object::cast_to<Node3D>(hit.collider);
		if (collider_node) {
			Camera3D *camera = viewport->get_camera_3d();
			if (camera) {
				MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
				if (grid_node) {
					// Work in MCGrid space so a moved/rotated grid still lines up.
					Vector3 hit_pos = grid_node->to_local(hit.position);
					Vector3 hit_normal =
							(grid_node->get_global_transform().basis.inverse().xform(hit.normal)).normalized();
					Vector3i grid_pos = Vector3i((hit_pos + hit_normal * 0.5f).floor());

					if (ctrl_held) {
						grid_pos = Vector3i((hit_pos - hit_normal * 0.05f).floor());
						locked_grid_pos = grid_pos;
						update_ui();
					}

					bool is_blocked = false;

					Vector3i check_size = current_placement_size;
					if (is_dragging && !drag_group.empty()) {
						check_size = drag_group[0].size;
					}

					if (grid_node->is_area_blocked_by_grid(grid_pos, check_size)) {
						is_blocked = true;
					}

					if (!is_blocked) {
						Vector3i check_size_obj = is_dragging
								? (drag_group.empty() ? current_placement_size : drag_group[0].size)
								: current_placement_size;
						AABB volume = AABB(Vector3(grid_pos), Vector3(check_size_obj));
						if (grid_node->is_area_blocked_by_objects(volume)) {
							is_blocked = true;
						}
					}

					if (is_dragging && !drag_group.empty() && interaction_mode == MODE_DRAG_OBJECT) {
						bool group_blocked = false;
						for (auto &obj : drag_group) {
							Vector3i target_pos = grid_pos + obj.relative_offset;
							obj.node->set_position(Vector3(target_pos) + (Vector3(obj.size) * 0.5f));
							if (grid_node->is_area_blocked_by_grid(target_pos, obj.size) ||
								grid_node->is_area_blocked_by_objects(AABB(Vector3(target_pos), Vector3(obj.size)))) {
								group_blocked = true;
							}
						}
						for (auto &obj : drag_group) {
							obj.node->set_material_overlay(group_blocked ? hover_mat_red : hover_mat_cyan);
						}
						drag_valid = !group_blocked;
					} else if (interaction_mode == MODE_PLACE_OBJECT) {
						_update_hover_box(grid_pos, is_blocked);
					} else if (interaction_mode == MODE_DRAG_OBJECT) {
						for (MeshInstance3D *node : selected_nodes) {
							if (node)
								node->set_material_overlay(hover_mat_cyan);
						}
					} else if (interaction_mode == MODE_TERRAIN || (interaction_mode == MODE_WATER && ctrl_held)) {
						if (ctrl_held) { // the cell (Water tool: only this, no corner hover)
							grid_node->update_hover_preview(
									Vector3(locked_grid_pos) + Vector3(0.5, 0.5, 0.5), hit_normal, camera
							);
						} else {
							Node *parent = collider_node->get_parent();
							Node3D *parent_node = Object::cast_to<Node3D>(parent);
							if (parent_node) {
								Vector3 final_pos = parent_node->get_position();
								locked_grid_pos = Vector3i(final_pos.round());
								grid_node->update_hover_preview(final_pos, hit_normal, camera);
							}
						}
					}
				}
			}
		}
	}
}

// Hash of a grid cell, 0 when there is no grid.
uint8_t MCManager::_get_cell_hash(const Vector3i &p_grid_pos) {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		return grid_node->get_cell_hash(p_grid_pos);
	}
	return 0;
}

} // namespace godot
