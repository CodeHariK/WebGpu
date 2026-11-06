#include "marching_cubes/editor/mc_manager.h"
#include "cui/cui.h"
#include "game_manager/game_manager.h"
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/library/mc_library.h"
#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
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

namespace {
const char *SAVE_PATH = "user://terrain.mct"; // Save / Load State, and loaded at start
} // namespace

// Inspector properties (node paths) and the methods the UI buttons call.
void MCManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tile_library_path", "p_path"), &MCManager::set_tile_library_path);
	ClassDB::bind_method(D_METHOD("get_tile_library_path"), &MCManager::get_tile_library_path);
	ADD_PROPERTY(
			PropertyInfo(Variant::NODE_PATH, "tile_library_path"), "set_tile_library_path", "get_tile_library_path"
	);

	ClassDB::bind_method(D_METHOD("set_grid_path", "p_path"), &MCManager::set_grid_path);
	ClassDB::bind_method(D_METHOD("get_grid_path"), &MCManager::get_grid_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "grid_path"), "set_grid_path", "get_grid_path");

	ClassDB::bind_method(D_METHOD("initialize_all"), &MCManager::initialize_all);
	ClassDB::bind_method(D_METHOD("_on_randomize_grid"), &MCManager::_on_randomize_grid);
	ClassDB::bind_method(D_METHOD("_on_unpin"), &MCManager::_on_unpin);
	ClassDB::bind_method(D_METHOD("_on_toggle_heightmap"), &MCManager::_on_toggle_heightmap);
	ClassDB::bind_method(D_METHOD("_on_select_tool", "mode"), &MCManager::_on_select_tool);
	ClassDB::bind_method(D_METHOD("_on_undo"), &MCManager::_on_undo);
	ClassDB::bind_method(D_METHOD("_on_redo"), &MCManager::_on_redo);
	ClassDB::bind_method(D_METHOD("_on_toggle_ui"), &MCManager::_on_toggle_ui);
	ClassDB::bind_method(D_METHOD("_on_toggle_visual_corners"), &MCManager::_on_toggle_visual_corners);
	ClassDB::bind_method(D_METHOD("_on_toggle_wireframe"), &MCManager::_on_toggle_wireframe);
	ClassDB::bind_method(D_METHOD("_on_toggle_water_sampled"), &MCManager::_on_toggle_water_sampled);
	ClassDB::bind_method(D_METHOD("_on_water_drop", "value"), &MCManager::_on_water_drop);
	ClassDB::bind_method(D_METHOD("_on_water_expand", "value"), &MCManager::_on_water_expand);
	ClassDB::bind_method(D_METHOD("_on_water_spill", "value"), &MCManager::_on_water_spill);
	ClassDB::bind_method(D_METHOD("_on_water_spill_radius", "value"), &MCManager::_on_water_spill_radius);
	ClassDB::bind_method(D_METHOD("_on_water_debug_stage", "value"), &MCManager::_on_water_debug_stage);
	ClassDB::bind_method(D_METHOD("_on_toggle_tile_seams"), &MCManager::_on_toggle_tile_seams);
	ClassDB::bind_method(D_METHOD("_on_toggle_water_outline_fill"), &MCManager::_on_toggle_water_outline_fill);
	ClassDB::bind_method(D_METHOD("_on_water_fall_lip", "value"), &MCManager::_on_water_fall_lip);
	ClassDB::bind_method(D_METHOD("_on_water_samples", "value"), &MCManager::_on_water_samples);
	ClassDB::bind_method(D_METHOD("_on_toggle_inspect"), &MCManager::_on_toggle_inspect);
	ClassDB::bind_method(D_METHOD("_on_toggle_edge_corners"), &MCManager::_on_toggle_edge_corners);
	ClassDB::bind_method(D_METHOD("_on_inspect_view_input", "event"), &MCManager::_on_inspect_view_input);
	ClassDB::bind_method(D_METHOD("inspect_at_cell", "cell"), &MCManager::inspect_at_cell);
	ClassDB::bind_method(D_METHOD("_on_toggle_water_debug"), &MCManager::_on_toggle_water_debug);
	ClassDB::bind_method(D_METHOD("_on_toggle_water_particles"), &MCManager::_on_toggle_water_particles);
	ClassDB::bind_method(D_METHOD("_on_cycle_water_view"), &MCManager::_on_cycle_water_view);
	ClassDB::bind_method(D_METHOD("_on_record_view"), &MCManager::_on_record_view);
	ClassDB::bind_method(D_METHOD("_on_replay_pour"), &MCManager::_on_replay_pour);
	ClassDB::bind_method(D_METHOD("_on_gui_input", "p_event"), &MCManager::_on_gui_input);
	ClassDB::bind_method(D_METHOD("_on_show_help"), &MCManager::_on_show_help);
	ClassDB::bind_method(D_METHOD("_on_save_terrain"), &MCManager::_on_save_terrain);
	ClassDB::bind_method(D_METHOD("_on_load_terrain"), &MCManager::_on_load_terrain);
	ClassDB::bind_method(D_METHOD("_on_reset_map"), &MCManager::_on_reset_map);
	ClassDB::bind_method(D_METHOD("_on_pour_back"), &MCManager::_on_pour_back);
	ClassDB::bind_method(D_METHOD("_on_pour_next"), &MCManager::_on_pour_next);
	ClassDB::bind_method(D_METHOD("_on_pour_back_10"), &MCManager::_on_pour_back_10);
	ClassDB::bind_method(D_METHOD("_on_pour_next_10"), &MCManager::_on_pour_next_10);
	ClassDB::bind_method(D_METHOD("_on_pour_whole"), &MCManager::_on_pour_whole);
	ClassDB::bind_method(D_METHOD("_on_trace_settle"), &MCManager::_on_trace_settle);
	ClassDB::bind_method(D_METHOD("_on_trace_spills"), &MCManager::_on_trace_spills);
	ClassDB::bind_method(D_METHOD("_on_trace_held"), &MCManager::_on_trace_held);
	ClassDB::bind_method(D_METHOD("_on_trace_flow"), &MCManager::_on_trace_flow);
	ClassDB::bind_method(D_METHOD("_on_copy_for_blender"), &MCManager::_on_copy_for_blender);
}

MCManager::MCManager() { current_placement_size = Vector3i(2, 3, 4); }

MCManager::~MCManager() {}

// Outside the editor: show the mouse and defer initialize_all until the scene is ready.
void MCManager::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	UtilityFunctions::print("MCManager: Ready, triggering initialization sequence...");
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	call_deferred("initialize_all");
	set_process(true);
}

// Node path to the MCTileLibrary.
void MCManager::set_tile_library_path(const NodePath &p_path) { tile_library_path = p_path; }

// Node path to the MCTileLibrary.
NodePath MCManager::get_tile_library_path() const { return tile_library_path; }

// Refresh the stats 10 times a second and run the hover raycast every frame.
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
	_update_inspect_view(p_delta);
}

// Node path to the MCGrid.
NodePath MCManager::get_grid_path() const { return grid_stats.path; }

// Node path to the MCGrid.
void MCManager::set_grid_path(const NodePath &p_path) { grid_stats.path = p_path; }

// Load the library, build the grid, create the UI and previews, register with GameManager.
void MCManager::initialize_all() {
	if (!is_inside_tree()) {
		return;
	}

	MCTileLibrary *tile_library = Object::cast_to<MCTileLibrary>(get_node_or_null(tile_library_path));
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));

	if (!tile_library) {
		UtilityFunctions::print("MCManager Error: MCTileLibrary not found at ", tile_library_path);
	}
	if (!grid_node) {
		UtilityFunctions::print("MCManager Error: MCGrid not found at ", grid_stats.path);
	}

	if (tile_library && grid_node) {
		UtilityFunctions::print("MCManager: Found all nodes. Forcing sequential initialization...");

		// 1. Force MCTileLibrary to be ready (load the tiles and fill the rest by turning them about Y)
		tile_library->initialize_library();

		// 2. Link nodes and trigger Terrain generation
		grid_node->set_tile_library(tile_library);
		start_grid_dims = grid_node->get_grid_dims(); // as the scene sets it: Reset rebuilds this
		start_chunk_dims = grid_node->get_chunk_dims();
		grid_node->initialize_grid(
				start_grid_dims.x, start_grid_dims.y, start_grid_dims.z,
				start_chunk_dims.x, start_chunk_dims.y, start_chunk_dims.z
		);
		// the saved map, when there is one (Map tab: Save State writes it, Reset clears it away)
		if (FileAccess::file_exists(SAVE_PATH)) {
			load_terrain(SAVE_PATH);
		}

		// 3. Setup UI
		ui.manager = CUI::create_on_new_layer(this);
		if (ui.manager) {
			setup_ui();
			UtilityFunctions::print("MCManager: UI initialized via CUI factory.");
		}
		update_ui();

		// 4-7. Setup Previews and Debug Cursor
		_initialize_previews();

		// Register with GameManager
		GameManager *gm = GameManager::get_singleton();
		if (gm) {
			gm->register_mc_manager(this);
		}

		UtilityFunctions::print("MCManager: Sequential initialization complete.");
	}
}

// Load button: user://terrain.mct (also loaded at start when it exists).
void MCManager::_on_load_terrain() { load_terrain(SAVE_PATH); }

// Save button: user://terrain.mct.
void MCManager::_on_save_terrain() { save_terrain(SAVE_PATH); }

// Reset button: clear everything. The grid is rebuilt as the scene starts it (its own size, flat
// ground, no water); placed objects, roofs, the pin, undo history and the last pour go with it
// (MCGrid::initialize_grid). The saved file is left alone (Load State brings it back).
void MCManager::_on_reset_map() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid_node || start_grid_dims == Vector3i()) {
		return;
	}
	cancel_drag(); // drop every pointer to placed objects first
	_clear_selection();
	grid_node->initialize_grid(
			start_grid_dims.x, start_grid_dims.y, start_grid_dims.z,
			start_chunk_dims.x, start_chunk_dims.y, start_chunk_dims.z,
			true
	);
	_step_pour(0); // the step label back to "whole pour"
	update_ui();
}

// Copy for Blender button: the grid's cells as JSON on the clipboard.
void MCManager::_on_copy_for_blender() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid_node) {
		return;
	}
	String json = grid_node->cells_to_json();
	DisplayServer::get_singleton()->clipboard_set(json);
	UtilityFunctions::print("MCManager: Copied ", json.length(), " characters of cell JSON for Blender.");
}

// Save the grid's corners to p_path.
void MCManager::save_terrain(const String &p_path) {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->save_grid(p_path);
	}
}

// Load corners from p_path, after putting any dragged objects back.
void MCManager::load_terrain(const String &p_path) {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		// Loading may free placed objects (when the grid size changes): drop every pointer to them
		// first, putting a drag back where it started.
		cancel_drag();
		_clear_selection();
		grid_node->load_grid(p_path);
	}
}

} // namespace godot
