#include "mc_spatial.h"

namespace godot {

void MCSpatial::add_object(const PlacedObject &p_obj) {
	objects.push_back(p_obj);
}

void MCSpatial::remove_object_at(const Vector3i &p_grid_pos) {
	for (auto it = objects.begin(); it != objects.end(); ++it) {
		if (it->grid_pos == p_grid_pos) {
			objects.erase(it);
			break;
		}
	}
}

void MCSpatial::remove_object_by_node(Object *p_node) {
	for (auto it = objects.begin(); it != objects.end(); ++it) {
		if (it->visual_node == p_node) {
			objects.erase(it);
			break;
		}
	}
}

bool MCSpatial::is_area_blocked(const AABB &p_aabb) const {
	AABB shrunk = p_aabb.grow(-0.01f);
	for (const auto &obj : objects) {
		if (obj.aabb.grow(-0.01f).intersects(shrunk)) {
			return true;
		}
	}
	return false;
}

const PlacedObject* MCSpatial::get_object_by_node(Object *p_node) const {
	for (const auto &obj : objects) {
		if (obj.visual_node == p_node) {
			return &obj;
		}
	}
	return nullptr;
}

void MCSpatial::clear() {
	objects.clear();
}

} // namespace godot
