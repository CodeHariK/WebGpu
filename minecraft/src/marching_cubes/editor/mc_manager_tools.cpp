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

// Step through the last pour (water debug): . next step, , back (Shift: 10 steps), / the whole pour.
// The first . starts at step 1. True when the key was used.
bool MCManager::_pour_step_shortcut(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key = p_event;
	if (key.is_null() || !key->is_pressed() || key->is_command_or_control_pressed()) {
		return false;
	}
	Key code = key->get_keycode();
	if (code != KEY_PERIOD && code != KEY_COMMA && code != KEY_SLASH) {
		return false;
	}
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid || grid->get_pour_step_count() == 0) {
		return false;
	}
	int stride = key->is_shift_pressed() ? 10 : 1;
	int step = grid->get_pour_step();
	if (code == KEY_SLASH) {
		step = -1;
	} else if (code == KEY_PERIOD) {
		step = step < 0 ? 0 : step + stride;
	} else {
		step = step < 0 ? grid->get_pour_step_count() - 1 : std::max(step - stride, 0);
	}
	grid->set_pour_step(step);
	return true;
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
