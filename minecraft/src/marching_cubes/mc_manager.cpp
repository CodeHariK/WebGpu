#include "mc_manager.h"
#include "cui/cui.h"
#include "mc.h"
#include "terrain.h"
#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_object3d.hpp>
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
	ClassDB::bind_method(D_METHOD("_on_toggle_placement_mode"), &MCManager::_on_toggle_placement_mode);
	ClassDB::bind_method(D_METHOD("_on_show_help"), &MCManager::_on_show_help);
	ClassDB::bind_method(D_METHOD("_on_save_terrain"), &MCManager::_on_save_terrain);
	ClassDB::bind_method(D_METHOD("_on_load_terrain"), &MCManager::_on_load_terrain);
}

MCManager::MCManager() {
	current_placement_size = Vector3i(2, 3, 4);
}

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

	_update_hover_raycast();
}

NodePath MCManager::get_terrain_path() const {
	return terrain.path;
}

void MCManager::set_terrain_path(const NodePath &p_path) {
	terrain.path = p_path;
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

		// 4-7. Setup Previews and Debug Cursor
		_initialize_previews();

		UtilityFunctions::print("MCManager: Sequential initialization complete.");
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
