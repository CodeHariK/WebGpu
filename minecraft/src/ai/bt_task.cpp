#include "bt_task.h"

namespace godot {

void BTTask::_bind_methods() {
	ClassDB::bind_method(D_METHOD("execute", "actor", "blackboard"), &BTTask::execute);
	ClassDB::bind_method(D_METHOD("abort", "actor", "blackboard"), &BTTask::abort);
	ClassDB::bind_method(D_METHOD("get_status"), &BTTask::get_status);
	ClassDB::bind_method(D_METHOD("set_status", "status"), &BTTask::set_status);

	BIND_ENUM_CONSTANT(IDLE);
	BIND_ENUM_CONSTANT(RUNNING);
	BIND_ENUM_CONSTANT(SUCCESS);
	BIND_ENUM_CONSTANT(FAILURE);
}

BTTask::BTTask() {}

BTTask::~BTTask() {}

BTTask::Status BTTask::execute(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (status != RUNNING) {
		_enter(p_actor, p_blackboard);
	}

	status = _tick(p_actor, p_blackboard);

	if (status != RUNNING) {
		_exit(p_actor, p_blackboard);
	}

	return status;
}

void BTTask::abort(Node *p_actor, const Ref<Blackboard> &p_blackboard) {
	if (status == RUNNING) {
		_exit(p_actor, p_blackboard);
	}
	status = IDLE;
}

} // namespace godot
