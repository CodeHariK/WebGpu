// MCManager: the Randomize section of the side panel (grid corners by the L / M / H rules,
// MCGrid::randomize_levels, core/mc_level_plan.h; in heightmap mode an island, MCGrid::
// randomize_heights — the "Heightmap mode" button switches it) and pinning: a left click on a library tile
// pins it, moves the grid into a row behind the library (pinned column lined up with the tile)
// and randomizes around it, like the Blender add-on's playground.
#include "cui/cui.h"
#include "game_manager/game_constants.h"
#include "marching_cubes/editor/mc_manager.h"
#include "marching_cubes/grid/mc_grid.h"
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
	bool on = grid_node && grid_node->get_heightmap_mode();
	heightmap_button = ui.manager->add_button(
			ui.stats_vbox, on ? "Heightmap mode: on" : "Heightmap mode: off", Callable(this, "_on_toggle_heightmap")
	);
	heightmap_button->set_custom_minimum_size(Vector2(0, 30));
}

// Heightmap mode on / off: on, clicks move whole columns and Randomize makes an island.
void MCManager::_on_toggle_heightmap() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid_node) {
		return;
	}
	cancel_drag();
	grid_node->set_heightmap_mode(!grid_node->get_heightmap_mode());
	if (heightmap_button) {
		heightmap_button->set_text(grid_node->get_heightmap_mode() ? "Heightmap mode: on" : "Heightmap mode: off");
	}
}

// Next seed, new corners for the whole grid (around the pin, if any); the summary label follows.
void MCManager::_on_randomize_grid() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid_node) {
		return;
	}
	cancel_drag();
	randomize_seed++;
	String summary = grid_node->randomize_levels(randomize_seed);
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
