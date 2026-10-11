// MCManager: mouse clicks. Raycast once into a ClickHit, then hand it to the current mode:
// terrain edits live here, object placing / selecting / dragging in mc_object_mode.cpp.
#include "marching_cubes/editor/mc_manager.h"
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/util/mc_physics.h"
#include "utils/raycast/mc_raycast.h"
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>

namespace godot {

void MCManager::_unhandled_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mouse_event = p_event;
	if (mouse_event.is_null()) {
		return;
	}
	MouseButton button = mouse_event->get_button_index();
	if (button != MOUSE_BUTTON_LEFT && button != MOUSE_BUTTON_RIGHT) {
		return;
	}
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(terrain.path));
	if (!grid) {
		return;
	}

	// A release only matters to end a drag, and only the left button's release does that.
	if (!mouse_event->is_pressed()) {
		if (is_dragging && button == MOUSE_BUTTON_LEFT) {
			_finish_drag(grid);
		}
		return;
	}

	ClickHit hit;
	if (!_raycast_click(p_event, grid, hit)) {
		return;
	}

	switch (interaction_mode) {
		case MODE_TERRAIN:
			_terrain_click(grid, hit, button);
			break;
		case MODE_PLACE_OBJECT:
			if (button == MOUSE_BUTTON_LEFT) {
				_place_click(grid, hit);
			} else if (hit.object) {
				_delete_object(grid, hit.object);
			}
			break;
		case MODE_DRAG_OBJECT:
			if (button == MOUSE_BUTTON_LEFT) {
				if (!is_dragging) {
					_select_or_start_drag(grid, hit, mouse_event->is_shift_pressed());
				}
			} else if (hit.object) {
				_delete_object(grid, hit.object);
			}
			break;
	}
}

TypedArray<RID> MCManager::_drag_group_exclusions() const {
	TypedArray<RID> exclude;
	if (!is_dragging) {
		return exclude;
	}
	for (const SelectedObject &obj : drag_group) {
		if (!obj.node) {
			continue;
		}
		TypedArray<Node> children = obj.node->get_children();
		for (int i = 0; i < children.size(); i++) {
			CollisionObject3D *co = Object::cast_to<CollisionObject3D>(children[i]);
			if (co) {
				exclude.push_back(co->get_rid());
			}
		}
	}
	return exclude;
}

bool MCManager::_raycast_click(
		const Ref<InputEvent> &p_event,
		MCGrid *p_grid,
		ClickHit &r_hit
) {
	// Terrain mode edits corners only: hitting a placed object must not edit the corner under it.
	uint32_t mask = interaction_mode == MODE_TERRAIN ? toLayer(LAYER_CORNERS)
													 : (toLayer(LAYER_OBJECTS) | toLayer(LAYER_CORNERS));
	MCRaycastHit hit = raycast_from_event(this, p_event, mask, 512, _drag_group_exclusions());
	if (!hit.is_hit) {
		return false;
	}
	Node3D *collider = Object::cast_to<Node3D>(hit.collider);
	if (!collider) {
		return false;
	}

	// Grid coordinates are local to MCGrid: convert the hit so a moved/rotated grid still works.
	r_hit.position = p_grid->to_local(hit.position);
	r_hit.normal = (p_grid->get_global_transform().basis.inverse().xform(hit.normal)).normalized();

	CollisionObject3D *body = Object::cast_to<CollisionObject3D>(collider);
	if (body && (body->get_collision_layer() & toLayer(LAYER_OBJECTS))) {
		// A placed object: the collision body is a child of the object's MeshInstance3D.
		r_hit.object = Object::cast_to<MeshInstance3D>(collider->get_parent());
	} else if (Node3D *cube = Object::cast_to<Node3D>(collider->get_parent())) {
		// A debug corner cube: its position is the corner.
		r_hit.corner = Vector3i(cube->get_position().round());
	}
	return true;
}

void MCManager::_terrain_click(
		MCGrid *p_grid,
		const ClickHit &p_hit,
		MouseButton p_button
) {
	if (p_button == MOUSE_BUTTON_LEFT) {
		// Add: fill the corner next to the clicked one, on the side that was clicked.
		Vector3i side(Math::round(p_hit.normal.x), Math::round(p_hit.normal.y), Math::round(p_hit.normal.z));
		p_grid->modify_corner(p_hit.corner + side, true);
	} else {
		// Remove: empty the clicked corner.
		p_grid->modify_corner(p_hit.corner, false);
	}
}

} // namespace godot
