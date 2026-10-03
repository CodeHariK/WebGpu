#include "projectile.h"

#include "../../debug_draw/debug_manager.h"
#include "../../enemy/enemy_base.h"
#include "../../enemy/enemy_manager.h"
#include "../../game_manager/game_constants.h"
#include "../../utils/body_velocity.h"
#include "../../utils/raycast/mc_raycast.h"
#include "ballistic_arc.h"
#include "homing_steering.h"

#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace godot {

static const float PARRY_RELAUNCH_TIME = 0.15f; // straight flight after a parry, before re-homing
static const float ARROW_TIP_OFFSET = 0.45f; // model centre to tip (m, at scale 1)
static const float FALLBACK_DROP = 1.0f; // no floor under the target: land this far below its centre
static const int MAX_BLAST_HITS = 16;
static const float MISS_GIVE_UP = 1.5f; // committed and now this many commit-distances away -> lost
static const uint32_t HIT_MASK = (1u << (LAYER_TERRAIN - 1)) | (1u << (LAYER_PLAYER - 1)) |
		(1u << (LAYER_ENEMY - 1)) | (1u << (LAYER_MOVING_OBJECTS - 1));

void Projectile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("fire", "origin", "direction", "target", "shooter"), &Projectile::fire);
	ClassDB::bind_method(D_METHOD("fire_velocity", "origin", "velocity", "shooter"), &Projectile::fire_velocity);
	ClassDB::bind_method(D_METHOD("set_profile", "profile"), &Projectile::set_profile);
	ClassDB::bind_method(D_METHOD("get_profile"), &Projectile::get_profile);
	ClassDB::bind_method(D_METHOD("set_debug_draw", "on"), &Projectile::set_debug_draw);
	ClassDB::bind_method(D_METHOD("get_debug_draw"), &Projectile::get_debug_draw);
	ClassDB::bind_method(D_METHOD("get_phase"), &Projectile::get_phase);
	ClassDB::bind_method(D_METHOD("get_velocity"), &Projectile::get_velocity);
	ClassDB::bind_method(D_METHOD("get_aim_point"), &Projectile::get_aim_point);
	ClassDB::bind_method(D_METHOD("get_impact_point"), &Projectile::get_impact_point);
	ClassDB::bind_method(D_METHOD("is_parried"), &Projectile::is_parried);
	ClassDB::bind_method(D_METHOD("get_target"), &Projectile::get_target);

	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "profile", PROPERTY_HINT_RESOURCE_TYPE, "ProjectileProfile"),
			"set_profile",
			"get_profile"
	);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_draw"), "set_debug_draw", "get_debug_draw");

	BIND_ENUM_CONSTANT(PHASE_LAUNCH);
	BIND_ENUM_CONSTANT(PHASE_SEEK);
	BIND_ENUM_CONSTANT(PHASE_COMMIT);
	BIND_ENUM_CONSTANT(PHASE_LOST);
	BIND_ENUM_CONSTANT(PHASE_BALLISTIC);
	BIND_ENUM_CONSTANT(PHASE_POP);

	ADD_SIGNAL(MethodInfo("hit", PropertyInfo(Variant::OBJECT, "body")));
	ADD_SIGNAL(MethodInfo("parried"));
	ADD_SIGNAL(MethodInfo("expired"));
}

Projectile::Projectile() {}

uint32_t Projectile::get_hit_mask() {
	return HIT_MASK;
}

void Projectile::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		set_physics_process(false);
		return;
	}
	if (profile.is_null()) {
		profile.instantiate(); // defaults = boss missile
	}
	visual.build(this, profile);
	if (profile->get_show_telegraph()) {
		telegraph.build(this);
	}
}

void Projectile::_exit_tree() {
	_clear_debug();
}

void Projectile::fire(
		const Vector3 &p_origin,
		const Vector3 &p_dir,
		Node3D *p_target,
		Node3D *p_shooter
) {
	if (profile.is_null()) {
		profile.instantiate();
	}
	Vector3 dir = (p_dir.length_squared() > 1e-6f) ? p_dir.normalized() : Vector3(0, 0, -1);
	speed = profile->get_speed();
	lead = profile->get_lead();
	velocity = dir * speed;
	target_id = p_target ? p_target->get_instance_id() : 0;
	shooter_id = p_shooter ? p_shooter->get_instance_id() : 0;
	aim = p_target ? p_target->get_global_position() : p_origin + dir * 10.0f;
	age = 0.0f;
	parried = false;
	fired = true;
	_set_phase(PHASE_LAUNCH);

	if (is_inside_tree()) {
		set_global_position(p_origin);
	} else {
		set_position(p_origin);
	}
	if (_is_ballistic()) {
		_aim_ballistic(p_target, p_origin, profile->get_min_flight_time()); // the solved arc replaces the launch direction
	}
	_orient();
}

void Projectile::fire_velocity(
		const Vector3 &p_origin,
		const Vector3 &p_velocity,
		Node3D *p_shooter
) {
	fire(p_origin, p_velocity, nullptr, p_shooter); // common setup (position, ids, state)
	velocity = p_velocity;
	speed = p_velocity.length();
	impact_point = p_origin;
	flight_time = profile->get_lifetime(); // only drives the shake ramp; there's no zone
	_set_phase(PHASE_BALLISTIC);
	_orient();
}

bool Projectile::_is_ballistic() const {
	return profile.is_valid() && profile->get_guidance() == ProjectileProfile::GUIDANCE_BALLISTIC;
}

Vector3 Projectile::_ground_below(
		const Vector3 &p_point,
		Node3D *p_ignore
) const {
	TypedArray<RID> exclude;
	if (CollisionObject3D *co = Object::cast_to<CollisionObject3D>(p_ignore)) {
		exclude.push_back(co->get_rid()); // bodies on the terrain layer would otherwise be "ground"
	}
	MCRaycastHit hit = raycast_3d(
			const_cast<Projectile *>(this),
			p_point + Vector3(0, 2.0f, 0),
			p_point - Vector3(0, 30.0f, 0),
			toLayer(LAYER_TERRAIN),
			exclude
	);
	return hit.is_hit ? hit.position : p_point - Vector3(0, FALLBACK_DROP, 0);
}

void Projectile::_aim_ballistic(
		Node3D *p_target,
		const Vector3 &p_from,
		float p_min_time
) {
	float g = profile->get_ballistic_gravity();
	float min_t = p_min_time;
	Vector3 goal = p_target ? p_target->get_global_position() : p_from + velocity.normalized() * 10.0f;
	Vector3 drift = p_target ? body_velocity(p_target) : Vector3();
	drift.y = 0.0f; // lead along the ground only; jumps shouldn't throw the zone into the air

	// Flight time depends on where we aim, and where we aim depends on flight time: two
	// fixed-point passes are plenty at these speeds.
	float t = BallisticArc::flight_time(p_from, goal, speed, min_t);
	for (int i = 0; i < 2; i++) {
		t = BallisticArc::flight_time(p_from, goal + drift * (t * lead), speed, min_t);
	}
	impact_point = _ground_below(goal + drift * (t * lead), p_target);
	flight_time = BallisticArc::flight_time(p_from, impact_point, speed, min_t);
	velocity = BallisticArc::launch_velocity(p_from, impact_point, flight_time, g);
	aim = impact_point;
	_set_phase(PHASE_BALLISTIC);
}

Node3D *Projectile::_resolve(uint64_t p_id) const {
	if (p_id == 0) {
		return nullptr;
	}
	Node3D *n = Object::cast_to<Node3D>(ObjectDB::get_instance(p_id));
	return (n && n->is_inside_tree()) ? n : nullptr;
}

void Projectile::_set_phase(Phase p_phase) {
	phase = p_phase;
	phase_age = 0.0f;
	if (phase == PHASE_POP) {
		visual.start_pop();
		telegraph.hide();
		_clear_debug();
	}
}

void Projectile::_update_phase(
		Node3D *p_target,
		const Vector3 &p_pos
) {
	Vector3 to_target = p_target ? p_target->get_global_position() - p_pos : Vector3();
	float dist = to_target.length();

	switch (phase) {
		case PHASE_LAUNCH: {
			float launch = parried ? PARRY_RELAUNCH_TIME : profile->get_launch_time();
			if (phase_age >= launch) {
				_set_phase(p_target ? PHASE_SEEK : PHASE_LOST);
			}
		} break;
		case PHASE_SEEK: {
			if (!p_target) {
				_set_phase(PHASE_LOST);
			} else if (dist < profile->get_commit_distance()) {
				_set_phase(PHASE_COMMIT);
			} else if (Math::rad_to_deg(velocity.angle_to(to_target)) > profile->get_lose_lock_angle()) {
				_set_phase(PHASE_LOST);
			}
		} break;
		case PHASE_COMMIT: {
			// Missed and flew past: give up rather than flying on with a red reticle.
			if (!p_target || (phase_age > 0.3f && dist > profile->get_commit_distance() * MISS_GIVE_UP)) {
				_set_phase(PHASE_LOST);
			}
		} break;
		default:
			break;
	}
}

float Projectile::_steer(
		Node3D *p_target,
		const Vector3 &p_pos,
		float p_dt
) {
	aim = HomingSteering::aim_point(
			p_pos,
			speed,
			p_target->get_global_position(),
			body_velocity(p_target),
			lead,
			profile->get_max_lead_time()
	);
	Vector3 dir = velocity.normalized();
	Vector3 desired = aim - p_pos;
	if (desired.length_squared() < 1e-6f) {
		return 0.0f;
	}
	float max_turn = Math::deg_to_rad(profile->get_turn_rate()) * p_dt;
	Vector3 new_dir = HomingSteering::turn_toward(dir, desired.normalized(), max_turn);
	velocity = new_dir * speed;
	return dir.cross(new_dir).y / p_dt; // signed yaw rate, for banking
}

bool Projectile::_try_parry(
		Node3D *p_target,
		const Vector3 &p_pos
) {
	if (!p_target || !profile->get_parryable() || phase == PHASE_POP) {
		return false;
	}
	if (p_pos.distance_to(p_target->get_global_position()) > profile->get_parry_radius()) {
		return false;
	}
	if (!p_target->has_method("is_parrying") || !bool(p_target->call("is_parrying"))) {
		return false;
	}

	// Flip sides: the parrier becomes the shooter; the shot pops up and falls on a random
	// enemy (the original shooter if nobody else is around). Homing or not, it's a pure
	// ballistic lob from here on, with the landing zone drawn under the victim.
	Node3D *lob = _pick_lob_target(p_target, p_pos);
	if (!lob) {
		lob = _resolve(shooter_id);
	}
	shooter_id = target_id;
	target_id = lob ? lob->get_instance_id() : 0;
	parried = true;
	speed *= profile->get_parry_speed_scale();
	lead = 1.0f;
	_aim_ballistic(lob, p_pos, profile->get_parry_lob_time());
	emit_signal("parried");
	return true;
}

Node3D *Projectile::_pick_lob_target(
		Node3D *p_exclude,
		const Vector3 &p_pos
) const {
	EnemyManager *em = EnemyManager::get_singleton();
	if (!em) {
		return nullptr;
	}
	float range = profile->get_parry_lob_range();
	std::vector<Node3D *> candidates;
	for (const EnemyData &e : em->get_enemies()) {
		Node3D *n = e.node;
		if (!n || n == p_exclude || !n->is_inside_tree() || n->is_queued_for_deletion()) {
			continue;
		}
		EnemyBase *eb = Object::cast_to<EnemyBase>(n);
		if (eb && eb->get_is_dead()) {
			continue;
		}
		if (n->get_global_position().distance_to(p_pos) <= range) {
			candidates.push_back(n);
		}
	}
	if (candidates.empty()) {
		return nullptr;
	}
	return candidates[UtilityFunctions::randi_range(0, (int64_t)candidates.size() - 1)];
}

bool Projectile::_sweep(
		Node3D *p_target,
		const Vector3 &p_from,
		const Vector3 &p_to
) {
	// Fat test against the target: closest point on this tick's segment to its centre.
	if (p_target) {
		Vector3 c = p_target->get_global_position();
		Vector3 seg = p_to - p_from;
		float len2 = seg.length_squared();
		float t = (len2 > 1e-8f) ? CLAMP((c - p_from).dot(seg) / len2, 0.0f, 1.0f) : 0.0f;
		Vector3 closest = p_from + seg * t;
		if (closest.distance_to(c) <= profile->get_hit_radius()) {
			set_global_position(closest);
			_impact(p_target);
			return true;
		}
	}

	// Thin ray against the world (and anything else in the way); never our own shooter.
	TypedArray<RID> exclude;
	if (CollisionObject3D *co = Object::cast_to<CollisionObject3D>(_resolve(shooter_id))) {
		exclude.push_back(co->get_rid());
	}
	MCRaycastHit hit = raycast_3d(this, p_from, p_to, HIT_MASK, exclude);
	if (hit.is_hit) {
		Vector3 at = hit.position;
		if (profile->get_style() == ProjectileProfile::STYLE_ARROW) {
			// Bury the tip, not the middle: the arrow sticks out of what it hit.
			at -= (p_to - p_from).normalized() * ARROW_TIP_OFFSET * profile->get_model_scale();
		}
		set_global_position(at);
		_impact(hit.collider);
		return true;
	}
	return false;
}

void Projectile::_damage(Object *p_hit) {
	if (Node *n = Object::cast_to<Node>(p_hit)) {
		if (n->has_method("take_damage")) {
			n->call("take_damage", profile->get_damage());
		} else if (n->has_method("pop")) {
			n->call("pop"); // e.g. a Bubble: soaks the shot like a shield
		}
	}
	emit_signal("hit", p_hit);
}

void Projectile::_impact(Object *p_hit) {
	if (profile->get_blast_radius() > 0.0f) {
		_blast(get_global_position(), p_hit);
	} else {
		_damage(p_hit);
	}
	_set_phase(PHASE_POP);
}

void Projectile::_blast(
		const Vector3 &p_pos,
		Object *p_direct
) {
	float radius = profile->get_blast_radius();
	std::vector<Object *> hits;
	auto add = [&hits](Object *o) {
		if (o && std::find(hits.begin(), hits.end(), o) == hits.end()) {
			hits.push_back(o);
		}
	};
	add(p_direct);

	// Every body in the blast sphere (one query, on impact only).
	Ref<World3D> world = get_world_3d();
	PhysicsDirectSpaceState3D *space = world.is_valid() ? world->get_direct_space_state() : nullptr;
	if (space) {
		Ref<SphereShape3D> sphere;
		sphere.instantiate();
		sphere->set_radius(radius);
		Ref<PhysicsShapeQueryParameters3D> q;
		q.instantiate();
		q->set_shape(sphere);
		q->set_transform(Transform3D(Basis(), p_pos));
		q->set_collision_mask(HIT_MASK);
		TypedArray<RID> exclude;
		if (CollisionObject3D *co = Object::cast_to<CollisionObject3D>(_resolve(shooter_id))) {
			exclude.push_back(co->get_rid());
		}
		q->set_exclude(exclude);
		TypedArray<Dictionary> results = space->intersect_shape(q, MAX_BLAST_HITS);
		for (int i = 0; i < results.size(); i++) {
			Dictionary d = results[i];
			add(Object::cast_to<Object>(d["collider"]));
		}
	}
	// The target counts if its centre is in range, even if its shape isn't on a hit layer.
	if (Node3D *target = _resolve(target_id)) {
		if (target->get_global_position().distance_to(p_pos) <= radius) {
			add(target);
		}
	}
	for (Object *o : hits) {
		_damage(o);
	}
}

void Projectile::_orient() {
	if (velocity.length_squared() < 1e-6f) {
		return;
	}
	Vector3 dir = velocity.normalized();
	Vector3 up = (std::abs(dir.y) > 0.98f) ? Vector3(0, 0, 1) : Vector3(0, 1, 0);
	Basis b = Basis::looking_at(dir, up);
	if (is_inside_tree()) {
		set_global_basis(b);
	} else {
		set_basis(b);
	}
}

float Projectile::_lock(
		Node3D *p_target,
		const Vector3 &p_pos
) const {
	// 0..1: how close it is, measured from 4 commit-distances in to the commit range.
	if (!p_target) {
		return 0.0f;
	}
	float commit = profile->get_commit_distance();
	float far = MAX(commit * 4.0f, commit + 1.0f);
	float dist = p_pos.distance_to(p_target->get_global_position());
	return 1.0f - CLAMP((dist - commit) / (far - commit), 0.0f, 1.0f);
}

float Projectile::_shake(
		Node3D *p_target,
		const Vector3 &p_pos
) const {
	// Calm on launch, increasingly frantic as it closes in; slack once it has given up.
	switch (phase) {
		case PHASE_LAUNCH:
			return 0.7f;
		case PHASE_SEEK:
			return 1.0f + 1.2f * _lock(p_target, p_pos);
		case PHASE_COMMIT:
			return 2.4f;
		case PHASE_LOST:
			return 0.5f;
		case PHASE_BALLISTIC:
			return 0.8f + 1.6f * CLAMP(phase_age / MAX(0.05f, flight_time), 0.0f, 1.0f);
		default:
			return 1.0f;
	}
}

void Projectile::_update_telegraph(
		Node3D *p_target,
		const Vector3 &p_pos,
		float p_dt
) {
	if (!profile->get_show_telegraph()) {
		return;
	}
	if (phase == PHASE_BALLISTIC) {
		float radius = MAX(profile->get_blast_radius(), profile->get_hit_radius());
		telegraph.update_zone(p_dt, impact_point, radius, phase_age / MAX(0.05f, flight_time));
		return;
	}
	bool tracking = p_target && (phase == PHASE_LAUNCH || phase == PHASE_SEEK || phase == PHASE_COMMIT);
	if (!tracking) {
		telegraph.hide();
		return;
	}
	float lock = _lock(p_target, p_pos);
	bool show_aim = lead > 0.05f && phase == PHASE_SEEK;
	telegraph.update(p_dt, p_target, aim, lock, phase == PHASE_COMMIT, show_aim);
}

void Projectile::_draw_debug(const Vector3 &p_pos) {
	DebugManager *dm = DebugManager::get_singleton();
	if (!dm) {
		return;
	}
	String id = String("homing_") + String::num_uint64(get_instance_id());
	Color col = (phase == PHASE_SEEK) ? Color(1, 0.85f, 0.2f) : Color(1, 0.3f, 0.2f);
	dm->draw_line(id, p_pos, aim, 0.03f, col);
	dm->draw_sphere(id, aim, 0.15f, col);
}

void Projectile::_clear_debug() {
	DebugManager *dm = DebugManager::get_singleton();
	if (!dm || !debug_draw) {
		return;
	}
	String id = String("homing_") + String::num_uint64(get_instance_id());
	dm->clear_line(id);
	dm->clear_sphere(id);
}

void Projectile::_physics_process(double p_delta) {
	if (!fired || Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	float dt = (float)p_delta;
	age += dt;
	phase_age += dt;

	if (phase == PHASE_POP) {
		if (visual.update_pop(dt)) {
			queue_free();
		}
		return;
	}

	Node3D *target = _resolve(target_id);
	Vector3 pos = get_global_position();
	if (_try_parry(target, pos)) {
		target = _resolve(target_id);
	}
	float turn = 0.0f;
	if (phase == PHASE_BALLISTIC) {
		velocity.y -= profile->get_ballistic_gravity() * dt; // pure arc: no steering, ever
	} else {
		_update_phase(target, pos);
		if (phase == PHASE_SEEK && target) {
			turn = _steer(target, pos, dt);
		} else {
			aim = pos + velocity; // not steering: "aiming" straight ahead
		}
		bool climbing = phase == PHASE_LAUNCH && !parried;
		velocity.y -= (climbing ? profile->get_launch_gravity() : profile->get_gravity()) * dt;
	}

	Vector3 next = pos + velocity * dt;
	if (_sweep(target, pos, next)) {
		return;
	}
	set_global_position(next);
	_orient();

	visual.update(dt, age, turn, _shake(target, next));
	_update_telegraph(target, next, dt);
	if (debug_draw) {
		_draw_debug(next);
	}

	if (age >= profile->get_lifetime()) {
		emit_signal("expired");
		_set_phase(PHASE_POP);
	}
}

} // namespace godot
