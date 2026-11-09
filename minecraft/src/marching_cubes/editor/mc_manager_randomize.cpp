// MCManager: the Randomize section of the side panel (grid corners by the L / M / H rules,
// MCGrid::randomize_levels, core/mc_level_plan.h; in heightmap mode an island, MCGrid::
// randomize_heights; with squares only an island of sheets, MCGrid::randomize_squares — the
// "Heightmap" and "Draw" buttons switch them) and pinning: a left click on a library tile pins it,
// moves the grid into a row behind the library (pinned column lined up with the tile) and
// randomizes around it, like the Blender add-on's playground.
#include "cui/cui.h"
#include "game_manager/game_constants.h"
#include "marching_cubes/editor/mc_manager.h"
#include "marching_cubes/terrain/mc_grid.h"
#include "marching_cubes/library/mc_library.h"
#include "utils/raycast/mc_raycast.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

namespace godot {

namespace {
const float ROW_GAP = 2.0f; // metres between the library and the grid's row
} // namespace

// The Randomize / Unpin buttons and the last result's summary.
void MCManager::_setup_randomize_ui() {
	if (!ui.manager || !ui.stats_vbox) {
		return;
	}
	ui.manager->add_header(ui.stats_vbox, "Randomize (L / M / H rules)");
	Button *randomize_btn = ui.manager->add_button(ui.stats_vbox, "Randomize Grid", Callable(this, "_on_randomize_grid"));
	randomize_btn->set_custom_minimum_size(Vector2(0, 30));
	Button *unpin_btn = ui.manager->add_button(ui.stats_vbox, "Unpin", Callable(this, "_on_unpin"));
	unpin_btn->set_custom_minimum_size(Vector2(0, 30));
	randomize_label = ui.manager->add_label(ui.stats_vbox, "Click a library tile to pin it");
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	heightmap_button = ui.manager->add_button(ui.stats_vbox, "", Callable(this, "_on_toggle_heightmap"));
	heightmap_button->set_custom_minimum_size(Vector2(0, 30));
	draw_button = ui.manager->add_button(ui.stats_vbox, "", Callable(this, "_on_toggle_squares"));
	draw_button->set_custom_minimum_size(Vector2(0, 30));
	_update_terrain_buttons(grid_node);
}

// The two terrain buttons' text: how the corners are edited, and how they are drawn.
void MCManager::_update_terrain_buttons(MCGrid *p_grid) {
	bool heightmap = p_grid && p_grid->get_heightmap_mode();
	bool squares = p_grid && p_grid->get_squares_mode();
	if (heightmap_button) {
		heightmap_button->set_text(heightmap ? "Heightmap: on" : "Heightmap: off");
	}
	if (draw_button) {
		draw_button->set_text(squares ? "Draw: squares" : "Draw: cubes");
	}
}

// Heightmap on / off: how the corners are edited (on: clicks move whole columns, water works,
// Randomize makes an island; switching clears the undo history).
void MCManager::_on_toggle_heightmap() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid_node) {
		return;
	}
	cancel_drag();
	grid_node->set_heightmap_mode(!grid_node->get_heightmap_mode());
	_update_terrain_buttons(grid_node);
	_update_history_buttons(); // switching clears the history
}

// Draw: cubes (the full tile set) or squares (each layer a marching squares sheet).
void MCManager::_on_toggle_squares() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid_node) {
		return;
	}
	grid_node->set_squares_mode(!grid_node->get_squares_mode());
	_update_terrain_buttons(grid_node);
}

// Next seed, new corners for the whole grid (around the pin, if any); the summary label follows.
void MCManager::_on_randomize_grid() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid_node) {
		return;
	}
	cancel_drag();
	randomize_seed++;
	grid_node->begin_edit(); // one undo step
	String summary = grid_node->randomize_levels(randomize_seed);
	grid_node->end_edit();
	_update_history_buttons();
	if (randomize_label) {
		randomize_label->set_text((pinned_name.is_empty() ? String() : "Pinned " + pinned_name + "\n") + summary);
	}
}

// Randomize no longer keeps a library tile.
void MCManager::_on_unpin() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node) {
		grid_node->clear_pin();
	}
	pinned_name = String();
	if (randomize_label) {
		randomize_label->set_text("Click a library tile to pin it");
	}
}

// The nearest thing under the mouse is a library tile: pin it, move the grid, randomize.
bool MCManager::_try_pin_click(
		const Ref<InputEvent> &p_event,
		MCGrid *p_grid
) {
	uint32_t mask = toLayer(LAYER_TILE_LIBRARY) | toLayer(LAYER_CORNERS) | toLayer(LAYER_OBJECTS) |
			toLayer(LAYER_TERRAIN);
	MCRaycastHit hit = raycast_from_event(this, p_event, mask, 512);
	Node3D *body = hit.is_hit ? Object::cast_to<Node3D>(hit.collider) : nullptr;
	if (!body || !body->has_meta("mc_hash")) {
		return false; // the grid (or nothing) is nearer: a normal click
	}
	Ref<Mesh> mesh = body->get_meta("mc_mesh");
	p_grid->set_pin(body->get_meta("mc_hash"), body->get_meta("mc_level"), mesh);
	pinned_name = body->get_meta("mc_name");

	// Like the add-on's playground: a row behind the library, the pinned column lined up with the tile.
	MCTileLibrary *library = Object::cast_to<MCTileLibrary>(get_node_or_null(tile_library_path));
	Vector3 tile = body->get_global_position();
	Vector3i pin_cell = p_grid->get_pin_cell();
	Vector3i cells = p_grid->get_grid_dims() * p_grid->get_chunk_dims();
	float behind = library ? library->get_showcase_bounds().position.z : tile.z;
	p_grid->set_global_position(Vector3(
			Math::floor(tile.x) - static_cast<float>(pin_cell.x), p_grid->get_global_position().y,
			Math::floor(behind - ROW_GAP) - static_cast<float>(cells.z)
	));
	_on_randomize_grid();
	return true;
}

} // namespace godot
