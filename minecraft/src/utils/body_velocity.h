#ifndef BODY_VELOCITY_H
#define BODY_VELOCITY_H

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/// Linear velocity of any moving body (RigidBody3D or CharacterBody3D); zero otherwise.
/// Used by anything that tracks a target: follow cameras, homing projectiles.
inline Vector3 body_velocity(Node *p_node) {
	if (RigidBody3D *rb = Object::cast_to<RigidBody3D>(p_node)) {
		return rb->get_linear_velocity();
	}
	if (CharacterBody3D *cb = Object::cast_to<CharacterBody3D>(p_node)) {
		return cb->get_velocity();
	}
	return Vector3();
}

} // namespace godot

#endif // BODY_VELOCITY_H
