#include "bt_decorators.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/time.hpp>

namespace godot {

void BTDecorator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_child", "child"), &BTDecorator::set_child);
	ClassDB::bind_method(D_METHOD("get_child"), &BTDecorator::get_child);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "child", PROPERTY_HINT_RESOURCE_TYPE, "BTTask"), "set_child", "get_child");
}

BTDecorator::BTDecorator() {}
BTDecorator::~BTDecorator() {}

void BTDecorator::set_child(const Ref<BTTask> &p_child) {
	child = p_child;
}

Ref<BTTask> BTDecorator::get_child() const {
	return child;
}

void BTDecorator::abort(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (get_status() == RUNNING && child.is_valid()) {
		child->abort(p_actor, p_blackboard);
	}
	BTTask::abort(p_actor, p_blackboard);
}

// BTInverter
BTTask::Status BTInverter::_tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (child.is_null()) return SUCCESS;

	Status status = child->execute(p_actor, p_blackboard);
	if (status == SUCCESS) return FAILURE;
	if (status == FAILURE) return SUCCESS;
	return RUNNING;
}

// BTForceSuccess
BTTask::Status BTForceSuccess::_tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (child.is_null()) return SUCCESS;

	Status status = child->execute(p_actor, p_blackboard);
	if (status == RUNNING) return RUNNING;
	return SUCCESS;
}

// BTForceFailure
BTTask::Status BTForceFailure::_tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (child.is_null()) return FAILURE;

	Status status = child->execute(p_actor, p_blackboard);
	if (status == RUNNING) return RUNNING;
	return FAILURE;
}

// BTProbability
void BTProbability::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_run_chance", "chance"), &BTProbability::set_run_chance);
	ClassDB::bind_method(D_METHOD("get_run_chance"), &BTProbability::get_run_chance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "run_chance"), "set_run_chance", "get_run_chance");
}

BTTask::Status BTProbability::_tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (child.is_null()) return SUCCESS;

	if (get_status() != RUNNING) {
		float roll = UtilityFunctions::randf();
		if (roll > run_chance) {
			return FAILURE;
		}
	}

	return child->execute(p_actor, p_blackboard);
}

// BTRepeat
void BTRepeat::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_repeat_times", "times"), &BTRepeat::set_repeat_times);
	ClassDB::bind_method(D_METHOD("get_repeat_times"), &BTRepeat::get_repeat_times);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "repeat_times"), "set_repeat_times", "get_repeat_times");
}

BTTask::Status BTRepeat::_tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (child.is_null()) return SUCCESS;

	Status status = child->execute(p_actor, p_blackboard);

	if (status == RUNNING) return RUNNING;

	current_count++;

	if (repeat_times >= 0 && current_count >= repeat_times) {
		return SUCCESS;
	}

	return RUNNING;
}

// BTRepeatUntilSuccess
BTTask::Status BTRepeatUntilSuccess::_tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (child.is_null()) return SUCCESS;

	Status status = child->execute(p_actor, p_blackboard);

	if (status == SUCCESS) {
		return SUCCESS;
	}

	return RUNNING;
}

// BTRepeatUntilFailure
BTTask::Status BTRepeatUntilFailure::_tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (child.is_null()) return FAILURE;

	Status status = child->execute(p_actor, p_blackboard);

	if (status == FAILURE) {
		return SUCCESS;
	}

	return RUNNING;
}

// BTDelay
void BTDelay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_delay", "delay"), &BTDelay::set_delay);
	ClassDB::bind_method(D_METHOD("get_delay"), &BTDelay::get_delay);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "delay"), "set_delay", "get_delay");
}

BTTask::Status BTDelay::_tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (elapsed < delay) {
		elapsed += p_actor->get_process_delta_time();
		return RUNNING;
	}
	if (child.is_null()) return SUCCESS;
	return child->execute(p_actor, p_blackboard);
}

// BTCooldown
void BTCooldown::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_cooldown", "cooldown"), &BTCooldown::set_cooldown);
	ClassDB::bind_method(D_METHOD("get_cooldown"), &BTCooldown::get_cooldown);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cooldown"), "set_cooldown", "get_cooldown");
}

BTTask::Status BTCooldown::_tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	uint64_t current_ticks = godot::Time::get_singleton()->get_ticks_msec();
	if (is_in_cooldown) {
		float elapsed_sec = (current_ticks - last_run_ticks) / 1000.0f;
		if (elapsed_sec < cooldown) {
			return FAILURE;
		} else {
			is_in_cooldown = false;
		}
	}

	if (child.is_null()) return SUCCESS;

	Status status = child->execute(p_actor, p_blackboard);
	if (status != RUNNING) {
		last_run_ticks = godot::Time::get_singleton()->get_ticks_msec();
		is_in_cooldown = true;
	}
	return status;
}

// BTRunLimit
void BTRunLimit::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_run_limit", "limit"), &BTRunLimit::set_run_limit);
	ClassDB::bind_method(D_METHOD("get_run_limit"), &BTRunLimit::get_run_limit);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "run_limit"), "set_run_limit", "get_run_limit");
}

BTTask::Status BTRunLimit::_tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (num_runs >= run_limit) return FAILURE;

	if (child.is_null()) return SUCCESS;

	Status status = child->execute(p_actor, p_blackboard);
	if (status == SUCCESS || status == FAILURE) {
		num_runs++;
	}
	return status;
}

// BTTimeLimit
void BTTimeLimit::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_time_limit", "limit"), &BTTimeLimit::set_time_limit);
	ClassDB::bind_method(D_METHOD("get_time_limit"), &BTTimeLimit::get_time_limit);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_limit"), "set_time_limit", "get_time_limit");
}

BTTask::Status BTTimeLimit::_tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (child.is_null()) return SUCCESS;

	elapsed += p_actor->get_process_delta_time();
	if (elapsed >= time_limit) {
		if (child->get_status() == RUNNING) {
			child->abort(p_actor, p_blackboard);
		}
		return FAILURE;
	}

	return child->execute(p_actor, p_blackboard);
}

} // namespace godot
