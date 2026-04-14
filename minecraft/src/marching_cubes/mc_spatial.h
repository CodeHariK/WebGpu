#ifndef MC_SPATIAL_H
#define MC_SPATIAL_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <vector>

namespace godot {

/**
 * Represents an object placed on the Dual Grid.
 */
struct PlacedObject {
	Vector3i grid_pos; // Dual Grid corner (cell center integer coord)
	Vector3i size;
	AABB aabb;
	Object *visual_node = nullptr;
};

/**
 * Simple Spatial Manager for Dual Grid objects.
 * Uses a list-based approach (Simple BVH / Spatial Partitioning).
 */
class MCSpatial {
private:
	std::vector<PlacedObject> objects;

public:
	void add_object(const PlacedObject &p_obj);
	void remove_object_at(const Vector3i &p_grid_pos);
	void remove_object_by_node(Object *p_node);
	
	bool is_area_blocked(const AABB &p_aabb) const;
	const PlacedObject* get_object_by_node(Object *p_node) const;
	const std::vector<PlacedObject>& get_objects() const { return objects; }
	
	void clear();
};

} // namespace godot

#endif // MC_SPATIAL_H
