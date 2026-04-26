#ifndef BT_DECORATORS_H
#define BT_DECORATORS_H

#include "bt_task.h"

namespace godot {

class BTDecorator : public BTTask {
	GDCLASS(BTDecorator, BTTask)

protected:
	Ref<BTTask> child;

	static void _bind_methods();

public:
	BTDecorator();
	~BTDecorator();

	void set_child(const Ref<BTTask> &p_child);
	Ref<BTTask> get_child() const;

	virtual void abort(Node *p_actor, const Ref<Blackboard> &p_blackboard) override;
};

class BTInverter : public BTDecorator {
	GDCLASS(BTInverter, BTDecorator)

protected:
	static void _bind_methods() {}
	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override;
};

class BTForceSuccess : public BTDecorator {
	GDCLASS(BTForceSuccess, BTDecorator)

protected:
	static void _bind_methods() {}
	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override;
};

class BTForceFailure : public BTDecorator {
	GDCLASS(BTForceFailure, BTDecorator)

protected:
	static void _bind_methods() {}
	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override;
};

class BTProbability : public BTDecorator {
	GDCLASS(BTProbability, BTDecorator)

private:
	float run_chance = 0.5f;

protected:
	static void _bind_methods();
	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override;

public:
	void set_run_chance(float p_chance) { run_chance = p_chance; }
	float get_run_chance() const { return run_chance; }
};

class BTRepeat : public BTDecorator {
	GDCLASS(BTRepeat, BTDecorator)

private:
	int repeat_times = -1;
	int current_count = 0;

protected:
	static void _bind_methods();

public:
	void set_repeat_times(int p_times) { repeat_times = p_times; }
	int get_repeat_times() const { return repeat_times; }

	virtual void _enter(Node *p_actor, const Ref<Blackboard> &p_blackboard) override { current_count = 0; }
	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override;
};

class BTRepeatUntilSuccess : public BTDecorator {
	GDCLASS(BTRepeatUntilSuccess, BTDecorator)

protected:
	static void _bind_methods() {}
	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override;
};

class BTRepeatUntilFailure : public BTDecorator {
	GDCLASS(BTRepeatUntilFailure, BTDecorator)

protected:
	static void _bind_methods() {}
	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override;
};

class BTDelay : public BTDecorator {
	GDCLASS(BTDelay, BTDecorator)

private:
	float delay = 1.0f;
	float elapsed = 0.0f;

protected:
	static void _bind_methods();

public:
	void set_delay(float p_delay) { delay = p_delay; }
	float get_delay() const { return delay; }

	virtual void _enter(Node *p_actor, const Ref<Blackboard> &p_blackboard) override { elapsed = 0.0f; }
	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override;
};

class BTCooldown : public BTDecorator {
	GDCLASS(BTCooldown, BTDecorator)

private:
	float cooldown = 1.0f;
	uint64_t last_run_ticks = 0;
	bool is_in_cooldown = false;

protected:
	static void _bind_methods();

public:
	void set_cooldown(float p_cooldown) { cooldown = p_cooldown; }
	float get_cooldown() const { return cooldown; }

	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override;
};

class BTRunLimit : public BTDecorator {
	GDCLASS(BTRunLimit, BTDecorator)

private:
	int run_limit = 1;
	int num_runs = 0;

protected:
	static void _bind_methods();

public:
	void set_run_limit(int p_limit) { run_limit = p_limit; }
	int get_run_limit() const { return run_limit; }

	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override;
};

class BTTimeLimit : public BTDecorator {
	GDCLASS(BTTimeLimit, BTDecorator)

private:
	float time_limit = 1.0f;
	float elapsed = 0.0f;

protected:
	static void _bind_methods();

public:
	void set_time_limit(float p_limit) { time_limit = p_limit; }
	float get_time_limit() const { return time_limit; }

	virtual void _enter(Node *p_actor, const Ref<Blackboard> &p_blackboard) override { elapsed = 0.0f; }
	virtual Status _tick(Node *p_actor, const Ref<Blackboard> &p_blackboard) override;
};

} // namespace godot

#endif // BT_DECORATORS_H
