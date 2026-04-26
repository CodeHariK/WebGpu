#ifndef BT_COMPOSITES_H
#define BT_COMPOSITES_H

#include "bt_task.h"
#include <godot_cpp/templates/vector.hpp>
#include <initializer_list>

namespace godot {

class BTComposite : public BTTask {
	GDCLASS(BTComposite, BTTask)

protected:
	Vector<Ref<BTTask>> children;
	int current_child_index = 0;

	static void _bind_methods();

public:
	BTComposite();
	~BTComposite();

	void add_child(const Ref<BTTask> &p_child);
	void remove_child(const Ref<BTTask> &p_child);
	void clear_children();

	void add_children(std::initializer_list<Ref<BTTask>> p_children) {
		for (const Ref<BTTask> &child : p_children) {
			add_child(child);
		}
	}

	virtual void abort(Node *p_actor, const Ref<BTStore> &p_btstore) override {
		if (get_status() == RUNNING) {
			if (current_child_index >= 0 && current_child_index < children.size()) {
				children.write[current_child_index]->abort(p_actor, p_btstore);
			}
		}
		BTTask::abort(p_actor, p_btstore);
	}
};

class BTSequence : public BTComposite {
	GDCLASS(BTSequence, BTComposite)

protected:
	static void _bind_methods() {}

public:
	static Ref<BTSequence> create(std::initializer_list<Ref<BTTask>> p_children = {}) {
		Ref<BTSequence> node;
		node.instantiate();
		node->add_children(p_children);
		return node;
	}

	virtual void _enter(Node *p_actor, const Ref<BTStore> &p_btstore) override {
		current_child_index = 0;
	}

	virtual Status _tick(Node *p_actor, const Ref<BTStore> &p_btstore) override;
};

class BTSelector : public BTComposite {
	GDCLASS(BTSelector, BTComposite)

protected:
	static void _bind_methods() {}

public:
	static Ref<BTSelector> create(std::initializer_list<Ref<BTTask>> p_children = {}) {
		Ref<BTSelector> node;
		node.instantiate();
		node->add_children(p_children);
		return node;
	}

	virtual void _enter(Node *p_actor, const Ref<BTStore> &p_btstore) override {
		current_child_index = 0;
	}

	virtual Status _tick(Node *p_actor, const Ref<BTStore> &p_btstore) override;
};

class BTRandomSelector : public BTComposite {
	GDCLASS(BTRandomSelector, BTComposite)

private:
	Vector<int> indices;

protected:
	static void _bind_methods() {}

public:
	static Ref<BTRandomSelector> create(std::initializer_list<Ref<BTTask>> p_children = {}) {
		Ref<BTRandomSelector> node;
		node.instantiate();
		node->add_children(p_children);
		return node;
	}

	virtual void _enter(Node *p_actor, const Ref<BTStore> &p_btstore) override;
	virtual Status _tick(Node *p_actor, const Ref<BTStore> &p_btstore) override;
};

class BTRandomSequence : public BTComposite {
	GDCLASS(BTRandomSequence, BTComposite)

private:
	Vector<int> indices;

protected:
	static void _bind_methods() {}

public:
	static Ref<BTRandomSequence> create(std::initializer_list<Ref<BTTask>> p_children = {}) {
		Ref<BTRandomSequence> node;
		node.instantiate();
		node->add_children(p_children);
		return node;
	}

	virtual void _enter(Node *p_actor, const Ref<BTStore> &p_btstore) override;
	virtual Status _tick(Node *p_actor, const Ref<BTStore> &p_btstore) override;
};

class BTReactiveSelector : public BTComposite {
	GDCLASS(BTReactiveSelector, BTComposite)

protected:
	static void _bind_methods() {}

public:
	static Ref<BTReactiveSelector> create(std::initializer_list<Ref<BTTask>> p_children = {}) {
		Ref<BTReactiveSelector> node;
		node.instantiate();
		node->add_children(p_children);
		return node;
	}

	virtual void _enter(Node *p_actor, const Ref<BTStore> &p_btstore) override {
		current_child_index = 0;
	}

	virtual Status _tick(Node *p_actor, const Ref<BTStore> &p_btstore) override;
};

class BTReactiveSequence : public BTComposite {
	GDCLASS(BTReactiveSequence, BTComposite)

protected:
	static void _bind_methods() {}

public:
	static Ref<BTReactiveSequence> create(std::initializer_list<Ref<BTTask>> p_children = {}) {
		Ref<BTReactiveSequence> node;
		node.instantiate();
		node->add_children(p_children);
		return node;
	}

	virtual void _enter(Node *p_actor, const Ref<BTStore> &p_btstore) override {
		current_child_index = 0;
	}

	virtual Status _tick(Node *p_actor, const Ref<BTStore> &p_btstore) override;
};

} // namespace godot

#endif // BT_COMPOSITES_H
