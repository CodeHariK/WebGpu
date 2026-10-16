// MCManager: the Randomize section of the side panel: new grid corners by the L / M / H rules
// (MCGrid::randomize_levels, core/mc_level_plan.h).
#include "cui/cui.h"
#include "marching_cubes/editor/mc_manager.h"
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

namespace godot {

// The Randomize button and the last result's summary.
void MCManager::_setup_randomize_ui() {
	if (!ui.manager || !ui.stats_vbox) {
		return;
	}
	ui.manager->add_header(ui.stats_vbox, "Randomize (L / M / H rules)");
	Button *randomize_btn = ui.manager->add_button(ui.stats_vbox, "Randomize Grid", Callable(this, "_on_randomize_grid"));
	randomize_btn->set_custom_minimum_size(Vector2(0, 30));
	randomize_label = ui.manager->add_label(ui.stats_vbox, "L bottom, H on column tops, same level side by side");
}

// Next seed, new corners for the whole grid; the summary label follows.
void MCManager::_on_randomize_grid() {
	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid_node) {
		return;
	}
	cancel_drag();
	randomize_seed++;
	String summary = grid_node->randomize_levels(randomize_seed);
	if (randomize_label) {
		randomize_label->set_text(summary);
	}
}

} // namespace godot
