#include "mc.h"
#include "mc_grid.h"
#include "mc_manager.h"
#include "mc_physics.h"
#include "utils/raycast/mc_raycast.h"

#include <godot_cpp/classes/box_mesh.hpp>
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

void MCManager::_on_toggle_collision_debug() {
	SceneTree *tree = get_tree();
	if (tree) {
		bool current = tree->is_debugging_collisions_hint();
		tree->set_debug_collisions_hint(!current);
		UtilityFunctions::print("MCManager: Collision Debug toggled to ", !current);
	}
}

void MCManager::_on_toggle_visual_corners() {
	MCGrid *terrain_node = Object::cast_to<MCGrid>(get_node_or_null(terrain.path));
	if (terrain_node) {
		bool current = terrain_node->is_debug_corners_visible();
		terrain_node->set_debug_corners_visible(!current);
		UtilityFunctions::print("MCManager: Visual Corners toggled to ", !current);
	}
}


void MCManager::_initialize_previews() {
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
		Ref<BoxMesh> box;
		box.instantiate();
		box->set_size(Vector3(current_placement_size));
		hover_box_node->set_mesh(box);
		add_child(hover_box_node);
		hover_box_node->hide();
	}
}

void MCManager::_update_hover_box(const Vector3i &p_grid_pos, bool p_is_blocked) {
	if (!hover_box_node)
		return;

	Ref<BoxMesh> box = hover_box_node->get_mesh();
	if (box.is_valid()) {
		box->set_size(Vector3(current_placement_size));
	}
	hover_box_node->set_position(Vector3(p_grid_pos) + (Vector3(current_placement_size) * 0.5f));
	hover_box_node->set_material_override(p_is_blocked ? hover_mat_red : hover_mat_yellow);
	hover_box_node->show();
}

void MCManager::_update_hover_raycast() {
	MCGrid *terrain_node = Object::cast_to<MCGrid>(get_node_or_null(terrain.path));
	if (terrain_node) {
		terrain_node->hide_hover_preview();
	}
	if (hover_box_node) {
		hover_box_node->hide();
	}

	Viewport *viewport = get_viewport();
	if (!viewport) {
		return;
	}

	TypedArray<RID> exclude;
	if (is_dragging && !drag_group.empty()) {
		for (const auto &obj : drag_group) {
			if (!obj.node)
				continue;
			TypedArray<Node> children = obj.node->get_children();
			for (int j = 0; j < children.size(); j++) {
				CollisionObject3D *co = Object::cast_to<CollisionObject3D>(children[j]);
				if (co) {
					exclude.push_back(co->get_rid());
				}
			}
		}
	}

	bool ctrl_held = Input::get_singleton()->is_key_pressed(KEY_CTRL) || Input::get_singleton()->is_key_pressed(KEY_META);

	uint32_t mask = LAYER_OBJECTS | LAYER_CORNERS;
	if (ctrl_held && interaction_mode == MODE_TERRAIN) {
		mask |= LAYER_TERRAIN;
	}

	MCRaycastHit hit = raycast_from_mouse(this, viewport->get_mouse_position(), mask, 1000.0f, exclude);
	if (hit.is_hit) {
		Node3D *collider_node = Object::cast_to<Node3D>(hit.collider);
		if (collider_node) {
			Camera3D *camera = viewport->get_camera_3d();
			if (camera) {
				MCGrid *terrain_node = Object::cast_to<MCGrid>(get_node_or_null(terrain.path));
				if (terrain_node) {
					Vector3i grid_pos = Vector3i((hit.position + hit.normal * 0.5f).floor());

					if (ctrl_held && interaction_mode == MODE_TERRAIN) {
						grid_pos = Vector3i((hit.position - hit.normal * 0.05f).floor());
						locked_grid_pos = grid_pos;
						update_ui();
					}

					bool is_blocked = false;

					Vector3i check_size = current_placement_size;
					if (is_dragging && !drag_group.empty()) {
						check_size = drag_group[0].size;
					}

					if (terrain_node->is_area_blocked_by_grid(grid_pos, check_size)) {
						is_blocked = true;
					}

					if (!is_blocked) {
						Vector3i check_size_obj = is_dragging ? (drag_group.empty() ? current_placement_size : drag_group[0].size) : current_placement_size;
						AABB volume = AABB(Vector3(grid_pos), Vector3(check_size_obj));
						if (terrain_node->is_area_blocked_by_objects(volume)) {
							is_blocked = true;
						}
					}

					if (is_dragging && !drag_group.empty() && interaction_mode == MODE_DRAG_OBJECT) {
						bool group_blocked = false;
						for (auto &obj : drag_group) {
							Vector3i target_pos = grid_pos + obj.relative_offset;
							obj.node->set_position(Vector3(target_pos) + (Vector3(obj.size) * 0.5f));
							if (terrain_node->is_area_blocked_by_grid(target_pos, obj.size) ||
									terrain_node->is_area_blocked_by_objects(AABB(Vector3(target_pos), Vector3(obj.size)))) {
								group_blocked = true;
							}
						}
						for (auto &obj : drag_group) {
							obj.node->set_material_override(group_blocked ? hover_mat_red : nullptr);
						}
						drag_valid = !group_blocked;
					} else if (interaction_mode == MODE_PLACE_OBJECT) {
						_update_hover_box(grid_pos, is_blocked);
					} else if (interaction_mode == MODE_DRAG_OBJECT) {
						for (MeshInstance3D *node : selected_nodes) {
							if (node)
								node->set_material_override(hover_mat_cyan);
						}
					} else if (interaction_mode == MODE_TERRAIN) {
						if (ctrl_held) {
							terrain_node->update_hover_preview(Vector3(locked_grid_pos) + Vector3(0.5, 0.5, 0.5), hit.normal, camera);
						} else {
							Node *parent = collider_node->get_parent();
							Node3D *parent_node = Object::cast_to<Node3D>(parent);
							if (parent_node) {
								Vector3 final_pos = parent_node->get_position();
								locked_grid_pos = Vector3i(final_pos.round());
								terrain_node->update_hover_preview(final_pos, hit.normal, camera);
							}
						}
					}
				}
			}
		}
	}
}

uint8_t MCManager::_get_cell_hash(const Vector3i &p_grid_pos) {
	MCGrid *terrain_node = Object::cast_to<MCGrid>(get_node_or_null(terrain.path));
	if (terrain_node) {
		return terrain_node->get_cell_hash_at_global_coord(p_grid_pos);
	}
	return 0;
}

} // namespace godot
