#include "flying_enemy.h"

#include "../../combat/projectile_launcher.h"
#include "../../game_manager/game_constants.h"
#include "../../game_manager/game_manager.h"
#include "../../utils/fx/toy_mesh.h"
#include "../../utils/raycast/mc_raycast.h"
#include "../enemy_manager.h"

#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

namespace godot {

static const float GROUND_REFRESH = 0.2f; // seconds between ground probes
static const float GROUND_PROBE = 60.0f; // how far down to look for terrain (m)
static const float ARRIVE_RADIUS = 4.0f; // start slowing down this close to the goal (m)
static const float BANK_GAIN = 0.08f; // radians of roll per m/s^2 of sideways acceleration
static const float BANK_MAX = 0.6f;
static const float PITCH_GAIN = 0.025f; // nose-down radians per m/s of forward speed
static const float FLASH_TIME = 0.12f;
static const float BOING_STIFF = 260.0f; // hit squash spring (underdamped: wobbles)
static const float BOING_DAMP = 9.0f;
static const float BOING_KICK = 4.0f;
static const float CRASH_GRAVITY = 14.0f;
static const float CRASH_MAX_TIME = 5.0f;
static const float POP_LINGER = 0.8f; // keep the node after the pop so puffs can fade

void FlyingEnemy::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_speed", "v"), &FlyingEnemy::set_max_speed);
	ClassDB::bind_method(D_METHOD("get_max_speed"), &FlyingEnemy::get_max_speed);
	ClassDB::bind_method(D_METHOD("set_acceleration", "v"), &FlyingEnemy::set_acceleration);
	ClassDB::bind_method(D_METHOD("get_acceleration"), &FlyingEnemy::get_acceleration);
	ClassDB::bind_method(D_METHOD("set_altitude", "v"), &FlyingEnemy::set_altitude);
	ClassDB::bind_method(D_METHOD("get_altitude"), &FlyingEnemy::get_altitude);
	ClassDB::bind_method(D_METHOD("set_aggro_range", "v"), &FlyingEnemy::set_aggro_range);
	ClassDB::bind_method(D_METHOD("get_aggro_range"), &FlyingEnemy::get_aggro_range);
	ClassDB::bind_method(D_METHOD("set_fire_interval", "v"), &FlyingEnemy::set_fire_interval);
	ClassDB::bind_method(D_METHOD("get_fire_interval"), &FlyingEnemy::get_fire_interval);
	ClassDB::bind_method(D_METHOD("set_projectile_profile", "profile"), &FlyingEnemy::set_projectile_profile);
	ClassDB::bind_method(D_METHOD("get_projectile_profile"), &FlyingEnemy::get_projectile_profile);
	ClassDB::bind_method(D_METHOD("is_crashing"), &FlyingEnemy::is_crashing);

	ADD_GROUP("Flight", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_speed", PROPERTY_HINT_RANGE, "0,40,0.1,suffix:m/s"), "set_max_speed", "get_max_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "acceleration", PROPERTY_HINT_RANGE, "0.5,60,0.1,suffix:m/s²"), "set_acceleration", "get_acceleration");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "altitude", PROPERTY_HINT_RANGE, "1,40,0.1,suffix:m"), "set_altitude", "get_altitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "aggro_range", PROPERTY_HINT_RANGE, "5,150,1,suffix:m"), "set_aggro_range", "get_aggro_range");
	ADD_GROUP("Attack", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fire_interval", PROPERTY_HINT_RANGE, "0.3,15,0.1,suffix:s"), "set_fire_interval", "get_fire_interval");
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "projectile_profile", PROPERTY_HINT_RESOURCE_TYPE, "ProjectileProfile"),
			"set_projectile_profile",
			"get_projectile_profile"
	);
	ADD_GROUP("", "");
}

void FlyingEnemy::_ready() {
	EnemyBase::_ready();
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	home = get_global_position();
	fire_timer = fire_interval * (float)UtilityFunctions::randf_range(0.5, 1.0); // stagger squads
	_build_common();
	_build_model();
	ground_y = _ground_height_at(home);
}

void FlyingEnemy::_build_common() {
	// Collision: a sphere on the ENEMY layer. Moved kinematically, so it needs no mask.
	set_collision_layer(toLayer(LAYER_ENEMY));
	set_collision_mask(0);
	bool has_shape = false;
	for (int i = 0; i < get_child_count(); i++) {
		has_shape = has_shape || Object::cast_to<CollisionShape3D>(get_child(i));
	}
	if (!has_shape) {
		Ref<SphereShape3D> sphere;
		sphere.instantiate();
		sphere->set_radius(body_radius);
		CollisionShape3D *cs = memnew(CollisionShape3D);
		cs->set_shape(sphere);
		add_child(cs);
	}

	model = memnew(Node3D);
	model->set_name("Model");
	add_child(model);

	if (projectile_profile.is_null()) {
		String path = _default_profile_path();
		ResourceLoader *rl = ResourceLoader::get_singleton();
		if (!path.is_empty() && rl->exists(path)) {
			projectile_profile = rl->load(path);
		}
	}
	launcher = memnew(ProjectileLauncher);
	launcher->set_name("Launcher");
	launcher->set_position(muzzle_offset);
	launcher->set_max_alive(2);
	if (projectile_profile.is_valid()) {
		launcher->set_profile(projectile_profile);
	}
	add_child(launcher);

	smoke.build(this, 14, 0.35f, Color(0.35f, 0.35f, 0.38f));
}

Ref<StandardMaterial3D> FlyingEnemy::_flashable(const Color &p_color) {
	Ref<StandardMaterial3D> m = ToyMesh::toon(p_color);
	flash_materials.push_back(m);
	flash_base_colors.push_back(p_color);
	return m;
}

Node3D *FlyingEnemy::_current_target() {
	GameManager *gm = GameManager::get_singleton();
	Node3D *t = gm ? Object::cast_to<Node3D>(gm->get_active_target()) : nullptr;
	if (!t || !t->is_inside_tree()) {
		return nullptr;
	}
	return t->get_global_position().distance_to(get_global_position()) <= aggro_range ? t : nullptr;
}

float FlyingEnemy::_ground_height_at(const Vector3 &p_point) {
	Vector3 top = p_point + Vector3(0, 5.0f, 0);
	MCRaycastHit hit = raycast_3d(this, top, top - Vector3(0, GROUND_PROBE, 0), toLayer(LAYER_TERRAIN));
	return hit.is_hit ? hit.position.y : home.y - altitude;
}

Vector3 FlyingEnemy::_hover_point(
		const Vector3 &p_xz,
		float p_altitude
) {
	return Vector3(p_xz.x, _ground_height_at(p_xz) + p_altitude, p_xz.z);
}

void FlyingEnemy::_steer_to(
		const Vector3 &p_goal,
		float p_dt,
		float p_speed_scale
) {
	Vector3 to = p_goal - get_global_position();
	float dist = to.length();
	float speed = max_speed * p_speed_scale * CLAMP(dist / ARRIVE_RADIUS, 0.0f, 1.0f);
	Vector3 desired = (dist > 1e-3f) ? to / dist * speed : Vector3();
	flight_velocity = flight_velocity.move_toward(desired, acceleration * p_dt);
}

void FlyingEnemy::_face(
		const Vector3 &p_dir,
		float p_dt,
		float p_rate
) {
	if (Vector2(p_dir.x, p_dir.z).length_squared() < 1e-4f) {
		return;
	}
	float target_yaw = Math::atan2(-p_dir.x, -p_dir.z);
	float yaw = get_rotation().y;
	float diff = UtilityFunctions::wrapf(target_yaw - yaw, -Math::PI, Math::PI);
	set_rotation(Vector3(0, yaw + diff * (1.0f - std::exp(-p_rate * p_dt)), 0));
}

void FlyingEnemy::_fire(Node3D *p_target) {
	if (launcher) {
		launcher->fire(p_target);
	}
}

void FlyingEnemy::_update_flight_pose(
		float p_dt,
		const Vector3 &p_prev_velocity
) {
	if (!model) {
		return;
	}
	// Lean into acceleration: roll from sideways accel, pitch nose-down with forward speed.
	Basis body = get_global_basis();
	Vector3 right = body.get_column(0);
	Vector3 fwd = -body.get_column(2);
	Vector3 accel = (flight_velocity - p_prev_velocity) / MAX(1e-3f, p_dt);
	float bank_target = CLAMP(-accel.dot(right) * BANK_GAIN, -BANK_MAX, BANK_MAX);
	bank += (bank_target - bank) * (1.0f - std::exp(-5.0f * p_dt));
	float pitch = -CLAMP(flight_velocity.dot(fwd) * PITCH_GAIN, -0.35f, 0.35f);
	model->set_rotation(Vector3(pitch, model->get_rotation().y, bank));
}

void FlyingEnemy::_update_hit_fx(float p_dt) {
	// White flash, fading back to the part colours.
	if (hit_flash > 0.0f) {
		hit_flash = MAX(0.0f, hit_flash - p_dt);
		float k = hit_flash / FLASH_TIME;
		for (size_t i = 0; i < flash_materials.size(); i++) {
			flash_materials[i]->set_albedo(flash_base_colors[i].lerp(Color(1, 1, 1), k));
		}
	}
	// Squash "boing" spring.
	boing_vel += (-BOING_STIFF * boing - BOING_DAMP * boing_vel) * p_dt;
	boing += boing_vel * p_dt;
	if (model) {
		float s = CLAMP(boing, -0.4f, 0.4f);
		model->set_scale(Vector3(1.0f + s, 1.0f - s, 1.0f + s));
	}
}

void FlyingEnemy::take_damage(float p_amount) {
	if (is_dead) {
		return;
	}
	hit_flash = FLASH_TIME;
	boing_vel += BOING_KICK;
	EnemyBase::take_damage(p_amount); // may call die()
}

void FlyingEnemy::die() {
	if (is_dead) {
		return;
	}
	is_dead = true;
	crashing = true;
	crash_time = 0.0f;
	crash_spin = (UtilityFunctions::randf() < 0.5f ? -1.0f : 1.0f) * 9.0f;
	flight_velocity += Vector3(0, 3.0f, 0); // a little hop as it's hit
	if (EnemyManager *em = EnemyManager::get_singleton()) {
		em->unregister_enemy(this); // nothing should target a wreck
	}
	set_collision_layer(0);
}

void FlyingEnemy::_update_crash(float p_dt) {
	crash_time += p_dt;
	smoke.update(p_dt);
	if (popped) {
		if (crash_time > POP_LINGER) {
			queue_free();
		}
		return;
	}
	flight_velocity.y -= CRASH_GRAVITY * p_dt;
	flight_velocity.x *= 1.0f - MIN(1.0f, 0.8f * p_dt);
	flight_velocity.z *= 1.0f - MIN(1.0f, 0.8f * p_dt);
	Vector3 pos = get_global_position() + flight_velocity * p_dt;
	set_global_position(pos);
	set_velocity(flight_velocity);
	if (model) {
		model->rotate_y(crash_spin * p_dt);
		model->set_rotation(Vector3(0.5f * std::sin(crash_time * 7.0f), model->get_rotation().y, 0.4f));
	}
	smoke_timer += p_dt;
	if (smoke_timer > 0.06f) {
		smoke_timer = 0.0f;
		smoke.emit(pos, Vector3(0, 1.5f, 0), 0.9f, 1.0f);
	}
	bool ground = pos.y <= _ground_height_at(pos) + body_radius * 0.5f;
	if (ground || crash_time > CRASH_MAX_TIME) {
		popped = true;
		crash_time = 0.0f;
		smoke.burst(pos, 8, 5.0f, 0.7f, 2.0f);
		if (model) {
			model->set_visible(false);
		}
		set_velocity(Vector3());
	}
}

void FlyingEnemy::_physics_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	float dt = (float)p_delta;
	age += dt;
	_animate(dt);
	_update_hit_fx(dt);
	if (crashing) {
		_update_crash(dt);
		return;
	}

	ground_timer -= dt;
	if (ground_timer <= 0.0f) {
		ground_timer = GROUND_REFRESH;
		ground_y = _ground_height_at(get_global_position());
	}

	Vector3 prev = flight_velocity;
	fire_timer -= dt;
	_think(dt, _current_target());

	Vector3 pos = get_global_position() + flight_velocity * dt;
	pos.y = MAX(pos.y, ground_y + 1.5f); // never scrape the ground, whatever the AI asked for
	set_global_position(pos);
	set_velocity(flight_velocity); // so body_velocity() / homing lead see our motion
	_update_flight_pose(dt, prev);
	smoke.update(dt);
}

} // namespace godot
