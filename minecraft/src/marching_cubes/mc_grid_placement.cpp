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
	int cx = (p_grid_pos.x >= grid_size.x * chunk_size.x) ? grid_size.x - 1 : p_grid_pos.x / chunk_size.x;
	int cy = (p_grid_pos.y >= grid_size.y * chunk_size.y) ? grid_size.y - 1 : p_grid_pos.y / chunk_size.y;
	int cz = (p_grid_pos.z >= grid_size.z * chunk_size.z) ? grid_size.z - 1 : p_grid_pos.z / chunk_size.z;

	if (cx < 0 || cx >= grid_size.x || cy < 0 || cy >= grid_size.y || cz < 0 || cz >= grid_size.z) {
		return false;
	}

	int idx = _get_chunk_index(cx, cy, cz);
	return chunks[idx].get_corner(p_grid_pos.x - (cx * chunk_size.x), p_grid_pos.y - (cy * chunk_size.y), p_grid_pos.z - (cz * chunk_size.z));
}

bool MCGrid::is_area_blocked_by_grid(const Vector3i &p_dual_grid_pos, const Vector3i &p_size) const {
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

bool MCGrid::is_area_blocked_by_objects(const AABB &p_aabb) const {
	return spatial.is_area_blocked(p_aabb);
}

void MCGrid::add_placed_object(const Vector3i &p_dual_grid_pos, const Vector3i &p_size) {
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
	StaticBody3D *sb = MCPhysics::create_static_box_collider(
			mi,
			LAYER_OBJECTS,
			Vector3(p_size));

	obj.visual_node = mi;
	spatial.add_object(obj);

	// Add a simple material to distinguish it from terrain
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_albedo(Color(0.2f, 0.6f, 1.0f)); // Light blue
	mi->set_material_override(mat);

	add_child(mi);
	if (is_inside_tree()) {
		mi->set_owner(get_owner() ? get_owner() : this);
	}

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

const PlacedObject *MCGrid::get_placed_object(Node *p_node) const {
	if (!p_node)
		return nullptr;
	return spatial.get_object_by_node(p_node);
}

} // namespace godot
