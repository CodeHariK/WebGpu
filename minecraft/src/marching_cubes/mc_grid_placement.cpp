#include "mc_grid.h"
#include "mc_physics.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

bool MCGrid::is_corner_active(const Vector3i &p_grid_pos) const {
	int chunk_index = 0;
	Vector3i local;
	if (!_corner_to_local(p_grid_pos, chunk_index, local)) {
		return false; // outside the grid counts as empty
	}
	return chunks[static_cast<size_t>(chunk_index)].get_corner(local.x, local.y, local.z);
}

bool MCGrid::is_area_blocked_by_grid(
		const Vector3i &p_dual_grid_pos,
		const Vector3i &p_size
) const {
	for (int y = 0; y <= p_size.y; ++y) {
		for (int z = 0; z <= p_size.z; ++z) {
			for (int x = 0; x <= p_size.x; ++x) {
				Vector3i corner_pos = p_dual_grid_pos + Vector3i(x, y, z);
				if (is_corner_active(corner_pos)) {
					return true;
				}
			}
		}
	}
	return false;
}

bool MCGrid::is_area_blocked_by_objects(const AABB &p_aabb) const { return spatial.is_area_blocked(p_aabb); }

void MCGrid::add_placed_object(
		const Vector3i &p_dual_grid_pos,
		const Vector3i &p_size
) {
	PlacedObject obj;
	obj.grid_pos = p_dual_grid_pos;
	obj.size = p_size;

	Vector3 pos = Vector3(p_dual_grid_pos) + (Vector3(p_size) * 0.5f);
	obj.aabb = AABB(Vector3(p_dual_grid_pos), Vector3(p_size));

	// Visual instantiation
	MeshInstance3D *mi = memnew(MeshInstance3D);
	Ref<BoxMesh> box;
	box.instantiate();
	box->set_size(Vector3(p_size));
	mi->set_mesh(box);
	mi->set_position(pos);
	mi->set_name("PlacedObject_" + String::num_int64(p_dual_grid_pos.x) + "_" + String::num_int64(p_dual_grid_pos.y));

	// Collision instantiation
	MCPhysics::create_static_box_collider(mi, toLayer(LAYER_OBJECTS), Vector3(p_size));

	obj.visual_node = mi;
	spatial.add_object(obj);

	// One shared light-blue material distinguishes placed objects from terrain. Highlights go
	// in material_overlay, so this override is never lost.
	if (placed_object_material.is_null()) {
		placed_object_material.instantiate();
		placed_object_material->set_albedo(Color(0.2f, 0.6f, 1.0f));
	}
	mi->set_material_override(placed_object_material);

	if (!placed_objects_container) {
		placed_objects_container = memnew(Node3D);
		placed_objects_container->set_name("PlacedObjects");
		add_child(placed_objects_container);
	}
	placed_objects_container->add_child(mi);

	UtilityFunctions::print("MCGrid: Placed object of size ", p_size, " at ", p_dual_grid_pos);
}

void MCGrid::remove_placed_object(Node *p_node) {
	if (!p_node)
		return;
	spatial.remove_object_by_node(p_node);
	p_node->queue_free();
	UtilityFunctions::print("MCGrid: Removed object node.");
}

void MCGrid::detach_placed_object(Node *p_node) {
	if (!p_node)
		return;
	spatial.remove_object_by_node(p_node);
	UtilityFunctions::print("MCGrid: Detached object node from spatial index.");
}

void MCGrid::_clear_placed_objects() {
	spatial.clear();
	if (!placed_objects_container) {
		return;
	}
	TypedArray<Node> children = placed_objects_container->get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		if (child) {
			placed_objects_container->remove_child(child);
			child->queue_free();
		}
	}
}

const PlacedObject *MCGrid::get_placed_object(Node *p_node) const {
	if (!p_node)
		return nullptr;
	return spatial.get_object_by_node(p_node);
}

} // namespace godot
