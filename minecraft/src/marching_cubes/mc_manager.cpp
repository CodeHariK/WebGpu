#include "mc_manager.h"
#include "mc.h"
#include "terrain.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/label.hpp>
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
	call_deferred("initialize_all");
	set_process(true);
}

void MCManager::set_mc_node_path(const NodePath &p_path) {
	mc_node_path = p_path;
}

NodePath MCManager::get_mc_node_path() const {
	return mc_node_path;
}

void MCManager::set_terrain_path(const NodePath &p_path) {
	terrain.path = p_path;
}

NodePath MCManager::get_terrain_path() const {
	return terrain.path;
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

		UtilityFunctions::print("MCManager: Sequential initialization complete.");
	}
}

} // namespace godot
