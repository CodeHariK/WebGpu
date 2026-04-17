#include "physics_character.h"
#include "../game_manager/game_manager.h"
#include "../utils/raycast/mc_raycast.h"
#include "ai/character_states.h"
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void PhysicsCharacter3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_ride_height", "p_val"), &PhysicsCharacter3D::set_ride_height);
	ClassDB::bind_method(D_METHOD("get_ride_height"), &PhysicsCharacter3D::get_ride_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ride_height"), "set_ride_height", "get_ride_height");

	ClassDB::bind_method(D_METHOD("set_spring_stiffness", "p_val"), &PhysicsCharacter3D::set_spring_stiffness);
	ClassDB::bind_method(D_METHOD("get_spring_stiffness"), &PhysicsCharacter3D::get_spring_stiffness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spring_stiffness"), "set_spring_stiffness", "get_spring_stiffness");

	ClassDB::bind_method(D_METHOD("set_spring_damping", "p_val"), &PhysicsCharacter3D::set_spring_damping);
	ClassDB::bind_method(D_METHOD("get_spring_damping"), &PhysicsCharacter3D::get_spring_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spring_damping"), "set_spring_damping", "get_spring_damping");

	ClassDB::bind_method(D_METHOD("set_acceleration", "p_val"), &PhysicsCharacter3D::set_acceleration);
	ClassDB::bind_method(D_METHOD("get_acceleration"), &PhysicsCharacter3D::get_acceleration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "acceleration"), "set_acceleration", "get_acceleration");

	ClassDB::bind_method(D_METHOD("set_max_speed", "p_val"), &PhysicsCharacter3D::set_max_speed);
	ClassDB::bind_method(D_METHOD("get_max_speed"), &PhysicsCharacter3D::get_max_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_speed"), "set_max_speed", "get_max_speed");

	ClassDB::bind_method(D_METHOD("set_jump_impulse", "p_val"), &PhysicsCharacter3D::set_jump_impulse);
	ClassDB::bind_method(D_METHOD("get_jump_impulse"), &PhysicsCharacter3D::get_jump_impulse);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_impulse"), "set_jump_impulse", "get_jump_impulse");

	ClassDB::bind_method(D_METHOD("set_kick_speed", "p_val"), &PhysicsCharacter3D::set_kick_speed);
	ClassDB::bind_method(D_METHOD("get_kick_speed"), &PhysicsCharacter3D::get_kick_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "kick_speed"), "set_kick_speed", "get_kick_speed");

	ClassDB::bind_method(D_METHOD("set_kick_max_distance", "p_val"), &PhysicsCharacter3D::set_kick_max_distance);
	ClassDB::bind_method(D_METHOD("get_kick_max_distance"), &PhysicsCharacter3D::get_kick_max_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "kick_max_distance"), "set_kick_max_distance", "get_kick_max_distance");

	ClassDB::bind_method(D_METHOD("set_kick_impact_force", "p_val"), &PhysicsCharacter3D::set_kick_impact_force);
	ClassDB::bind_method(D_METHOD("get_kick_impact_force"), &PhysicsCharacter3D::get_kick_impact_force);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "kick_impact_force"), "set_kick_impact_force", "get_kick_impact_force");
}

PhysicsCharacter3D::PhysicsCharacter3D() {
}

PhysicsCharacter3D::~PhysicsCharacter3D() {
	if (idle_state)
		delete idle_state;
	if (move_state)
		delete move_state;
	if (jump_state)
		delete jump_state;
	if (fall_state)
		delete fall_state;
	if (grounded_state)
		delete grounded_state;
	if (airborne_state)
		delete airborne_state;
	if (dropkick_state)
		delete dropkick_state;
}

void PhysicsCharacter3D::_ready() {
	if (Engine::get_singleton()->is_editor_hint())
		return;

	_setup_character();
}

void PhysicsCharacter3D::_setup_character() {
	// 1. State Allocation with renamed Classes
	grounded_state = new CharGroundedState(this, nullptr);
	airborne_state = new CharAirborneState(this, nullptr);

	idle_state = new CharIdleState(this, grounded_state);
	move_state = new CharMoveState(this, grounded_state);
	jump_state = new CharJumpState(this, airborne_state);
	fall_state = new CharFallState(this, airborne_state);
	dropkick_state = new CharDropkickState(this, nullptr);

	current_state = fall_state;
	current_state->enter();

	// 2. Visual & Collision Setup
	// We create these programmatically if they don't exist
	if (get_child_count() == 0 || !has_node("CollisionShape3D")) {
		// Mesh Instance
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_name("CharacterMesh");

		Ref<CapsuleMesh> mesh;
		mesh.instantiate();
		mesh->set_radius(0.45f);
		mesh->set_height(1.8f);
		mi->set_mesh(mesh);

		// Premium Material
		Ref<StandardMaterial3D> mat;
		mat.instantiate();
		mat->set_albedo(Color(0.2f, 0.6f, 1.0f, 0.5f)); // Semi-transparent Blue
		mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
		mat->set_feature(StandardMaterial3D::FEATURE_EMISSION, true);
		mat->set_emission(Color(0.1f, 0.3f, 0.8f));
		mat->set_emission_energy_multiplier(2.0f);
		mi->set_material_override(mat);

		add_child(mi);

		// Collision Shape
		CollisionShape3D *cs = memnew(CollisionShape3D);
		cs->set_name("CollisionShape3D");

		Ref<CapsuleShape3D> shape;
		shape.instantiate();
		shape->set_radius(0.45f);
		shape->set_height(1.8f);
		cs->set_shape(shape);

		add_child(cs);

		// Offset both to sit above the "floating" center
		// Bottom of capsule should be at Vector3(0, -ride_height, 0) relative to origin
		float offset_y = 0.0f; // Adjust if needed based on floating logic
		mi->set_position(Vector3(0, offset_y, 0));
		cs->set_position(Vector3(0, offset_y, 0));
	}

	// 3. Rigidbody Setup
	set_lock_rotation_enabled(true);
	set_mass(80.0f); // Average human mass

	// 4. Register with GameManager
	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_character(this);
	}
}

void PhysicsCharacter3D::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint())
		return;

	float f_delta = (float)delta;

	// 1. Detect ground and update normals/distance
	_handle_ground_detection(f_delta);

	// 2. Execute HSM logic for inputs and transitions
	if (current_state) {
		current_state->physics_update(f_delta);
	}

	// 3. Apply floating spring force if grounded
	_apply_hover_spring(f_delta);
}

void PhysicsCharacter3D::_handle_ground_detection(float delta) {
	// Spherecast parameters
	float cast_dist = ride_height * 1.5f;
	float radius = 0.45f; // Slightly smaller than capsule collision

	MCRaycastHit hit = spherecast_3d(this, get_global_position(), Vector3(0, -1, 0), cast_dist, radius);

	is_grounded = hit.is_hit;
	if (is_grounded) {
		dist_to_ground = (get_global_position() - hit.position).length();
		ground_normal = hit.normal;
	} else {
		dist_to_ground = cast_dist;
		ground_normal = Vector3(0, 1, 0);
	}
}

void PhysicsCharacter3D::_apply_hover_spring(float delta) {
	if (!is_grounded)
		return;

	// PD Controller: Force = Stiffness * error - Damping * velocity
	float error = ride_height - dist_to_ground;
	float velocity_on_ray = get_linear_velocity().y;

	float force_magnitude = (error * spring_stiffness) - (velocity_on_ray * spring_damping);

	if (force_magnitude > 0) {
		apply_central_force(Vector3(0, force_magnitude, 0));
	}
}

void PhysicsCharacter3D::change_state(CharacterState *p_new_state) {
	if (current_state == p_new_state)
		return;

	if (current_state)
		current_state->exit();
	current_state = p_new_state;
	if (current_state)
		current_state->enter();
}

} // namespace godot
