// MCManager: the tool bar (top of the screen, next to Stats / Help) and undo / redo.
//
// One tool is active at a time and decides what a click does (like Animal Crossing's Island
// Designer tools):
//   Terrain  corner mode: left fills the corner beside the clicked face, right empties the clicked
//            corner; heightmap mode: whole columns rise / drop (MCGrid::set_corner)
//   Water    heightmap mode: left fills the clicked column's hole one tier deep (fill_water), right
//            drains that body of water (clear_water)
//   Place    left places a box, right deletes one (mc_object_mode.cpp)
//   Move     left drags boxes (shift: add to the selection), right deletes
// Undo / Redo buttons and Cmd/Ctrl+Z, Cmd/Ctrl+Shift+Z (or Cmd/Ctrl+Y): MCGrid's history
// (grid/mc_grid_undo.cpp). Every terrain / water click and every Randomize is one step.
#include "cui/cui.h"
#include "marching_cubes/editor/mc_manager.h"
#include <algorithm>
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/button_group.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

namespace {
const char *TOOL_NAMES[] = { "Terrain", "Place", "Move", "Water" }; // InteractionMode order
} // namespace

// The tool buttons (one pressed at a time) and Undo / Redo, in the top bar.
void MCManager::_setup_tool_bar(HBoxContainer *p_bar) {
	Ref<ButtonGroup> group;
	group.instantiate();
	const InteractionMode order[] = { MODE_TERRAIN, MODE_WATER, MODE_PLACE_OBJECT, MODE_DRAG_OBJECT };
	for (InteractionMode mode : order) {
		Button *button = ui.manager->add_button(
				p_bar, TOOL_NAMES[mode], Callable(this, "_on_select_tool").bind(static_cast<int>(mode))
		);
		button->set_toggle_mode(true);
		button->set_button_group(group);
		button->set_pressed_no_signal(mode == interaction_mode);
		button->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
		button->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
		button->set_custom_minimum_size(Vector2(80, 40));
		tool_buttons[mode] = button;
	}
	_mark_active_tool();
	const char *labels[] = { "Undo", "Redo" };
	const char *methods[] = { "_on_undo", "_on_redo" };
	for (int i = 0; i < 2; i++) {
		Button *button = ui.manager->add_button(p_bar, labels[i], Callable(this, methods[i]));
		button->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
		button->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
		button->set_custom_minimum_size(Vector2(70, 40));
		(i == 0 ? undo_button : redo_button) = button;
	}
	_update_history_buttons();
}

// Make a tool the active one (finishes any drag first).
void MCManager::_on_select_tool(int p_mode) {
	cancel_drag();
	interaction_mode = static_cast<InteractionMode>(p_mode);
	_mark_active_tool();
	UtilityFunctions::print("MCManager: tool ", TOOL_NAMES[interaction_mode]);
	update_ui();
}

// The active tool's button pressed and labelled "● Name" (the theme has no pressed look).
void MCManager::_mark_active_tool() {
	for (int mode = 0; mode < 4; mode++) {
		if (!tool_buttons[mode]) {
			continue;
		}
		bool active = mode == interaction_mode;
		tool_buttons[mode]->set_pressed_no_signal(active);
		tool_buttons[mode]->set_text((active ? String::utf8("● ") : String()) + TOOL_NAMES[mode]);
	}
}

void MCManager::_on_undo() {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid) {
		cancel_drag();
		grid->undo();
	}
	_update_history_buttons();
}

void MCManager::_on_redo() {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid) {
		cancel_drag();
		grid->redo();
	}
	_update_history_buttons();
}

// Undo / Redo greyed out when there is nothing to undo / redo.
void MCManager::_update_history_buttons() {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (undo_button) {
		undo_button->set_disabled(!grid || !grid->can_undo());
	}
	if (redo_button) {
		redo_button->set_disabled(!grid || !grid->can_redo());
	}
}

// Cmd/Ctrl+Z undo, Cmd/Ctrl+Shift+Z or Cmd/Ctrl+Y redo; true when the key was used.
bool MCManager::_history_shortcut(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key = p_event;
	if (key.is_null() || !key->is_pressed() || key->is_echo() || !key->is_command_or_control_pressed()) {
		return false;
	}
	if (key->get_keycode() == KEY_Z && !key->is_shift_pressed()) {
		_on_undo();
		return true;
	}
	if ((key->get_keycode() == KEY_Z && key->is_shift_pressed()) || key->get_keycode() == KEY_Y) {
		_on_redo();
		return true;
	}
	return false;
}

// The pour, step by step (Debug tab buttons): p_stride steps forward (> 0) or back (< 0) through
// the last pour's trace, 0 for the whole pour again. From the whole pour, Next starts at step 1
// and Back at the last step. With no trace yet (a loaded map), stepping first records step 1 for
// the whole map (MCGrid::trace_settle). The label under the buttons says where it is.
void MCManager::_step_pour(int p_stride) {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	int count = grid ? grid->get_pour_step_count() : 0;
	if (grid && count == 0 && p_stride != 0 && grid->get_water_trace_kind() == 0) {
		count = grid->trace_settle(); // no pour yet: step 1 for the whole map
	}
	if (grid && count > 0) {
		int step = grid->get_pour_step();
		if (p_stride == 0) {
			step = -1;
		} else if (p_stride > 0) {
			step = step < 0 ? 0 : std::min(step + p_stride, count - 1);
		} else {
			step = step < 0 ? count - 1 : std::max(step + p_stride, 0);
		}
		grid->set_pour_step(step);
	}
	_update_pour_label();
}

void MCManager::_update_pour_label() {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	int count = grid ? grid->get_pour_step_count() : 0;
	if (pour_step_ui_label) {
		if (count == 0 && grid && grid->get_water_trace_kind() != 0) {
			pour_step_ui_label->set_text("Nothing to step through here: this part of step 2 made no decisions on this map.");
		} else if (count == 0) {
			pour_step_ui_label->set_text("Next / Back step through step 1 for the whole map (or pour with the Water tool to step through that pour).");
		} else if (grid->get_pour_step() < 0) {
			const char *names[5] = { "step 1 (the pour)", "", "step 2a (spills)", "step 2b (held / stream)", "step 2c (flow)" };
			int kind = std::clamp(grid->get_water_trace_kind(), 0, 4);
			pour_step_ui_label->set_text(vformat("%s: %d steps. Next / Back step through it.", names[kind], count));
		} else {
			pour_step_ui_label->set_text(grid->get_pour_step_text()); // the same text floats over the column
		}
	}
}

void MCManager::_on_pour_back() { _step_pour(-1); }
void MCManager::_on_pour_next() { _step_pour(1); }
void MCManager::_on_pour_back_10() { _step_pour(-10); }
void MCManager::_on_pour_next_10() { _step_pour(10); }
void MCManager::_on_pour_whole() { _step_pour(0); }
// Trace step 2a / 2b / 2c buttons: record that part of step 2 (MCGrid::trace_water_step) and show
// its first step; Back / Next then go through it.
void MCManager::_trace_water(int p_kind) {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid) {
		grid->trace_water_step(p_kind);
	}
	_step_pour(1);
}

void MCManager::_on_trace_spills() { _trace_water(2); }
void MCManager::_on_trace_held() { _trace_water(3); }
void MCManager::_on_trace_flow() { _trace_water(4); }

// Trace step 1 button: record step 1 for the whole map again (replacing the last pour's trace).
void MCManager::_on_trace_settle() {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid) {
		grid->trace_settle();
	}
	_step_pour(1); // show its first step
}

// Water tool (heightmap mode): left fills the clicked column's hole one tier deep, right drains the
// body of water there. One undo step.
void MCManager::_water_click(
		MCGrid *p_grid,
		const ClickHit &p_hit,
		MouseButton p_button
) {
	if (!p_grid->get_heightmap_mode()) {
		UtilityFunctions::print("MCManager: the Water tool works in heightmap mode");
		return;
	}
	Vector2i column(p_hit.corner.x, p_hit.corner.z);
	p_grid->begin_edit();
	if (p_button == MOUSE_BUTTON_LEFT) {
		// pour: it flows over everything reachable not higher than here, one layer deep
		p_grid->fill_water(column, p_grid->get_column_height(column) + 1);
	} else {
		p_grid->clear_water(column);
	}
	p_grid->end_edit();
	_update_history_buttons();
}

} // namespace godot
