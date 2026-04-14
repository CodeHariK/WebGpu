#include "terrain.h"
#include "mc_physics.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

bool MCTerrain::is_corner_active(const Vector3i &p_grid_pos) const {
	int gx = p_grid_pos.x;
	int gy = p_grid_pos.y;
	int gz = p_grid_pos.z;

	for (const Chunk &chunk : chunks) {
		int start_x = chunk.loc_x * chunk.size_x;
		int end_x = start_x + chunk.size_x;
		int start_y = chunk.loc_y * chunk.size_y;
		int end_y = start_y + chunk.size_y;
		int start_z = chunk.loc_z * chunk.size_z;
		int end_z = start_z + chunk.size_z;

		if (gx >= start_x && gx <= end_x &&
				gy >= start_y && gy <= end_y &&
				gz >= start_z && gz <= end_z) {
			return chunk.get_corner(gx - start_x, gy - start_y, gz - start_z);
		}
	}
	return false;
}

bool MCTerrain::is_area_blocked_by_terrain(const Vector3i &p_dual_grid_pos, const Vector3i &p_size) const {
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

bool MCTerrain::is_area_blocked_by_objects(const AABB &p_aabb) const {
	return spatial.is_area_blocked(p_aabb);
}

void MCTerrain::add_placed_object(const Vector3i &p_dual_grid_pos, const Vector3i &p_size) {
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
	
	UtilityFunctions::print("MCTerrain: Placed object of size ", p_size, " at ", p_dual_grid_pos);
}

void MCTerrain::remove_placed_object(Node *p_node) {
	if (!p_node) return;
	spatial.remove_object_by_node(p_node);
	p_node->queue_free();
	UtilityFunctions::print("MCTerrain: Removed object node.");
}

void MCTerrain::detach_placed_object(Node *p_node) {
	if (!p_node) return;
	spatial.remove_object_by_node(p_node);
	UtilityFunctions::print("MCTerrain: Detached object node from spatial index.");
}

const PlacedObject* MCTerrain::get_placed_object(Node *p_node) const {
	if (!p_node) return nullptr;
	return spatial.get_object_by_node(p_node);
}

} // namespace godot
