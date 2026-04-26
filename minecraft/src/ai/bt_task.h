#ifndef BT_TASK_H
#define BT_TASK_H

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/node.hpp>
#include "blackboard.h"

namespace godot {

class BTTask : public RefCounted {
	GDCLASS(BTTask, RefCounted)

public:
	enum Status {
		IDLE,
		RUNNING,
		SUCCESS,
		FAILURE
	};

private:
	Status status = IDLE;

protected:
	static void _bind_methods();

	virtual void _enter(Node *p_actor, const Ref<Blackboard> &p_blackboard) {}
	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) { return SUCCESS; }
	virtual void _exit(Node *p_actor, const Ref<Blackboard> &p_blackboard) {}

public:
	BTTask();
	virtual ~BTTask();

	Status execute(Node *p_actor, const Ref<Blackboard> &p_blackboard);
	
	Status get_status() const { return status; }
	void set_status(Status p_status) { status = p_status; }

	virtual void abort(Node *p_actor, const Ref<Blackboard> &p_blackboard);
};

} // namespace godot

VARIANT_ENUM_CAST(BTTask::Status);

#endif // BT_TASK_H
