// MCPlacedObjects: the boxes placed on an MCGrid (editor object mode), kept in a plain list with
// overlap checks. A list is enough for the few hundred objects an editor scene has; swap in a
// spatial hash if that ever grows.
#ifndef MC_PLACED_OBJECTS_H
#define MC_PLACED_OBJECTS_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <vector>

namespace godot {

// One box placed on the grid.
struct PlacedObject {
	Vector3i grid_pos; // lower cell it occupies (grid coordinates)
	Vector3i size; // cells it covers per axis
	AABB aabb; // grid-space box, for overlap checks
	Object *visual_node = nullptr; // its MeshInstance3D
};

class MCPlacedObjects {
private:
	std::vector<PlacedObject> objects;

public:
	// Add an object (no overlap check: use is_area_blocked first).
	void add_object(const PlacedObject &p_obj);
	// Remove the object whose lower cell is p_grid_pos.
	void remove_object_at(const Vector3i &p_grid_pos);
	// Remove the object drawn by p_node.
	void remove_object_by_node(Object *p_node);

	// Does p_aabb (grid space) overlap any object?
	bool is_area_blocked(const AABB &p_aabb) const;
	// The object drawn by p_node, or nullptr.
	const PlacedObject *get_object_by_node(Object *p_node) const;
	// Every placed object, in placement order.
	const std::vector<PlacedObject> &get_objects() const { return objects; }

	// Forget every object (does not free their nodes).
	void clear();
};

} // namespace godot

#endif // MC_PLACED_OBJECTS_H
