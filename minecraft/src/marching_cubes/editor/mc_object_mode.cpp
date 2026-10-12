// MCManager: placed objects - place on click, select (shift toggles), drag the selection,
// delete, and finish / cancel a drag. Called from _unhandled_input (mc_manager_input.cpp).
//
// Dragging: at drag start every selected object is detached from the grid's spatial index
// (so it doesn't block itself) and remembered with its offset from the clicked "pivot".
// The hover code (mc_manager_hover.cpp) moves them each frame; releasing re-places them,
// or puts them back where they started if the drop spot is blocked.
#include "marching_cubes/editor/mc_manager.h"
#include "marching_cubes/grid/mc_grid.h"
#include <algorithm>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// Place mode: put a box of the current size next to the hit face, unless blocked.
void MCManager::_place_click(
		MCGrid *p_grid,
		const ClickHit &p_hit
) {
	// The cell just outside the clicked surface.
	Vector3i place_pos = Vector3i((p_hit.position + p_hit.normal * 0.5f).floor());

	bool blocked = p_grid->is_area_blocked_by_grid(place_pos, current_placement_size) ||
			p_grid->is_area_blocked_by_objects(AABB(Vector3(place_pos), Vector3(current_placement_size)));
	if (blocked) {
		UtilityFunctions::print("MCManager: Cannot place object, area blocked.");
		return;
	}
	p_grid->add_placed_object(place_pos, current_placement_size);
}

// Drag mode press: shift toggles an object in the selection; a plain click on an object starts
// dragging the selection; a click on nothing clears it.
void MCManager::_select_or_start_drag(
		MCGrid *p_grid,
		const ClickHit &p_hit,
		bool p_shift
) {
	MeshInstance3D *clicked = p_hit.object;
	if (!clicked) {
		if (!p_shift) {
			_clear_selection(); // clicked empty space
		}
		return;
	}

	auto it = std::find(selected_nodes.begin(), selected_nodes.end(), clicked);
	bool is_selected = it != selected_nodes.end();

	if (p_shift) { // toggle this object in the selection
		if (is_selected) {
			clicked->set_material_overlay(nullptr);
			selected_nodes.erase(it);
		} else {
			selected_nodes.push_back(clicked);
			clicked->set_material_overlay(hover_mat_cyan);
		}
		return;
	}

	if (!is_selected) { // a plain click on an unselected object selects only it
		_clear_selection();
		selected_nodes.push_back(clicked);
		clicked->set_material_overlay(hover_mat_cyan);
	}
	_start_drag(p_grid, clicked);
}

// Detach the selected objects from the grid and remember their offsets from the pivot.
void MCManager::_start_drag(
		MCGrid *p_grid,
		MeshInstance3D *p_pivot
) {
	is_dragging = true;
	drag_group.clear();
	const PlacedObject *pivot = p_grid->get_placed_object(p_pivot);
	if (!pivot) {
		return;
	}
	Vector3i pivot_pos = pivot->grid_pos;
	for (MeshInstance3D *node : selected_nodes) {
		const PlacedObject *obj = p_grid->get_placed_object(node);
		if (!obj) {
			continue;
		}
		SelectedObject so;
		so.node = node;
		so.original_grid_pos = obj->grid_pos;
		so.size = obj->size;
		so.relative_offset = obj->grid_pos - pivot_pos;
		drag_group.push_back(so);
		p_grid->detach_placed_object(node);
	}
}

// Re-place the dragged objects at their new cells, or back at the start if blocked.
void MCManager::_finish_drag(MCGrid *p_grid) {
	// Re-place every dragged object: where it was dropped if the drop is valid, else where it
	// started. (add_placed_object makes a fresh node, so the dragged one is freed.)
	for (const SelectedObject &obj : drag_group) {
		Vector3i target = obj.original_grid_pos;
		if (drag_valid) {
			Vector3 visual_pos = obj.node->get_position();
			target = Vector3i((visual_pos - Vector3(obj.size) * 0.5f).round());
		}
		p_grid->add_placed_object(target, obj.size);
		obj.node->queue_free();
	}
	is_dragging = false;
	drag_group.clear();
	selected_nodes.clear(); // the old nodes are gone; selection highlights reset
}

// Delete a placed object; if it was being dragged, the rest of the group goes back first.
void MCManager::_delete_object(
		MCGrid *p_grid,
		MeshInstance3D *p_object
) {
	// Deleting a member of the group being dragged: put the OTHER members back where they came
	// from (they were detached from the spatial index at drag start).
	if (is_dragging) {
		bool in_group = std::any_of(drag_group.begin(), drag_group.end(), [&](const SelectedObject &o) {
			return o.node == p_object;
		});
		if (in_group) {
			for (const SelectedObject &obj : drag_group) {
				if (obj.node && obj.node != p_object) {
					p_grid->add_placed_object(obj.original_grid_pos, obj.size);
					obj.node->queue_free();
				}
			}
			is_dragging = false;
			drag_group.clear();
			selected_nodes.clear();
			UtilityFunctions::print("MCManager: Multi-drag cancelled due to deletion.");
		}
	}

	auto it = std::find(selected_nodes.begin(), selected_nodes.end(), p_object);
	if (it != selected_nodes.end()) {
		selected_nodes.erase(it);
	}
	p_grid->remove_placed_object(p_object);
}

// Un-highlight and forget the selected objects.
void MCManager::_clear_selection() {
	for (MeshInstance3D *node : selected_nodes) {
		if (node) {
			node->set_material_overlay(nullptr);
		}
	}
	selected_nodes.clear();
}

// Put every dragged object back where the drag started.
void MCManager::cancel_drag() {
	if (!is_dragging) {
		drag_group.clear();
		return;
	}

	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid) {
		for (const SelectedObject &obj : drag_group) {
			grid->add_placed_object(obj.original_grid_pos, obj.size);
			if (obj.node) {
				obj.node->queue_free();
			}
		}
		UtilityFunctions::print("MCManager: Multi-drag cancelled, all objects restored.");
	}

	is_dragging = false;
	drag_group.clear();
	selected_nodes.clear();
}

} // namespace godot
