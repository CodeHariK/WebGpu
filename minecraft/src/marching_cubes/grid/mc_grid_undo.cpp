// MCGrid: undo / redo of terrain edits.
//
// An edit is whatever happens between begin_edit() and end_edit() (the editor wraps every click,
// Randomize and water fill in one): while it is open, every value about to change records its old
// value once — a column height and a water level (heightmap mode, the corners follow from the
// heights) or a corner (corner mode) — plus the decor seed and the M roofs as they were when it
// began. undo() writes those old values back while recording the current ones (that recording is
// the redo), then rebuilds the grid; redo() does the same the other way. Only the changed values are
// kept, so a click costs a few bytes and a Randomize one entry per column / corner.
// Changes made outside an edit (scripts calling set_column_height directly) aren't recorded.
// The history is cleared when the grid is re-initialised, loaded or switches heightmap mode, and
// keeps the last MAX_HISTORY edits. Placed objects and pins are not part of it.
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

namespace godot {

namespace {
const size_t MAX_HISTORY = 100;
} // namespace

// Start recording an edit (nested calls join the open one).
void MCGrid::begin_edit() {
	if (edit_depth++ > 0) {
		return;
	}
	recording = MCEdit();
	recording.decor_seed = decor_seed;
	recording.roof_m = roof_m;
	recorded_heights.clear();
	recorded_water.clear();
	recorded_corners.clear();
}

// Finish the edit: kept (and the redo history dropped) when it changed anything.
void MCGrid::end_edit() {
	if (edit_depth == 0 || --edit_depth > 0) {
		return;
	}
	bool changed = !recording.heights.empty() || !recording.water.empty() || !recording.corners.empty() ||
			recording.decor_seed != decor_seed;
	if (!changed) {
		return;
	}
	undo_stack.push_back(std::move(recording));
	if (undo_stack.size() > MAX_HISTORY) {
		undo_stack.erase(undo_stack.begin());
	}
	redo_stack.clear();
}

// Forget every edit (the grid changed under the history: re-initialised, loaded, mode switch).
void MCGrid::clear_history() {
	undo_stack.clear();
	redo_stack.clear();
}

bool MCGrid::can_undo() const { return !undo_stack.empty(); }

bool MCGrid::can_redo() const { return !redo_stack.empty(); }

// The old values of one recorded change, the first time it changes in this edit.
void MCGrid::_record_height(int p_index) {
	if (edit_depth > 0 && recorded_heights.insert(p_index).second) {
		recording.heights.emplace_back(p_index, column_heights[static_cast<size_t>(p_index)]);
	}
}

void MCGrid::_record_water(int p_index) {
	if (edit_depth > 0 && recorded_water.insert(p_index).second) {
		recording.water.emplace_back(p_index, water_levels[static_cast<size_t>(p_index)]);
	}
}

void MCGrid::_record_corner(const Vector3i &p_corner) {
	if (edit_depth == 0 || heightmap_mode) {
		return; // heightmap mode: the heights are recorded, the corners follow from them
	}
	Vector3i size = _dims().max_corner() + Vector3i(1, 1, 1);
	int key = p_corner.x + size.x * (p_corner.y + size.y * p_corner.z);
	if (recorded_corners.insert(key).second) {
		recording.corners.emplace_back(key, is_corner_active(p_corner) ? 1 : 0);
	}
}

// Write an edit's old values back, recording the current ones into r_inverse; then rebuild.
void MCGrid::_apply_edit(
		const MCEdit &p_edit,
		MCEdit &r_inverse
) {
	edit_depth = 0;
	begin_edit();
	Vector2i columns = _column_counts();
	for (const auto &[index, height] : p_edit.heights) {
		_record_height(index);
		column_heights[static_cast<size_t>(index)] = height;
		_write_column(index % columns.x, index / columns.x, height);
	}
	for (const auto &[index, level] : p_edit.water) {
		_record_water(index);
		water_levels[static_cast<size_t>(index)] = level;
	}
	Vector3i size = _dims().max_corner() + Vector3i(1, 1, 1);
	for (const auto &[key, solid] : p_edit.corners) {
		Vector3i corner(key % size.x, (key / size.x) % size.y, key / (size.x * size.y));
		_record_corner(corner);
		_write_corner(corner, solid != 0);
	}
	decor_seed = p_edit.decor_seed;
	roof_m = p_edit.roof_m;
	r_inverse = std::move(recording);
	edit_depth = 0;
	refresh_grid(); // levels, groups, every chunk, water
}

// Undo the last edit; false when there is none.
bool MCGrid::undo() {
	if (undo_stack.empty() || edit_depth > 0) {
		return false;
	}
	MCEdit edit = std::move(undo_stack.back());
	undo_stack.pop_back();
	MCEdit inverse;
	_apply_edit(edit, inverse);
	redo_stack.push_back(std::move(inverse));
	return true;
}

// Redo the last undone edit; false when there is none.
bool MCGrid::redo() {
	if (redo_stack.empty() || edit_depth > 0) {
		return false;
	}
	MCEdit edit = std::move(redo_stack.back());
	redo_stack.pop_back();
	MCEdit inverse;
	_apply_edit(edit, inverse);
	undo_stack.push_back(std::move(inverse));
	return true;
}

} // namespace godot
