#ifndef BT_LEAVES_H
#define BT_LEAVES_H

#include "bt_task.h"
#include "../enemy/enemy_base.h"

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
	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override {
		EnemyBase *enemy = Object::cast_to<EnemyBase>(p_actor);
		if (enemy) {
			enemy->shoot();
			return SUCCESS;
		}
		return FAILURE;
	}
};

} // namespace godot

#endif // BT_LEAVES_H
