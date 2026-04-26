#ifndef BT_LEAVES_H
#define BT_LEAVES_H

#include "bt_task.h"
#include "../enemy/enemy_base.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/math.hpp>

namespace godot {

// BTActionWait
class BTWait : public BTTask {
	GDCLASS(BTWait, BTTask)

private:
	float duration = 0.0f;
	float elapsed = 0.0f;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_duration", "duration"), &BTWait::set_duration);
		ClassDB::bind_method(D_METHOD("get_duration"), &BTWait::get_duration);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration"), "set_duration", "get_duration");
	}

public:
	void set_duration(float p_duration) { duration = p_duration; }
	float get_duration() const { return duration; }

	static Ref<BTWait> create(float p_duration) {
		Ref<BTWait> node;
		node.instantiate();
		node->set_duration(p_duration);
		return node;
	}

	virtual void _enter(Node *p_actor, const Ref<Blackboard> &p_blackboard) override { 
		elapsed = 0.0f; 
	}

	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override {
		elapsed += p_actor->get_process_delta_time();
		if (elapsed >= duration) {
			return SUCCESS;
		}
		return RUNNING;
	}
};

// BTConditionInRange
class BTIsInRange : public BTTask {
	GDCLASS(BTIsInRange, BTTask)

private:
	float range = 10.0f;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_range", "range"), &BTIsInRange::set_range);
		ClassDB::bind_method(D_METHOD("get_range"), &BTIsInRange::get_range);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "range"), "set_range", "get_range");
	}

public:
	void set_range(float p_range) { range = p_range; }
	float get_range() const { return range; }

	static Ref<BTIsInRange> create(float p_range) {
		Ref<BTIsInRange> node;
		node.instantiate();
		node->set_range(p_range);
		return node;
	}

	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override {
		Node3D *actor3d = Object::cast_to<Node3D>(p_actor);
		if (!actor3d) return FAILURE;

		// Try to get target from blackboard
		Node3D *target = Object::cast_to<Node3D>(p_blackboard->get_value("target"));
		if (!target) return FAILURE;

		float dist = actor3d->get_global_position().distance_to(target->get_global_position());
		return (dist <= range) ? SUCCESS : FAILURE;
	}
};

// BTActionShoot
class BTActionShoot : public BTTask {
	GDCLASS(BTActionShoot, BTTask)

protected:
	static void _bind_methods() {}

public:
	static Ref<BTActionShoot> create() {
		Ref<BTActionShoot> node;
		node.instantiate();
		return node;
	}

	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override {
		EnemyBase *enemy = Object::cast_to<EnemyBase>(p_actor);
		if (enemy) {
			enemy->shoot();
			return SUCCESS;
		}
		return FAILURE;
	}
};

// BTActionApproach
class BTActionApproach : public BTTask {
	GDCLASS(BTActionApproach, BTTask)

private:
	float speed = 5.0f;
	float stop_distance = 1.5f;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_speed", "speed"), &BTActionApproach::set_speed);
		ClassDB::bind_method(D_METHOD("get_speed"), &BTActionApproach::get_speed);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");

		ClassDB::bind_method(D_METHOD("set_stop_distance", "distance"), &BTActionApproach::set_stop_distance);
		ClassDB::bind_method(D_METHOD("get_stop_distance"), &BTActionApproach::get_stop_distance);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stop_distance"), "set_stop_distance", "get_stop_distance");
	}

public:
	void set_speed(float p_speed) { speed = p_speed; }
	float get_speed() const { return speed; }
	void set_stop_distance(float p_dist) { stop_distance = p_dist; }
	float get_stop_distance() const { return stop_distance; }

	static Ref<BTActionApproach> create(float p_speed, float p_stop_distance) {
		Ref<BTActionApproach> node;
		node.instantiate();
		node->set_speed(p_speed);
		node->set_stop_distance(p_stop_distance);
		return node;
	}

	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override {
		CharacterBody3D *actor3d = Object::cast_to<CharacterBody3D>(p_actor);
		if (!actor3d) return FAILURE;

		Node3D *target = Object::cast_to<Node3D>(p_blackboard->get_value("target"));
		if (!target) return FAILURE;

		Vector3 pos = actor3d->get_global_position();
		Vector3 target_pos = target->get_global_position();
		
		// 2D distance for stopping
		Vector3 pos_2d = Vector3(pos.x, 0, pos.z);
		Vector3 target_2d = Vector3(target_pos.x, 0, target_pos.z);
		float dist = pos_2d.distance_to(target_2d);

		if (dist <= stop_distance) {
			Vector3 vel = actor3d->get_velocity();
			vel.x = 0; vel.z = 0;
			actor3d->set_velocity(vel);
			return SUCCESS;
		}

		Vector3 dir = (target_pos - pos).normalized();
		Vector3 vel = actor3d->get_velocity();
		vel.x = dir.x * speed;
		vel.z = dir.z * speed;
		actor3d->set_velocity(vel);

		return RUNNING;
	}
};

// BTPatrol
class BTPatrol : public BTTask {
	GDCLASS(BTPatrol, BTTask)

private:
	float speed = 2.0f;
	float patrol_radius = 10.0f;
	Vector3 home_pos;
	Vector3 target_pos;
	bool has_target = false;
	bool home_set = false;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_speed", "speed"), &BTPatrol::set_speed);
		ClassDB::bind_method(D_METHOD("get_speed"), &BTPatrol::get_speed);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");

		ClassDB::bind_method(D_METHOD("set_patrol_radius", "radius"), &BTPatrol::set_patrol_radius);
		ClassDB::bind_method(D_METHOD("get_patrol_radius"), &BTPatrol::get_patrol_radius);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "patrol_radius"), "set_patrol_radius", "get_patrol_radius");
	}

public:
	void set_speed(float p_speed) { speed = p_speed; }
	float get_speed() const { return speed; }
	void set_patrol_radius(float p_radius) { patrol_radius = p_radius; }
	float get_patrol_radius() const { return patrol_radius; }

	static Ref<BTPatrol> create(float p_speed, float p_patrol_radius = 10.0f) {
		Ref<BTPatrol> node;
		node.instantiate();
		node->set_speed(p_speed);
		node->set_patrol_radius(p_patrol_radius);
		return node;
	}

	virtual void _enter(Node *p_actor, const Ref<Blackboard> &p_blackboard) override {
		CharacterBody3D *actor3d = Object::cast_to<CharacterBody3D>(p_actor);
		if (actor3d && !home_set) {
			home_pos = actor3d->get_global_position();
			home_set = true;
		}
	}

	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override {
		CharacterBody3D *actor3d = Object::cast_to<CharacterBody3D>(p_actor);
		if (!actor3d) return FAILURE;

		if (!has_target) {
			float angle = UtilityFunctions::randf_range(0, Math_TAU);
			float dist = UtilityFunctions::randf_range(0, patrol_radius);
			target_pos = home_pos + Vector3(Math::cos(angle) * dist, 0, Math::sin(angle) * dist);
			has_target = true;
		}

		Vector3 pos = actor3d->get_global_position();
		Vector3 pos_2d = Vector3(pos.x, 0, pos.z);
		Vector3 target_2d = Vector3(target_pos.x, 0, target_pos.z);
		float dist = pos_2d.distance_to(target_2d);

		if (dist <= 0.5f) {
			has_target = false;
			Vector3 vel = actor3d->get_velocity();
			vel.x = 0; vel.z = 0;
			actor3d->set_velocity(vel);
			return SUCCESS;
		}

		Vector3 dir = (target_pos - pos).normalized();
		Vector3 vel = actor3d->get_velocity();
		vel.x = dir.x * speed;
		vel.z = dir.z * speed;
		actor3d->set_velocity(vel);

		return RUNNING;
	}
};

// BTActionMelee
class BTActionMelee : public BTTask {
	GDCLASS(BTActionMelee, BTTask)

protected:
	static void _bind_methods() {}

public:
	static Ref<BTActionMelee> create() {
		Ref<BTActionMelee> node;
		node.instantiate();
		return node;
	}

	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override {
		EnemyBase *enemy = Object::cast_to<EnemyBase>(p_actor);
		if (enemy) {
			enemy->melee_attack();
			return SUCCESS;
		}
		return FAILURE;
	}
};

} // namespace godot

#endif // BT_LEAVES_H
