#include "helicopter_enemy.h"

#include "../../../utils/fx/toy_mesh.h"

#include <godot_cpp/core/math.hpp>

#include <cmath>

namespace godot {

static const float AIM_TIME = 0.7f; // slow down + point the nose before firing
static const float FIRE_AT = 0.45f; // fire this far into the aim
static const float AIM_SPEED_SCALE = 0.25f;
static const float PATROL_RADIUS = 8.0f;
static const float PATROL_SPEED_SCALE = 0.4f;
static const float MAIN_ROTOR_SPEED = 28.0f; // rad/s
static const float TAIL_ROTOR_SPEED = 40.0f;

void HelicopterEnemy::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_orbit_radius", "v"), &HelicopterEnemy::set_orbit_radius);
	ClassDB::bind_method(D_METHOD("get_orbit_radius"), &HelicopterEnemy::get_orbit_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "orbit_radius", PROPERTY_HINT_RANGE, "3,60,0.5,suffix:m"), "set_orbit_radius", "get_orbit_radius");
}

HelicopterEnemy::HelicopterEnemy() {
	set_enemy_kind("Helicopter");
	health = 4.0f;
	max_health = 4.0f;
	max_speed = 9.0f;
	acceleration = 9.0f;
	altitude = 8.0f;
	fire_interval = 3.5f;
	body_radius = 1.2f;
	muzzle_offset = Vector3(0, -0.4f, -1.4f);
}

// Forward is -Z. Everything hangs off `model` so bank / pitch / boing move it all.
void HelicopterEnemy::_build_model() {
	Ref<StandardMaterial3D> body = _flashable(Color(1.0f, 0.62f, 0.2f)); // toy orange
	Ref<StandardMaterial3D> dark = _flashable(Color(0.2f, 0.2f, 0.24f));
	Ref<StandardMaterial3D> glass = _flashable(Color(0.55f, 0.85f, 1.0f));
	Ref<StandardMaterial3D> white = _flashable(Color(1, 1, 1));

	// Cabin: a chubby egg, with a big round windscreen at the front.
	ToyMesh::add(model, ToyMesh::sphere(0.9f), body, Vector3(), Vector3(), Vector3(1.0f, 0.85f, 1.2f));
	ToyMesh::add(model, ToyMesh::sphere(0.62f), glass, Vector3(0, 0.12f, -0.62f), Vector3(), Vector3(1.0f, 0.85f, 0.7f));
	// Eyes + angry brows on the windscreen.
	for (int side = -1; side <= 1; side += 2) {
		ToyMesh::add(model, ToyMesh::sphere(0.14f, 8), white, Vector3(0.22f * side, 0.2f, -1.02f));
		ToyMesh::add(model, ToyMesh::sphere(0.07f, 8), dark, Vector3(0.22f * side, 0.2f, -1.14f));
		ToyMesh::add(model, ToyMesh::box(Vector3(0.28f, 0.06f, 0.06f)), dark, Vector3(0.22f * side, 0.4f, -1.08f), Vector3(0, 0, -0.45f * side));
	}
	// Tail boom + fin.
	ToyMesh::add(model, ToyMesh::cylinder(0.12f, 0.22f, 2.0f, 8), body, Vector3(0, 0.15f, 1.75f), Vector3(Math::PI * 0.5f, 0, 0));
	ToyMesh::add(model, ToyMesh::box(Vector3(0.06f, 0.6f, 0.4f)), body, Vector3(0, 0.45f, 2.65f));
	// Skids.
	for (int side = -1; side <= 1; side += 2) {
		ToyMesh::add(model, ToyMesh::cylinder(0.05f, 0.05f, 1.8f, 6), dark, Vector3(0.55f * side, -0.95f, 0), Vector3(Math::PI * 0.5f, 0, 0));
		ToyMesh::add(model, ToyMesh::cylinder(0.04f, 0.04f, 0.4f, 6), dark, Vector3(0.5f * side, -0.75f, -0.4f));
		ToyMesh::add(model, ToyMesh::cylinder(0.04f, 0.04f, 0.4f, 6), dark, Vector3(0.5f * side, -0.75f, 0.4f));
	}
	// Main rotor: mast + two crossed blades on a spinning node.
	ToyMesh::add(model, ToyMesh::cylinder(0.08f, 0.08f, 0.35f, 6), dark, Vector3(0, 0.88f, 0));
	main_rotor = memnew(Node3D);
	main_rotor->set_position(Vector3(0, 1.06f, 0));
	model->add_child(main_rotor);
	ToyMesh::add(main_rotor, ToyMesh::box(Vector3(3.6f, 0.04f, 0.22f)), dark, Vector3());
	ToyMesh::add(main_rotor, ToyMesh::box(Vector3(0.22f, 0.04f, 3.6f)), dark, Vector3());
	ToyMesh::add(main_rotor, ToyMesh::sphere(0.14f, 8), body, Vector3());
	// Tail rotor (spins around X).
	tail_rotor = memnew(Node3D);
	tail_rotor->set_position(Vector3(0.1f, 0.45f, 2.7f));
	model->add_child(tail_rotor);
	ToyMesh::add(tail_rotor, ToyMesh::box(Vector3(0.04f, 0.9f, 0.12f)), dark, Vector3());
}

void HelicopterEnemy::_animate(float p_dt) {
	float spin = crashing ? 0.5f : 1.0f; // rotors wind down as it falls
	if (main_rotor) {
		main_rotor->rotate_y(MAIN_ROTOR_SPEED * spin * p_dt);
	}
	if (tail_rotor) {
		tail_rotor->rotate_x(TAIL_ROTOR_SPEED * spin * p_dt);
	}
}

void HelicopterEnemy::_think(
		float p_dt,
		Node3D *p_target
) {
	Vector3 pos = get_global_position();
	float bob = 0.35f * std::sin(age * 1.7f); // gentle hover bob

	if (!p_target) {
		// Patrol: a slow circle around home.
		orbit_angle += (max_speed * PATROL_SPEED_SCALE / PATROL_RADIUS) * p_dt;
		Vector3 goal = home + Vector3(std::cos(orbit_angle), 0, std::sin(orbit_angle)) * PATROL_RADIUS;
		goal = _hover_point(goal, altitude);
		goal.y += bob;
		_steer_to(goal, p_dt, PATROL_SPEED_SCALE);
		_face(flight_velocity, p_dt);
		aim_hold = 0.0f;
		return;
	}

	Vector3 tpos = p_target->get_global_position();
	if (aim_hold <= 0.0f && fire_timer <= 0.0f) {
		aim_hold = AIM_TIME; // start an attack run: slow, turn the nose, fire
		fired_this_aim = false;
	}

	float speed_scale = 1.0f;
	if (aim_hold > 0.0f) {
		aim_hold -= p_dt;
		speed_scale = AIM_SPEED_SCALE;
		_face(tpos - pos, p_dt, 8.0f);
		if (!fired_this_aim && aim_hold <= AIM_TIME - FIRE_AT) {
			fired_this_aim = true;
			_fire(p_target);
			fire_timer = fire_interval;
		}
	} else {
		_face(flight_velocity, p_dt);
	}

	// Orbit the target: advance around the circle at the current speed.
	Vector3 flat_from(pos.x - tpos.x, 0, pos.z - tpos.z);
	if (flat_from.length_squared() > 1.0f && aim_hold <= 0.0f) {
		orbit_angle = Math::atan2(flat_from.z, flat_from.x); // continue from where we are
	}
	orbit_angle += (max_speed * speed_scale / orbit_radius) * p_dt;
	Vector3 goal = tpos + Vector3(std::cos(orbit_angle), 0, std::sin(orbit_angle)) * orbit_radius;
	goal = _hover_point(goal, altitude);
	goal.y += bob;
	// Lead the goal a little around the circle so it keeps moving instead of parking.
	goal += Vector3(-std::sin(orbit_angle), 0, std::cos(orbit_angle)) * 2.0f;
	_steer_to(goal, p_dt, speed_scale);
}

} // namespace godot
