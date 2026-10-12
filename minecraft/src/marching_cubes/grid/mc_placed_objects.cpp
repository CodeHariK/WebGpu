// MCPlacedObjects: list of placed boxes with overlap checks (see header).
#include "marching_cubes/grid/mc_placed_objects.h"

namespace godot {

// Append (no overlap check).
void MCPlacedObjects::add_object(const PlacedObject &p_obj) { objects.push_back(p_obj); }

// Remove the object whose lower cell is p_grid_pos.
void MCPlacedObjects::remove_object_at(const Vector3i &p_grid_pos) {
	for (auto it = objects.begin(); it != objects.end(); ++it) {
		if (it->grid_pos == p_grid_pos) {
			objects.erase(it);
			break;
		}
	}
}

// Remove the object drawn by p_node.
void MCPlacedObjects::remove_object_by_node(Object *p_node) {
	for (auto it = objects.begin(); it != objects.end(); ++it) {
		if (it->visual_node == p_node) {
			objects.erase(it);
			break;
		}
	}
}

// Does p_aabb overlap any object's box?
bool MCPlacedObjects::is_area_blocked(const AABB &p_aabb) const {
	AABB shrunk = p_aabb.grow(-0.01f);
	for (const auto &obj : objects) {
		if (obj.aabb.grow(-0.01f).intersects(shrunk)) {
			return true;
		}
	}
	return false;
}

// The object drawn by p_node, or nullptr.
const PlacedObject *MCPlacedObjects::get_object_by_node(Object *p_node) const {
	for (const auto &obj : objects) {
		if (obj.visual_node == p_node) {
			return &obj;
		}
	}
	return nullptr;
}

// Forget every object (their nodes are freed by MCGrid).
void MCPlacedObjects::clear() { objects.clear(); }

} // namespace godot
