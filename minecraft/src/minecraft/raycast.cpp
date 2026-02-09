#include "minecraft.h"

#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/vector3.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/mesh_instance3d.hpp>

#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>

void MinecraftNode::raycast() {
	Vector3 origin = camera->get_global_transform().origin;
	Vector3 direction = -camera->get_global_transform().basis.get_column(2); // forward
	Vector3 end = origin + direction * 100.0f;

	Ref<World3D> world = get_world_3d();
	if (world.is_null())
		return;

	PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
	if (!space_state)
		return;

	Ref<PhysicsRayQueryParameters3D> params = PhysicsRayQueryParameters3D::create(origin, end);
	Dictionary result = space_state->intersect_ray(params);

	if (!result.is_empty()) {
		Vector3 hit_pos = result["position"];
		Vector3 hit_normal = result["normal"];
		Object *collider = Object::cast_to<Object>(result["collider"]);

		UtilityFunctions::print("Hit at: ", hit_pos);
		UtilityFunctions::print("Normal: ", hit_normal);

		if (Node3D *node = Object::cast_to<Node3D>(collider)) {
			UtilityFunctions::print("Hit node: ", node->get_name());
			if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(node)) {
				Ref<Material> mat = mi->get_active_material(0);
				if (mat.is_valid()) {
					UtilityFunctions::print("Material: ", mat->get_name());
				}
			}
		}
	} else {
		UtilityFunctions::print("No hit");
	}
}