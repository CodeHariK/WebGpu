#ifndef BT_TASK_H
#define BT_TASK_H

#include "bt_store.h"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>

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

	virtual void _enter(Node *p_actor, const Ref<BTStore> &p_btstore) {}
	virtual Status _tick(Node *p_actor, const Ref<BTStore> &p_btstore) { return SUCCESS; }
	virtual void _exit(Node *p_actor, const Ref<BTStore> &p_btstore) {}

public:
	BTTask();
	virtual ~BTTask();

	Status execute(Node *p_actor, const Ref<BTStore> &p_btstore);

	Status get_status() const { return status; }
	void set_status(Status p_status) { status = p_status; }

	virtual void abort(Node *p_actor, const Ref<BTStore> &p_btstore);
};

} // namespace godot

VARIANT_ENUM_CAST(BTTask::Status);

#endif // BT_TASK_H
