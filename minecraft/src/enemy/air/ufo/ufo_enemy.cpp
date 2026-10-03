#include "ufo_enemy.h"

#include "../../../utils/fx/toy_mesh.h"

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

namespace godot {

static const int LIGHT_COUNT = 8;
static const float ARRIVED = 0.8f; // close enough to the hop goal to start hovering (m)
static const float DROP_AT = 0.55f; // fraction of the hover when the bomb drops
static const float IDLE_RADIUS = 10.0f;
static const float IDLE_SPEED_SCALE = 0.5f;
static const float SAUCER_SPIN = 1.2f; // rad/s
static const float BEAM_LENGTH = 7.0f;
static const Color BEAM_COLOR(0.5f, 1.0f, 0.45f);

void UfoEnemy::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_hop_radius", "v"), &UfoEnemy::set_hop_radius);
	ClassDB::bind_method(D_METHOD("get_hop_radius"), &UfoEnemy::get_hop_radius);
	ClassDB::bind_method(D_METHOD("set_hover_time", "v"), &UfoEnemy::set_hover_time);
	ClassDB::bind_method(D_METHOD("get_hover_time"), &UfoEnemy::get_hover_time);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hop_radius", PROPERTY_HINT_RANGE, "0,30,0.5,suffix:m"), "set_hop_radius", "get_hop_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hover_time", PROPERTY_HINT_RANGE, "0.3,5,0.05,suffix:s"), "set_hover_time", "get_hover_time");
}

UfoEnemy::UfoEnemy() {
	set_enemy_kind("UFO");
	health = 3.0f;
	max_health = 3.0f;
	max_speed = 10.0f;
	acceleration = 14.0f;
	altitude = 9.0f;
	fire_interval = 3.5f; // min time between drops; it just drifts between hops while reloading
	body_radius = 1.5f;
	muzzle_offset = Vector3(0, -0.7f, 0);
}

void UfoEnemy::_build_model() {
	Ref<StandardMaterial3D> hull = _flashable(Color(0.72f, 0.66f, 0.95f)); // lilac toy metal
	Ref<StandardMaterial3D> rim = _flashable(Color(0.45f, 0.4f, 0.7f));
	Ref<StandardMaterial3D> alien = _flashable(Color(0.45f, 0.9f, 0.35f));
	Ref<StandardMaterial3D> dark = _flashable(Color(0.08f, 0.08f, 0.1f));
	Ref<StandardMaterial3D> glass = ToyMesh::glow(Color(0.6f, 0.9f, 1.0f, 0.35f));

	saucer = memnew(Node3D);
	model->add_child(saucer);
	// Saucer: a squashed sphere with a darker rim band, and a little bottom bulb.
	ToyMesh::add(saucer, ToyMesh::sphere(1.6f, 16), hull, Vector3(), Vector3(), Vector3(1.0f, 0.28f, 1.0f));
	ToyMesh::add(saucer, ToyMesh::cylinder(1.62f, 1.62f, 0.12f, 16), rim, Vector3(0, -0.02f, 0));
	ToyMesh::add(saucer, ToyMesh::sphere(0.5f, 10), rim, Vector3(0, -0.35f, 0), Vector3(), Vector3(1.0f, 0.5f, 1.0f));
	// Ring of lights (each its own material so they can blink in a chase pattern).
	Ref<SphereMesh> bulb = ToyMesh::sphere(0.12f, 8);
	for (int i = 0; i < LIGHT_COUNT; i++) {
		float a = Math::TAU * i / LIGHT_COUNT;
		Ref<StandardMaterial3D> m = ToyMesh::glow(Color(1, 0.9f, 0.3f, 1.0f));
		lights.push_back(m);
		ToyMesh::add(saucer, bulb, m, Vector3(std::cos(a) * 1.45f, 0.0f, std::sin(a) * 1.45f));
	}

	// Pilot: green head with big eyes and antennae, under a glass dome (not spinning).
	ToyMesh::add(model, ToyMesh::sphere(0.38f, 12), alien, Vector3(0, 0.55f, 0));
	for (int side = -1; side <= 1; side += 2) {
		ToyMesh::add(model, ToyMesh::sphere(0.11f, 8), dark, Vector3(0.14f * side, 0.62f, -0.3f), Vector3(), Vector3(1.0f, 1.3f, 0.6f));
		ToyMesh::add(model, ToyMesh::cylinder(0.02f, 0.02f, 0.3f, 4), alien, Vector3(0.15f * side, 0.98f, 0), Vector3(0, 0, -0.3f * side));
		ToyMesh::add(model, ToyMesh::sphere(0.06f, 6), alien, Vector3(0.2f * side, 1.13f, 0));
	}
	ToyMesh::add(model, ToyMesh::sphere(0.75f, 14), glass, Vector3(0, 0.45f, 0), Vector3(), Vector3(1.0f, 0.9f, 1.0f));

	// Tractor beam: a soft green cone under the saucer, shown while about to drop.
	beam_mat = ToyMesh::glow(Color(BEAM_COLOR, 0.0f));
	beam = ToyMesh::add(model, ToyMesh::cylinder(0.5f, 2.2f, BEAM_LENGTH, 16), beam_mat, Vector3(0, -0.4f - BEAM_LENGTH * 0.5f, 0));
	beam->set_visible(false);
}

void UfoEnemy::_animate(float p_dt) {
	if (saucer) {
		saucer->rotate_y(SAUCER_SPIN * (crashing ? 4.0f : 1.0f) * p_dt);
	}
	// Chase lights: a bright spot running round the ring.
	for (int i = 0; i < (int)lights.size(); i++) {
		float phase = std::fmod(age * 6.0f - i, (float)LIGHT_COUNT);
		float on = (phase >= 0.0f && phase < 1.5f) ? 1.0f : 0.25f;
		lights[i]->set_albedo(Color(1.0f, 0.9f * on + 0.1f, 0.3f * on, 1.0f));
	}
}

void UfoEnemy::_pick_goal(
		const Vector3 &p_center,
		float p_radius
) {
	float a = (float)UtilityFunctions::randf_range(0.0, Math::TAU);
	float r = p_radius * (float)UtilityFunctions::randf_range(0.4, 1.0);
	goal = _hover_point(p_center + Vector3(std::cos(a) * r, 0, std::sin(a) * r), altitude);
	has_goal = true;
	hover_t = -1.0f;
	dropped = false;
}

void UfoEnemy::_think(
		float p_dt,
		Node3D *p_target
) {
	// Lazy saucer wobble on top of the flight lean (applied to the dome/saucer pivot).
	if (saucer) {
		saucer->set_rotation(Vector3(0.08f * std::sin(age * 2.3f), saucer->get_rotation().y, 0.08f * std::sin(age * 1.9f + 1.0f)));
	}

	Vector3 pos = get_global_position();
	if (!has_goal) {
		_pick_goal(p_target ? p_target->get_global_position() : home, p_target ? hop_radius : IDLE_RADIUS);
	}

	if (hover_t < 0.0f) {
		// Gliding to the hop spot.
		_steer_to(goal, p_dt, p_target ? 1.0f : IDLE_SPEED_SCALE);
		if (beam) {
			beam->set_visible(false);
		}
		if (pos.distance_to(goal) < ARRIVED) {
			if (p_target && fire_timer > 0.0f) {
				_pick_goal(p_target->get_global_position(), hop_radius); // reloading: keep drifting
			} else {
				hover_t = 0.0f;
			}
		}
		return;
	}

	// Hovering: hold still, beam on (the tell), drop, move on.
	hover_t += p_dt;
	_steer_to(goal + Vector3(0, 0.25f * std::sin(age * 3.0f), 0), p_dt);
	float k = hover_t / hover_time;
	if (beam && p_target) {
		beam->set_visible(true);
		float flicker = 0.75f + 0.25f * std::sin(age * 30.0f);
		beam_mat->set_albedo(Color(BEAM_COLOR, 0.28f * flicker * MIN(1.0f, k * 3.0f)));
	}
	if (p_target && !dropped && k >= DROP_AT) {
		dropped = true;
		_fire(p_target);
		fire_timer = fire_interval;
	}
	if (k >= 1.0f) {
		has_goal = false; // next hop
	}
}

} // namespace godot
