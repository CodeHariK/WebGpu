#include "mc_manager.h"
#include "mc.h"
#include "terrain.h"
#include "utils/raycast/mc_raycast.h"

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/label.hpp>
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
	if (is_dragging && drag_node) {
		// Collect RIDs of collision objects under drag_node
		TypedArray<Node> children = drag_node->get_children();
		for (int i = 0; i < children.size(); i++) {
			CollisionObject3D *co = Object::cast_to<CollisionObject3D>(children[i]);
			if (co) {
				exclude.push_back(co->get_rid());
			}
		}
	}

	uint32_t mask = 40; // Default: Layer 4 (Objects) | Layer 6 (Corners)

	MCRaycastHit hit = raycast_from_mouse(this, viewport->get_mouse_position(), mask, 1000.0f, exclude);
	if (hit.is_hit) {
		Node3D *collider_node = Object::cast_to<Node3D>(hit.collider);
		if (collider_node) {
			Camera3D *camera = viewport->get_camera_3d();
			if (camera) {
				MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));
				if (terrain_node) {
					// 1. Calculate grid_pos based on layer hit
					Vector3i grid_pos;
					CollisionObject3D *co = Object::cast_to<CollisionObject3D>(hit.collider);
					uint32_t hit_layer = co ? co->get_collision_layer() : 0;

					if (hit_layer == 64) {
						// Hit a Cell (Layer 7): Position is floor of hit point
						grid_pos = Vector3i(hit.position.floor());
					} else {
						// Hit an Object or Corner: Use normal offset for snapping
						grid_pos = Vector3i((hit.position + hit.normal * 0.5f).floor());
					}

					bool is_blocked = false;

					// 1. Check Terrain
					Vector3i check_size = is_dragging ? drag_size : current_placement_size;
					if (terrain_node->is_area_blocked_by_terrain(grid_pos, check_size)) {
						is_blocked = true;
					}

					// 2. Check Objects
					if (!is_blocked) {
						AABB volume = AABB(Vector3(grid_pos), Vector3(check_size));
						if (terrain_node->is_area_blocked_by_objects(volume)) {
							is_blocked = true;
						}
					}

					// Determine mode-based visualization
					if (is_dragging && drag_node && interaction_mode == MODE_DRAG_OBJECT) {
						// DRAGGING LOGIC
						drag_node->set_position(Vector3(grid_pos) + (Vector3(drag_size) * 0.5f));
						drag_node->set_material_override(is_blocked ? hover_mat_red : nullptr); // Revert to light blue or use red if blocked
						drag_valid = !is_blocked;
					} else if (interaction_mode == MODE_PLACE_OBJECT) {
						// PLACE PREVIEW ONLY
						_update_hover_box(grid_pos, is_blocked);
					} else {
						// TERRAIN MODE
						Node *parent = collider_node->get_parent();
						Node3D *parent_node = Object::cast_to<Node3D>(parent);
						if (parent_node) {
							Vector3 final_pos = parent_node->get_position();
							_update_hover_preview(final_pos, hit.normal, camera);
							hover_root->show();
						}
					}
				}
			}
		}
	}
}

} // namespace godot
