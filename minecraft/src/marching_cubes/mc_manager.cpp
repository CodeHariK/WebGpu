#include "mc_manager.h"
#include "mc.h"
#include "terrain.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include "mc_raycast.h"
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "cui/cui.h"

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
	if (Engine::get_singleton()->is_editor_hint())
		return;

	perf.update_timer += p_delta;
	if (perf.update_timer >= 0.1) {
		perf.update_timer = 0.0;
		update_ui();
	}

	// Hover Raycast
	if (!hover_preview) return;

	Viewport *viewport = get_viewport();
	if (!viewport) return;

	MCRaycastHit hit = raycast_from_mouse(this, viewport->get_mouse_position(), 512); // Layer 10
	if (hit.is_hit) {
		Node3D *collider_node = Object::cast_to<Node3D>(hit.collider);
		if (collider_node) {
			Node *parent = collider_node->get_parent();
			Node3D *parent_node = Object::cast_to<Node3D>(parent);
			if (parent_node) {
				Vector3 hit_pos = parent_node->get_position();
				// Default to "Add" position using typed access
				Vector3 target_pos = hit_pos + hit.normal;
				hover_preview->set_position(target_pos);
				hover_preview->show();
				return;
			}
		}
	}
	hover_preview->hide();
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
		
		// 1. Force MCNode to be ready (load library and variants)
		mc_node->load_mesh_library();
		mc_node->generate_variants();
		
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

		// 4. Setup Hover Preview
		if (!hover_preview) {
			hover_preview = memnew(MeshInstance3D);
			Ref<BoxMesh> box_mesh;
			box_mesh.instantiate();
			box_mesh->set_size(Vector3(1.05, 1.05, 1.05)); // Slightly larger than 1.0 corners to avoid Z-fighting
			hover_preview->set_mesh(box_mesh);
			
			hover_mat.instantiate();
			hover_mat->set_albedo(Color(1, 1, 0, 0.5)); // Semi-transparent yellow
			hover_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
			hover_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
			hover_preview->set_material_override(hover_mat);
			
			add_child(hover_preview);
			hover_preview->hide();
		}

		UtilityFunctions::print("MCManager: Sequential initialization complete.");
	}
}

void MCManager::_input(const Ref<InputEvent> &p_event) {
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
		if (!collider_node) return;

		// Debug corners are StaticBody3D children of MeshInstance3D
		Node *parent = collider_node->get_parent();
		Node3D *parent_node = Object::cast_to<Node3D>(parent);
		if (!parent_node) return;

		Vector3 hit_pos = parent_node->get_position();
		Vector3i grid_pos = Vector3i(Math::round(hit_pos.x), Math::round(hit_pos.y), Math::round(hit_pos.z));

		if (button_index == MOUSE_BUTTON_LEFT) {
			// Activate neighbor
			Vector3i target_pos = grid_pos + Vector3i(Math::round(hit.normal.x), Math::round(hit.normal.y), Math::round(hit.normal.z));
			terrain_node->modify_corner(target_pos, true);
		} else if (button_index == MOUSE_BUTTON_RIGHT) {
			// Deactivate current
			terrain_node->modify_corner(grid_pos, false);
		}
	}
}

} // namespace godot
