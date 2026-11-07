// MCManager: the water, step by step (Debug tab: "Start water debug", then Prev / Next or the
// left / right arrow keys). One walk through every stage of how the water is built:
//
//   0  step 1   the flood (the last pour with the Water tool, or the whole map: MCGrid::trace_settle)
//   1  step 2a  the spills          (MCGrid::trace_water_step 2)
//   2  step 2b  held or stream      (3)
//   3  step 2c  the flow            (4)
//   4  steps 3 - 4 for one cell     (MCGrid::trace_water_cell): it asks for a left click on the map
//                                   to pick the cell; Next past its last step asks for another
//
// Next past a stage's last step records the next stage and shows its first step; Prev before a
// stage's first step goes back to the previous stage's last. A stage with nothing to show (no
// spills on this map, say) is skipped. Two labels: where the walk is (stage, step i / n) and the
// step's explanation. The overlay is MCGrid's water debug (it draws the shown step).
#include "marching_cubes/editor/mc_manager.h"
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <algorithm>
#include <cmath>

namespace godot {

namespace {
const int STAGE_CELL = 4; // steps 3 - 4 (one cell)
// (plain ASCII: a C string becomes a Godot String as Latin-1)
const char *STAGE_NAME[5] = { "Step 1: flood", "Step 2a: spills", "Step 2b: held or stream", "Step 2c: flow",
	"Steps 3 - 4: one cell" };
const char *CLICK_TEXT = "Left-click a cell on the map: steps 3 - 4 are shown for that cell (one with water's edge or a fall shows the most).";
} // namespace

void MCManager::_on_water_steps_toggle() {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid) {
		return;
	}
	water_steps_on = !water_steps_on;
	picking_trace_cell = false;
	if (water_steps_on) {
		// a pour with the Water tool just made: walk that one; else the whole map's step 1
		bool pour = grid->get_water_trace_kind() == 0 && grid->get_pour_step_count() > 0;
		_water_steps_go(0, false, !pour);
	} else {
		grid->set_pour_step(-1);
		grid->set_debug_water(false);
	}
	if (water_steps_button) {
		water_steps_button->set_text(water_steps_on ? "Stop water debug" : "Start water debug");
	}
	_update_water_steps_labels();
}

void MCManager::_on_water_steps_next() {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid || !water_steps_on || picking_trace_cell) {
		return; // (picking: the click goes on)
	}
	int step = grid->get_pour_step();
	if (step + 1 < grid->get_pour_step_count()) {
		grid->set_pour_step(step + 1);
	} else if (water_steps_stage < STAGE_CELL) {
		_water_steps_go(water_steps_stage + 1, false);
	} else {
		picking_trace_cell = true; // past the cell's last step: another cell
	}
	_update_water_steps_labels();
}

void MCManager::_on_water_steps_prev() {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid || !water_steps_on) {
		return;
	}
	int step = grid->get_pour_step();
	if (picking_trace_cell) {
		picking_trace_cell = false; // back to step 2c's last step
		_water_steps_go(STAGE_CELL - 1, true);
	} else if (step > 0) {
		grid->set_pour_step(step - 1);
	} else if (water_steps_stage > 0) {
		_water_steps_go(water_steps_stage - 1, true);
	}
	_update_water_steps_labels();
}

void MCManager::_water_steps_go(
		int p_stage,
		bool p_from_end,
		bool p_record
) {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid) {
		return;
	}
	int stage = std::clamp(p_stage, 0, STAGE_CELL);
	while (true) {
		water_steps_stage = stage;
		int count = 0;
		if (stage == 0) {
			count = p_record ? grid->trace_settle() : grid->get_pour_step_count();
			p_record = true; // only the first visit may keep the Water tool's pour
		} else if (stage < STAGE_CELL) {
			count = grid->trace_water_step(stage + 1);
		} else {
			picking_trace_cell = true; // steps 3 - 4 need their cell: the next left click on the map
			grid->set_pour_step(-1);
			_update_water_steps_labels();
			return;
		}
		if (count > 0) {
			grid->set_pour_step(p_from_end ? count - 1 : 0);
			break;
		}
		int next = stage + (p_from_end ? -1 : 1); // nothing in this stage: on to the next
		if (next < 0 || next > STAGE_CELL) {
			grid->set_pour_step(-1);
			break;
		}
		stage = next;
	}
	_update_water_steps_labels();
}

void MCManager::_trace_cell(
		MCGrid *p_grid,
		const Vector3 &p_local
) {
	picking_trace_cell = false;
	water_steps_cell = Vector2i(static_cast<int>(std::floor(p_local.x)), static_cast<int>(std::floor(p_local.z)));
	water_steps_stage = STAGE_CELL;
	int count = p_grid->trace_water_cell(water_steps_cell);
	p_grid->set_pour_step(count > 0 ? 0 : -1);
	_update_water_steps_labels();
}

void MCManager::_water_steps_after_pour(MCGrid *p_grid) {
	if (!water_steps_on) {
		return;
	}
	picking_trace_cell = false;
	water_steps_stage = 0; // the pour is step 1's trace now
	p_grid->set_pour_step(p_grid->get_pour_step_count() > 0 ? 0 : -1);
	_update_water_steps_labels();
}

bool MCManager::_water_steps_key(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key = p_event;
	if (!water_steps_on || key.is_null() || !key->is_pressed()) {
		return false;
	}
	if (key->get_keycode() == KEY_RIGHT) {
		_on_water_steps_next();
		return true;
	}
	if (key->get_keycode() == KEY_LEFT) {
		_on_water_steps_prev();
		return true;
	}
	return false;
}

void MCManager::_update_water_steps_labels() {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!water_steps_where || !pour_step_ui_label) {
		return;
	}
	if (!grid || !water_steps_on) {
		water_steps_where->set_text("Water debug is off");
		pour_step_ui_label->set_text("Start it to walk through how the water is built: step 1 (flood), 2a, 2b, 2c (per column),"
									 " then 3 - 4 for a cell you click. Prev / Next, or the left / right arrow keys.");
		return;
	}
	String stage = STAGE_NAME[std::clamp(water_steps_stage, 0, STAGE_CELL)];
	if (picking_trace_cell) {
		water_steps_where->set_text(stage + ", click a cell");
		pour_step_ui_label->set_text(CLICK_TEXT);
		return;
	}
	int count = grid->get_pour_step_count();
	int step = grid->get_pour_step();
	String where = stage;
	if (water_steps_stage == STAGE_CELL) {
		where += vformat(" (%d, %d)", water_steps_cell.x, water_steps_cell.y);
	}
	water_steps_where->set_text(step >= 0 ? vformat("%s  (%d / %d)", where, step + 1, count) : where);
	if (step < 0) {
		pour_step_ui_label->set_text("Nothing to show in this stage on this map.");
		return;
	}
	// the step's own text, without its "step i / n" (the label above says it)
	String text = grid->get_pour_step_text();
	if (text.begins_with("step ")) {
		int colon = text.find(": ");
		int line = text.find("\n");
		if (colon >= 0 && (line < 0 || colon < line)) {
			text = text.substr(colon + 2); // "step i / n: take (x, z) off the queue ..."
		} else {
			text = line >= 0 ? text.substr(line + 1) : String(); // "step i / n" alone on its line
		}
	}
	pour_step_ui_label->set_text(text);
}

} // namespace godot
