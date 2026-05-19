#include "physics_pusher.h"
#include "godot_cpp/core/defs.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/kinematic_collision3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>

namespace godot {

void PhysicsPusher::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_push_force", "force"), &PhysicsPusher::set_push_force);
	ClassDB::bind_method(D_METHOD("get_push_force"), &PhysicsPusher::get_push_force);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "push_force"), "set_push_force", "get_push_force");
}

PhysicsPusher::PhysicsPusher() {}
PhysicsPusher::~PhysicsPusher() {}

void PhysicsPusher::_physics_process(double delta) {
	CharacterBody3D *body = Object::cast_to<CharacterBody3D>(get_parent());
	if (!body)
		return;

	// Iterate through all collisions that happened during move_and_slide()
	for (int i = 0; i < body->get_slide_collision_count(); i++) {
		Ref<KinematicCollision3D> col = body->get_slide_collision(i);
		RigidBody3D *rb = Object::cast_to<RigidBody3D>(col->get_collider());

		if (rb) {
			// Apply an impulse in the opposite direction of the collision normal
			Vector3 push_dir = -col->get_normal();

			// Scale impulse proportionally to target's mass, capped at a maximum of 10.0
			float dynamic_force = push_force * MIN(rb->get_mass(), 1.0f);

			// We apply the impulse at the collision point for more realistic physics
			// (e.g. hitting a door at the edge will swing it more than hitting the hinge)
			Vector3 local_hit_pos = col->get_position() - rb->get_global_position();
			rb->apply_impulse(push_dir * dynamic_force, local_hit_pos);
		}
	}
}

void PhysicsPusher::set_push_force(float p_force) { push_force = p_force; }
float PhysicsPusher::get_push_force() const { return push_force; }

} // namespace godot
