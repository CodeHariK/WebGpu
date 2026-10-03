#include "lift_throw.h"

#include "../camera/camera.h"
#include "../combat/projectile/ballistic_arc.h"
#include "../combat/projectile/projectile.h"
#include "../combat/trajectory_preview.h"
#include "../enemy/enemy_base.h"
#include "../enemy/enemy_manager.h"
#include "../game_manager/game_manager.h"
#include "../utils/body_velocity.h"
#include "../vehicle/arcade_vehicle.h"

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/object.hpp>

#include <algorithm>
#include <cmath>

namespace godot {

static const float HOLD_GAP = 0.15f; // space between head and the held body (m)
static const float CARRY_STIFF = 120.0f; // carry follow spring (slightly underdamped: it sways)
static const float CARRY_DAMP = 16.0f;
static const float HOLD_BOB = 0.08f; // m
static const float TUMBLE = 5.0f; // end-over-end spin on a throw (rad/s)
static const float WATCH_TIME = 3.0f; // how long a thrown body can still hit things (s)
static const float UNEXCEPT_AFTER = 0.4f; // carrier collision restored this long after the throw
static const float ENEMY_RADIUS = 1.4f; // rough enemy size for the impact test (m)
static const float MIN_FLIGHT = 0.5f; // shortest auto-aimed throw (s)
static const float AIM_CONE = 0.5f; // auto-aim needs dot(camera forward, to target) above this

void LiftThrow::setup(
		Node3D *p_owner,
		float p_owner_half_height
) {
	owner = p_owner;
	owner_half_height = p_owner_half_height;
	preview = memnew(TrajectoryPreview);
	preview->set_name("ThrowTrajectory");
	preview->set_color(Color(1.0f, 0.75f, 0.35f, 0.9f));
	preview->set_max_time(4.0f);
	owner->add_child(preview);
	puffs.build(owner, 16, 0.4f, Color(0.95f, 0.92f, 0.85f));
}

// The carried / flying body must not count as a wall for the follow camera's
// collision rays, or it drags the camera in right behind the player's head.
static void _camera_ignore(
		const RID &p_rid,
		bool p_ignore
) {
	GameManager *gm = GameManager::get_singleton();
	GameCamera *cam = gm ? gm->get_camera() : nullptr;
	if (!cam || !p_rid.is_valid()) {
		return;
	}
	if (p_ignore) {
		cam->add_ray_exclude(p_rid);
	} else {
		cam->remove_ray_exclude(p_rid);
	}
}

RigidBody3D *LiftThrow::_resolve(uint64_t p_id) const {
	RigidBody3D *b = Object::cast_to<RigidBody3D>(ObjectDB::get_instance(p_id));
	return (b && b->is_inside_tree()) ? b : nullptr;
}

float LiftThrow::_radius_of(RigidBody3D *p_body) {
	float r = 0.5f;
	for (int i = 0; i < p_body->get_child_count(); i++) {
		CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(p_body->get_child(i));
		if (!cs || cs->get_shape().is_null()) {
			continue;
		}
		Ref<Shape3D> s = cs->get_shape();
		float lr = 0.5f;
		if (Ref<SphereShape3D> sp = s; sp.is_valid()) {
			lr = sp->get_radius();
		} else if (Ref<BoxShape3D> bx = s; bx.is_valid()) {
			lr = bx->get_size().length() * 0.5f;
		} else if (Ref<CapsuleShape3D> cp = s; cp.is_valid()) {
			lr = MAX(cp->get_radius(), cp->get_height() * 0.5f);
		} else if (Ref<CylinderShape3D> cy = s; cy.is_valid()) {
			lr = MAX(cy->get_radius(), cy->get_height() * 0.5f);
		}
		r = MAX(r, lr + cs->get_position().length());
	}
	return r;
}

float LiftThrow::_gravity_of(RigidBody3D *p_body) {
	float g = (float)ProjectSettings::get_singleton()->get_setting("physics/3d/default_gravity", 9.8);
	g *= p_body->get_gravity_scale();
	// An airborne ArcadeVehicle also pushes itself down (AirborneState downforce).
	if (ArcadeVehicle *car = Object::cast_to<ArcadeVehicle>(p_body)) {
		Ref<VehicleConfig> cfg = car->get_vehicle_config();
		if (cfg.is_valid() && p_body->get_mass() > 0.0f) {
			g += cfg->get_downforce() / p_body->get_mass();
		}
	}
	return g;
}

RigidBody3D *LiftThrow::_find_liftable() const {
	Ref<World3D> world = owner->get_world_3d();
	PhysicsDirectSpaceState3D *space = world.is_valid() ? world->get_direct_space_state() : nullptr;
	if (!space) {
		return nullptr;
	}
	Ref<SphereShape3D> sphere;
	sphere.instantiate();
	sphere->set_radius(reach);
	Ref<PhysicsShapeQueryParameters3D> q;
	q.instantiate();
	q->set_shape(sphere);
	q->set_transform(Transform3D(Basis(), owner->get_global_position()));
	q->set_collide_with_bodies(true);
	TypedArray<Dictionary> hits = space->intersect_shape(q, 16);

	RigidBody3D *best = nullptr;
	float best_d = 1e9f;
	Vector3 pos = owner->get_global_position();
	for (int i = 0; i < hits.size(); i++) {
		Dictionary d = hits[i];
		RigidBody3D *b = Object::cast_to<RigidBody3D>(d["collider"]);
		if (!b || b->is_freeze_enabled()) {
			continue; // static-ish or already held by someone
		}
		float dist = b->get_global_position().distance_to(pos) - _radius_of(b);
		if (dist < best_d) {
			best_d = dist;
			best = b;
		}
	}
	return best;
}

Vector3 LiftThrow::_hold_point() const {
	float bob = HOLD_BOB * std::sin(clock * 5.0f);
	return owner->get_global_position() + Vector3(0, owner_half_height + held_radius + HOLD_GAP + bob, 0);
}

void LiftThrow::_lift(RigidBody3D *p_body) {
	held_id = p_body->get_instance_id();
	saved_freeze = p_body->is_freeze_enabled();
	saved_freeze_mode = (int)p_body->get_freeze_mode();
	held_radius = _radius_of(p_body);
	p_body->set_freeze_mode(RigidBody3D::FREEZE_MODE_KINEMATIC);
	p_body->set_freeze_enabled(true);
	p_body->set_linear_velocity(Vector3());
	p_body->set_angular_velocity(Vector3());
	if (PhysicsBody3D *pb = Object::cast_to<PhysicsBody3D>(owner)) {
		pb->add_collision_exception_with(p_body);
		p_body->add_collision_exception_with(pb);
	}
	camera_rid = p_body->get_rid();
	_camera_ignore(camera_rid, true); // until the throw has landed (see _watch_thrown)
	hoist_from = p_body->get_global_transform();
	hoist_t = 0.0f;
	yaw_offset = hoist_from.basis.get_euler().y - owner->get_global_rotation().y;
	carry_pos = hoist_from.origin;
	carry_vel = Vector3();
	phase = PHASE_HOIST;
	aiming = false;
}

void LiftThrow::_carry(
		RigidBody3D *p_body,
		float p_dt
) {
	Vector3 hold = _hold_point();
	float yaw = owner->get_global_rotation().y + yaw_offset;
	Quaternion upright(Vector3(0, 1, 0), yaw);

	if (phase == PHASE_HOIST) {
		hoist_t += p_dt;
		float t = CLAMP(hoist_t / hoist_time, 0.0f, 1.0f);
		const float c = 1.70158f; // ease-out-back: swings past overhead, settles
		float e = 1.0f + (c + 1.0f) * std::pow(t - 1.0f, 3.0f) + c * std::pow(t - 1.0f, 2.0f);
		carry_pos = hoist_from.origin.lerp(hold, e);
		Quaternion q = hoist_from.basis.get_rotation_quaternion().slerp(upright, t);
		p_body->set_global_transform(Transform3D(Basis(q), carry_pos));
		if (t >= 1.0f) {
			phase = PHASE_HOLD;
			carry_vel = Vector3();
		}
		return;
	}
	// Held: follow the hold point on a spring, so it lags and sways like something heavy.
	carry_vel += ((hold - carry_pos) * CARRY_STIFF - carry_vel * CARRY_DAMP) * p_dt;
	carry_pos += carry_vel * p_dt;
	float sway = CLAMP(-carry_vel.length() * 0.02f, -0.15f, 0.15f);
	p_body->set_global_transform(Transform3D(Basis(upright) * Basis(Vector3(1, 0, 0), sway), carry_pos));
}

Node3D *LiftThrow::_find_target(
		GameCamera *p_cam,
		const Vector3 &p_from
) const {
	EnemyManager *em = EnemyManager::get_singleton();
	if (!em) {
		return nullptr;
	}
	float yaw = p_cam->get_yaw();
	Vector3 fwd(-std::sin(yaw), 0.0f, -std::cos(yaw));
	Node3D *best = nullptr;
	float best_score = -1e9f;
	for (const EnemyData &e : em->get_enemies()) {
		Node3D *n = e.node;
		if (!n || !n->is_inside_tree() || n->is_queued_for_deletion()) {
			continue;
		}
		EnemyBase *eb = Object::cast_to<EnemyBase>(n);
		if (eb && eb->get_is_dead()) {
			continue;
		}
		Vector3 to = n->get_global_position() - p_from;
		float dist = to.length();
		Vector3 flat(to.x, 0.0f, to.z);
		if (dist > throw_range || flat.length() < 0.5f) {
			continue;
		}
		float dot = fwd.dot(flat.normalized());
		if (dot < AIM_CONE) {
			continue;
		}
		// Look direction matters most; flyers get a bonus (that's what throwing is for).
		float score = dot * 3.0f - dist / throw_range + (to.y > 3.0f ? 0.5f : 0.0f);
		if (score > best_score) {
			best_score = score;
			best = n;
		}
	}
	return best;
}

Vector3 LiftThrow::_throw_velocity(
		GameCamera *p_cam,
		RigidBody3D *p_body
) {
	Vector3 from = p_body->get_global_position();
	float g = _gravity_of(p_body);
	aim_yaw = p_cam->get_yaw();

	if (Node3D *target = _find_target(p_cam, from)) {
		Vector3 goal = target->get_global_position();
		Vector3 drift = body_velocity(target);
		float t = BallisticArc::flight_time(from, goal, throw_speed, MIN_FLIGHT);
		for (int i = 0; i < 2; i++) {
			t = BallisticArc::flight_time(from, goal + drift * t, throw_speed, MIN_FLIGHT);
		}
		Vector3 aim = goal + drift * t;
		Vector3 flat(aim.x - from.x, 0.0f, aim.z - from.z);
		if (flat.length_squared() > 1e-4f) {
			aim_yaw = Math::atan2(-flat.x, -flat.z);
		}
		return BallisticArc::launch_velocity(from, aim, t, g);
	}
	float pitch = CLAMP(p_cam->get_pitch() + loft, -0.3f, 1.2f);
	return Vector3(0, 0, -1).rotated(Vector3(1, 0, 0), pitch).rotated(Vector3(0, 1, 0), aim_yaw) * throw_speed;
}

void LiftThrow::_release(RigidBody3D *p_body) {
	p_body->set_freeze_enabled(saved_freeze);
	p_body->set_freeze_mode((RigidBody3D::FreezeMode)saved_freeze_mode);
	phase = PHASE_NONE;
	aiming = false;
	held_id = 0;
	if (preview) {
		preview->hide_arc();
	}
}

void LiftThrow::_throw(
		RigidBody3D *p_body,
		const Vector3 &p_velocity,
		GameCamera *p_cam
) {
	_release(p_body);
	p_body->set_linear_velocity(p_velocity);
	// End-over-end tumble around the throw's sideways axis.
	Vector3 side = Vector3(0, 1, 0).cross(p_velocity);
	if (side.length_squared() > 1e-4f) {
		p_body->set_angular_velocity(side.normalized() * -TUMBLE);
	}
	thrown_id = p_body->get_instance_id();
	thrown_t = 0.0f;
	thrown_radius = held_radius;
	already_hit.clear();
}

void LiftThrow::_watch_thrown(float p_dt) {
	if (thrown_t < 0.0f) {
		return;
	}
	thrown_t += p_dt;
	RigidBody3D *body = _resolve(thrown_id);
	if (!body || thrown_t > WATCH_TIME) {
		thrown_t = -1.0f;
		if (body) {
			if (PhysicsBody3D *pb = Object::cast_to<PhysicsBody3D>(owner)) {
				pb->remove_collision_exception_with(body);
				body->remove_collision_exception_with(pb);
			}
		}
		_camera_ignore(camera_rid, false); // landed (or gone): a normal camera obstacle again
		camera_rid = RID();
		return;
	}
	if (thrown_t > UNEXCEPT_AFTER) {
		if (PhysicsBody3D *pb = Object::cast_to<PhysicsBody3D>(owner)) {
			pb->remove_collision_exception_with(body);
			body->remove_collision_exception_with(pb);
		}
	}
	EnemyManager *em = EnemyManager::get_singleton();
	if (!em) {
		return;
	}
	Vector3 pos = body->get_global_position();
	std::vector<EnemyData> enemies = em->get_enemies(); // copy: a hit can unregister one
	for (const EnemyData &e : enemies) {
		if (!e.node || !e.node->is_inside_tree()) {
			continue;
		}
		uint64_t id = e.node->get_instance_id();
		if (std::find(already_hit.begin(), already_hit.end(), id) != already_hit.end()) {
			continue;
		}
		if (e.node->get_global_position().distance_to(pos) > thrown_radius + ENEMY_RADIUS) {
			continue;
		}
		already_hit.push_back(id);
		if (EnemyBase *eb = Object::cast_to<EnemyBase>(e.node)) {
			eb->take_damage(impact_damage);
		}
		puffs.burst(e.node->get_global_position(), 8, 6.0f, 0.6f, 1.5f);
		// Cartoon bounce: the body loses most of its speed and pops up off the victim.
		body->set_linear_velocity(body->get_linear_velocity() * 0.35f + Vector3(0, 4.0f, 0));
	}
}

void LiftThrow::drop() {
	if (RigidBody3D *body = _resolve(held_id)) {
		_release(body);
		thrown_id = body->get_instance_id(); // let the watcher restore the carrier collision
		thrown_t = WATCH_TIME;
	} else {
		phase = PHASE_NONE;
		aiming = false;
	}
}

void LiftThrow::update(
		float p_dt,
		bool p_held,
		bool p_just_pressed,
		GameCamera *p_cam,
		bool p_active
) {
	clock += p_dt;
	puffs.update(p_dt);
	_watch_thrown(p_dt);
	if (!owner) {
		return;
	}
	if (!p_active) {
		if (is_holding()) {
			drop();
		}
		return;
	}

	if (phase == PHASE_NONE) {
		if (p_just_pressed) {
			if (RigidBody3D *b = _find_liftable()) {
				_lift(b);
			}
		}
		return;
	}

	RigidBody3D *body = _resolve(held_id);
	if (!body) {
		phase = PHASE_NONE;
		aiming = false;
		return;
	}
	_carry(body, p_dt);

	// A NEW press while holding starts the aim; releasing it throws.
	if (phase == PHASE_HOLD && p_just_pressed) {
		aiming = true;
	}
	if (!aiming || !p_cam) {
		return;
	}
	Vector3 vel = _throw_velocity(p_cam, body);
	if (p_held) {
		TypedArray<RID> exclude;
		exclude.push_back(body->get_rid());
		if (CollisionObject3D *co = Object::cast_to<CollisionObject3D>(owner)) {
			exclude.push_back(co->get_rid());
		}
		preview->show_arc(body->get_global_position(), vel, _gravity_of(body), p_dt, Projectile::get_hit_mask(), exclude);
		return;
	}
	_throw(body, vel, p_cam);
}

} // namespace godot
