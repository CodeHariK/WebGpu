#include "mc.h"
#include "mc_manager.h"
#include "mc_physics.h"
#include "terrain.h"
#include "utils/raycast/mc_raycast.h"

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/material.hpp>
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
	MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));
	if (terrain_node) {
		bool current = terrain_node->is_debug_corners_visible();
		terrain_node->set_debug_corners_visible(!current);
		UtilityFunctions::print("MCManager: Visual Corners toggled to ", !current);
	}
}

void MCManager::_update_hover_preview(const Vector3 &p_corner_pos, const Vector3 &p_hit_normal, Camera3D *p_camera) {
	if (!hover_root || !p_camera) {
		return;
	}

	Vector3 cam_pos = p_camera->get_global_position();
	Vector3 dir_to_cam = cam_pos - p_corner_pos;

	// Visible normals (3 faces) based on camera position relative to corner
	Vector3 normals[3] = {
		Vector3(dir_to_cam.x >= 0 ? 1.0f : -1.0f, 0.0f, 0.0f),
		Vector3(0.0f, dir_to_cam.y >= 0 ? 1.0f : -1.0f, 0.0f),
		Vector3(0.0f, 0.0f, dir_to_cam.z >= 0 ? 1.0f : -1.0f)
	};

	hover_root->set_position(p_corner_pos);

	for (int i = 0; i < 3; ++i) {
		MeshInstance3D *quad = hover_quads[i];
		Vector3 n = normals[i];

		// Position quad on the face (0.505 offset to avoid internal Z-fighting)
		quad->set_position(n * 0.505f);

		// Orientation to face the normal
		if (n.x != 0.0f) {
			quad->set_rotation_degrees(Vector3(0.0f, n.x > 0.0f ? 90.0f : -90.0f, 0.0f));
		} else if (n.y != 0.0f) {
			quad->set_rotation_degrees(Vector3(n.y > 0.0f ? -90.0f : 90.0f, 0.0f, 0.0f));
		} else {
			quad->set_rotation_degrees(Vector3(0.0f, n.z > 0.0f ? 0.0f : 180.0f, 0.0f));
		}

		// Material logic: Highlight the currently hovered face
		if (n.is_equal_approx(p_hit_normal)) {
			quad->set_material_override(hover_mat_yellow);
		} else {
			quad->set_material_override(hover_mat_white);
		}
	}
}

void MCManager::_initialize_previews() {
	// 4. Setup Hover Preview (3-Quad System)
	if (!hover_root) {
		hover_root = memnew(Node3D);
		add_child(hover_root);

		// Materials
		hover_mat_yellow.instantiate();
		hover_mat_yellow->set_albedo(Color(1, 1, 0, 0.4)); // Translucent yellow
		hover_mat_yellow->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		hover_mat_yellow->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

		hover_mat_white.instantiate();
		hover_mat_white->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		hover_mat_white->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

		hover_mat_red.instantiate();
		hover_mat_red->set_albedo(Color(1, 0, 0, 0.6));
		hover_mat_red->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		hover_mat_red->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

		hover_mat_cyan.instantiate();
		hover_mat_cyan->set_albedo(Color(0, 1, 1, 0.6));
		hover_mat_cyan->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		hover_mat_cyan->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

		// Quads
		Ref<QuadMesh> quad_mesh;
		quad_mesh.instantiate();
		quad_mesh->set_size(Vector2(1.01, 1.01));

		for (int i = 0; i < 3; ++i) {
			hover_quads[i] = memnew(MeshInstance3D);
			hover_quads[i]->set_mesh(quad_mesh);
			hover_root->add_child(hover_quads[i]);
		}
		hover_root->hide();
	}

	// 6. Setup Hover Box (AxBxC Preview)
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
	if (!hover_root || !hover_box_node) {
		return;
	}
	hover_root->hide();
	hover_box_node->hide();

	Viewport *viewport = get_viewport();
	if (!viewport) {
		return;
	}

	TypedArray<RID> exclude;
	if (is_dragging && !drag_group.empty()) {
		// Collect RIDs of collision objects under all nodes in drag_group
		for (const auto &obj : drag_group) {
			if (!obj.node) continue;
			TypedArray<Node> children = obj.node->get_children();
			for (int j = 0; j < children.size(); j++) {
				CollisionObject3D *co = Object::cast_to<CollisionObject3D>(children[j]);
				if (co) {
					exclude.push_back(co->get_rid());
				}
			}
		}
	} else if (!selected_nodes.empty()) {
		// Also exclude selected nodes from raycast if needed? 
		// Actually, we want to click on them to start dragging or toggle selection.
		// So DO NOT exclude selected_nodes unless dragging.
	}

	uint32_t mask = LAYER_OBJECTS | LAYER_CORNERS;

	MCRaycastHit hit = raycast_from_mouse(this, viewport->get_mouse_position(), mask, 1000.0f, exclude);
	if (hit.is_hit) {
		Node3D *collider_node = Object::cast_to<Node3D>(hit.collider);
		if (collider_node) {
			Camera3D *camera = viewport->get_camera_3d();
			if (camera) {
				MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));
				if (terrain_node) {
					// 1. Calculate grid_pos based on hit
					Vector3i grid_pos = Vector3i((hit.position + hit.normal * 0.5f).floor());

					bool is_blocked = false;

					// 2. Check Terrain
					Vector3i check_size = current_placement_size;
					if (is_dragging && !drag_group.empty()) {
						check_size = drag_group[0].size; // Use primary member size for general raycast feedback
					}
					
					if (terrain_node->is_area_blocked_by_terrain(grid_pos, check_size)) {
						is_blocked = true;
					}

					// 3. Check Objects (for the primary placement/drag object)
					// Note: Detailed group validation is done below
					if (!is_blocked) {
						Vector3i check_size = is_dragging ? (drag_group.empty() ? current_placement_size : drag_group[0].size) : current_placement_size;
						AABB volume = AABB(Vector3(grid_pos), Vector3(check_size));
						if (terrain_node->is_area_blocked_by_objects(volume)) {
							is_blocked = true;
						}
					}

					// Determine mode-based visualization
					if (is_dragging && !drag_group.empty() && interaction_mode == MODE_DRAG_OBJECT) {
						// MULTI-DRAGGING LOGIC
						bool group_blocked = false;
						
						// 1. Calculate and apply positions, then check global validity
						for (auto &obj : drag_group) {
							Vector3i target_pos = grid_pos + obj.relative_offset;
							obj.node->set_position(Vector3(target_pos) + (Vector3(obj.size) * 0.5f));
							
							// Check if this member is blocked
							if (terrain_node->is_area_blocked_by_terrain(target_pos, obj.size) ||
								terrain_node->is_area_blocked_by_objects(AABB(Vector3(target_pos), Vector3(obj.size)))) {
								group_blocked = true;
							}
						}
						
						// 2. Apply visual feedback to all objects in group
						for (auto &obj : drag_group) {
							obj.node->set_material_override(group_blocked ? hover_mat_red : nullptr);
						}
						
						drag_valid = !group_blocked;
					} else if (interaction_mode == MODE_PLACE_OBJECT) {
						// PLACE PREVIEW ONLY
						_update_hover_box(grid_pos, is_blocked);
					} else if (interaction_mode == MODE_DRAG_OBJECT) {
						// Not dragging, just hovering. Show selection highlights.
						// We'll handle selection highlights in a separate pass below or here.
						for (MeshInstance3D *node : selected_nodes) {
							if (node) node->set_material_override(hover_mat_cyan);
						}
					} else if (interaction_mode == MODE_TERRAIN) {
						if (is_locked) {
							// Shift to dual grid center (0.5 offset)
							_update_hover_preview(Vector3(locked_grid_pos) + Vector3(0.5, 0.5, 0.5), hit.normal, camera);
							hover_root->show();
						} else {
							// Update locked_grid_pos for UI even when not locked
							Node *parent = collider_node->get_parent();
							Node3D *parent_node = Object::cast_to<Node3D>(parent);
							if (parent_node) {
								Vector3 final_pos = parent_node->get_position();
								locked_grid_pos = Vector3i(final_pos.round());
								_update_hover_preview(final_pos, hit.normal, camera);
								hover_root->show();
							}
						}
					}
				}
			}
		}
	} else if (interaction_mode == MODE_TERRAIN && is_locked) {
		// When locked, keep showing the quads even if mouse is away
		Viewport *viewport = get_viewport();
		if (viewport) {
			Camera3D *camera = viewport->get_camera_3d();
			if (camera) {
				// Use zero normal (no specific face highlight) when mouse is away
				_update_hover_preview(Vector3(locked_grid_pos) + Vector3(0.5, 0.5, 0.5), Vector3(0, 0, 0), camera);
				hover_root->show();
			}
		}
	}
}

uint8_t MCManager::_get_cell_hash(const Vector3i &p_grid_pos) {
	MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));
	if (terrain_node) {
		return terrain_node->get_cell_hash_at_global_coord(p_grid_pos);
	}
	return 0;
}

} // namespace godot
