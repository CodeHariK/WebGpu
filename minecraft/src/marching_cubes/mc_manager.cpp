#include "mc_manager.h"
#include "cui/cui.h"
#include "mc.h"
#include "mc_raycast.h"
#include "terrain.h"
#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void MCManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mc_node_path", "p_path"), &MCManager::set_mc_node_path);
	ClassDB::bind_method(D_METHOD("get_mc_node_path"), &MCManager::get_mc_node_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "mc_node_path"), "set_mc_node_path", "get_mc_node_path");

	ClassDB::bind_method(D_METHOD("set_terrain_path", "p_path"), &MCManager::set_terrain_path);
	ClassDB::bind_method(D_METHOD("get_terrain_path"), &MCManager::get_terrain_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "terrain_path"), "set_terrain_path", "get_terrain_path");

	ClassDB::bind_method(D_METHOD("initialize_all"), &MCManager::initialize_all);
	ClassDB::bind_method(D_METHOD("_on_toggle_ui"), &MCManager::_on_toggle_ui);
	ClassDB::bind_method(D_METHOD("_on_toggle_collision_debug"), &MCManager::_on_toggle_collision_debug);
	ClassDB::bind_method(D_METHOD("_on_toggle_visual_corners"), &MCManager::_on_toggle_visual_corners);
	ClassDB::bind_method(D_METHOD("_on_gui_input", "p_event"), &MCManager::_on_gui_input);
	ClassDB::bind_method(D_METHOD("_on_show_help"), &MCManager::_on_show_help);
	ClassDB::bind_method(D_METHOD("_on_save_terrain"), &MCManager::_on_save_terrain);
	ClassDB::bind_method(D_METHOD("_on_load_terrain"), &MCManager::_on_load_terrain);
}

MCManager::MCManager() {}

MCManager::~MCManager() {}

void MCManager::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	UtilityFunctions::print("MCManager: Ready, triggering initialization sequence...");
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	call_deferred("initialize_all");
	set_process(true);
}

void MCManager::set_mc_node_path(const NodePath &p_path) {
	mc_node_path = p_path;
}

NodePath MCManager::get_mc_node_path() const {
	return mc_node_path;
}

void MCManager::_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	perf.update_timer += p_delta;
	if (perf.update_timer >= 0.1) {
		perf.update_timer = 0.0;
		update_ui();
	}

	// Update Debug Cursor
	if (debug_cursor_node) {
		debug_cursor_node->set_position(Vector3(debug_cursor_pos.x + 0.5f, debug_cursor_pos.y + 0.5f, debug_cursor_pos.z + 0.5f));
	}

	// Hover Raycast
	if (!hover_root) {
		return;
	}
	hover_root->hide();

	Viewport *viewport = get_viewport();
	if (!viewport) {
		return;
	}

	MCRaycastHit hit = raycast_from_mouse(this, viewport->get_mouse_position(), 512); // Layer 10
	if (hit.is_hit) {
		Node3D *collider_node = Object::cast_to<Node3D>(hit.collider);
		if (collider_node) {
			Node *parent = collider_node->get_parent();
			Node3D *parent_node = Object::cast_to<Node3D>(parent);
			if (parent_node) {
				Camera3D *camera = viewport->get_camera_3d();
				if (camera) {
					Vector3 hit_pos = parent_node->get_position();
					_update_hover_preview(hit_pos, hit.normal, camera);
					hover_root->show();
					return;
				}
			}
		}
	}
}

NodePath MCManager::get_terrain_path() const {
	return terrain.path;
}

void MCManager::set_terrain_path(const NodePath &p_path) {
	terrain.path = p_path;
}

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

void MCManager::initialize_all() {
	if (!is_inside_tree()) {
		return;
	}

	MCNode *mc_node = Object::cast_to<MCNode>(get_node_or_null(mc_node_path));
	MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));

	if (!mc_node) {
		UtilityFunctions::print("MCManager Error: MCNode not found at ", mc_node_path);
	}
	if (!terrain_node) {
		UtilityFunctions::print("MCManager Error: MCTerrain not found at ", terrain.path);
	}

	if (mc_node && terrain_node) {
		UtilityFunctions::print("MCManager: Found all nodes. Forcing sequential initialization...");

		// 1. Force MCNode to be ready (load library and variants based on its import_mode)
		mc_node->initialize_library();

		// 2. Link nodes and trigger Terrain generation
		terrain_node->set_mc_node(mc_node);
		terrain_node->generate_with_noise();

		// 3. Setup UI
		ui.manager = CUI::create_on_new_layer(this);
		if (ui.manager) {
			setup_ui();
			UtilityFunctions::print("MCManager: UI initialized via CUI factory.");
		}
		update_ui();

		// 4. Setup Hover Preview (3-Quad System)
		if (!hover_root) {
			hover_root = memnew(Node3D);
			add_child(hover_root);

			// Materials
			hover_mat_yellow.instantiate();
			hover_mat_yellow->set_albedo(Color(1, 1, 0, 0.6));
			hover_mat_yellow->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
			hover_mat_yellow->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

			hover_mat_white.instantiate();
			hover_mat_white->set_albedo(Color(1, 1, 1, 0.4));
			hover_mat_white->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
			hover_mat_white->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

			// Quads
			Ref<QuadMesh> quad_mesh;
			quad_mesh.instantiate();
			quad_mesh->set_size(Vector2(1.01, 1.01)); // Slightly larger to cover corner sphere

			for (int i = 0; i < 3; ++i) {
				hover_quads[i] = memnew(MeshInstance3D);
				hover_quads[i]->set_mesh(quad_mesh);
				hover_root->add_child(hover_quads[i]);
			}
			hover_root->hide();
		}

		// 5. Setup Debug Cursor
		if (!debug_cursor_node) {
			debug_cursor_node = memnew(MeshInstance3D);
			add_child(debug_cursor_node);

			Ref<BoxMesh> box;
			box.instantiate();
			box->set_size(Vector3(1.05f, 1.05f, 1.05f));
			debug_cursor_node->set_mesh(box);

			debug_cursor_mat.instantiate();
			debug_cursor_mat->set_albedo(Color(0.0f, 1.0f, 0.0f, 0.4f)); // Green transparent
			debug_cursor_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
			debug_cursor_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
			debug_cursor_node->set_material_override(debug_cursor_mat);

			debug_cursor_pos = Vector3i(0, 0, 0); // Start at origin
		}

		UtilityFunctions::print("MCManager: Sequential initialization complete.");
		mc_node->print_library_hashes();
	}
}

void MCManager::_input(const Ref<InputEvent> &p_event) {
	// 1. Debug Cursor Keys (Handle FIRST to avoid early returns)
	Ref<InputEventKey> key_event = p_event;
	if (key_event.is_valid() && key_event->is_pressed()) {
		Key scancode = key_event->get_keycode();
		Vector3i delta(0, 0, 0);

		if (scancode == KEY_A)
			delta.x = -1;
		else if (scancode == KEY_D)
			delta.x = 1;
		else if (scancode == KEY_W)
			delta.z = -1;
		else if (scancode == KEY_S)
			delta.z = 1;
		else if (scancode == KEY_Q)
			delta.y = -1;
		else if (scancode == KEY_E)
			delta.y = 1;

		if (delta != Vector3i(0, 0, 0)) {
			UtilityFunctions::print("MCManager: WASDQE Key Pressed - Delta: ", delta);
			debug_cursor_pos += delta;

			// Clamp to terrain size
			MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));
			if (terrain_node) {
				Vector3i t_size = terrain_node->get_terrain_size();
				Vector3i c_size = terrain_node->get_chunk_size();
				Vector3i max_pos = Vector3i(t_size.x * c_size.x - 1, t_size.y * c_size.y - 1, t_size.z * c_size.z - 1);
				debug_cursor_pos.x = Math::clamp(debug_cursor_pos.x, 0, max_pos.x);
				debug_cursor_pos.y = Math::clamp(debug_cursor_pos.y, 0, max_pos.y);
				debug_cursor_pos.z = Math::clamp(debug_cursor_pos.z, 0, max_pos.z);
			}
			update_ui();
			get_viewport()->set_input_as_handled();
			return; // Handled
		}
	}

	// 2. Mouse Events
	Ref<InputEventMouseButton> mouse_event = p_event;
	if (mouse_event.is_null() || !mouse_event->is_pressed()) {
		return;
	}

	MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));
	if (!terrain_node) {
		return;
	}

	int button_index = mouse_event->get_button_index();
	if (button_index != MOUSE_BUTTON_LEFT && button_index != MOUSE_BUTTON_RIGHT) {
		return;
	}

	MCRaycastHit hit = raycast_from_event(this, p_event, 512); // Layer 10
	if (hit.is_hit) {
		Node3D *collider_node = Object::cast_to<Node3D>(hit.collider);
		if (!collider_node) {
			return;
		}

		// Debug corners are StaticBody3D children of MeshInstance3D
		Node *parent = collider_node->get_parent();
		Node3D *parent_node = Object::cast_to<Node3D>(parent);
		if (!parent_node) {
			return;
		}

		Vector3 hit_pos = parent_node->get_position();
		Vector3i grid_pos = Vector3i(
				static_cast<int32_t>(Math::round(hit_pos.x)),
				static_cast<int32_t>(Math::round(hit_pos.y)),
				static_cast<int32_t>(Math::round(hit_pos.z)));

		if (button_index == MOUSE_BUTTON_LEFT) {
			// Activate neighbor
			Vector3i target_pos = grid_pos + Vector3i(Math::round(hit.normal.x), Math::round(hit.normal.y), Math::round(hit.normal.z));
			terrain_node->modify_corner(target_pos, true);
		} else if (button_index == MOUSE_BUTTON_RIGHT) {
			// Deactivate current corner
			terrain_node->modify_corner(grid_pos, false);
		}
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

void MCManager::_on_save_terrain() {
	MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));
	if (terrain_node) {
		terrain_node->save_terrain("user://terrain.mct");
	}
}

void MCManager::_on_load_terrain() {
	MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));
	if (terrain_node) {
		terrain_node->load_terrain("user://terrain.mct");
	}
}

} // namespace godot
