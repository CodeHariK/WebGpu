#include "bt_composites.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void BTComposite::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_child", "child"), &BTComposite::add_child);
	ClassDB::bind_method(D_METHOD("remove_child", "child"), &BTComposite::remove_child);
	ClassDB::bind_method(D_METHOD("clear_children"), &BTComposite::clear_children);
}

BTComposite::BTComposite() {}
BTComposite::~BTComposite() {}

void BTComposite::add_child(const Ref<BTTask> &p_child) {
	children.push_back(p_child);
}

void BTComposite::remove_child(const Ref<BTTask> &p_child) {
	children.erase(p_child);
}

void BTComposite::clear_children() {
	children.clear();
}

// BTSequence implementation
BTTask::Status BTSequence::_tick(Node *p_actor, const Ref<BTStore> &p_btstore) {
	for (; current_child_index < children.size(); current_child_index++) {
		Status status = children.write[current_child_index]->execute(p_actor, p_btstore);

		if (status == RUNNING) {
			return RUNNING;
		}

		if (status == FAILURE) {
			return FAILURE;
		}
	}

	return SUCCESS;
}

// BTSelector implementation
BTTask::Status BTSelector::_tick(Node *p_actor, const Ref<BTStore> &p_btstore) {
	for (; current_child_index < children.size(); current_child_index++) {
		Status status = children.write[current_child_index]->execute(p_actor, p_btstore);

		if (status == RUNNING) {
			return RUNNING;
		}

		if (status == SUCCESS) {
			return SUCCESS;
		}
	}

	return FAILURE;
}

// BTRandomSelector implementation
void BTRandomSelector::_enter(Node *p_actor, const Ref<BTStore> &p_btstore) {
	current_child_index = 0;
	if (indices.size() != children.size()) {
		indices.resize(children.size());
	}
	for (int i = 0; i < children.size(); i++) {
		indices.write[i] = i;
	}
	// Shuffle
	for (int i = indices.size() - 1; i > 0; i--) {
		int j = UtilityFunctions::randi() % (i + 1);
		int temp = indices[i];
		indices.write[i] = indices[j];
		indices.write[j] = temp;
	}
}

BTTask::Status BTRandomSelector::_tick(Node *p_actor, const Ref<BTStore> &p_btstore) {
	for (; current_child_index < indices.size(); current_child_index++) {
		Status status = children.write[indices[current_child_index]]->execute(p_actor, p_btstore);

		if (status == RUNNING) {
			return RUNNING;
		}

		if (status == SUCCESS) {
			return SUCCESS;
		}
	}

	return FAILURE;
}

// BTRandomSequence implementation
void BTRandomSequence::_enter(Node *p_actor, const Ref<BTStore> &p_btstore) {
	current_child_index = 0;
	if (indices.size() != children.size()) {
		indices.resize(children.size());
	}
	for (int i = 0; i < children.size(); i++) {
		indices.write[i] = i;
	}
	// Shuffle
	for (int i = indices.size() - 1; i > 0; i--) {
		int j = UtilityFunctions::randi() % (i + 1);
		int temp = indices[i];
		indices.write[i] = indices[j];
		indices.write[j] = temp;
	}
}

BTTask::Status BTRandomSequence::_tick(Node *p_actor, const Ref<BTStore> &p_btstore) {
	for (; current_child_index < indices.size(); current_child_index++) {
		Status status = children.write[indices[current_child_index]]->execute(p_actor, p_btstore);

		if (status == RUNNING) {
			return RUNNING;
		}

		if (status == FAILURE) {
			return FAILURE;
		}
	}

	return SUCCESS;
}

// BTReactiveSelector implementation
BTTask::Status BTReactiveSelector::_tick(Node *p_actor, const Ref<BTStore> &p_btstore) {
	for (int i = 0; i < children.size(); i++) {
		Status status = children.write[i]->execute(p_actor, p_btstore);

		if (status == RUNNING) {
			if (current_child_index != i) {
				if (current_child_index < children.size() && children.write[current_child_index]->get_status() == RUNNING) {
					children.write[current_child_index]->abort(p_actor, p_btstore);
				}
				current_child_index = i;
			}
			return RUNNING;
		}

		if (status == SUCCESS) {
			if (current_child_index != i) {
				if (current_child_index < children.size() && children.write[current_child_index]->get_status() == RUNNING) {
					children.write[current_child_index]->abort(p_actor, p_btstore);
				}
				current_child_index = i;
			}
			return SUCCESS;
		}
	}

	return FAILURE;
}

// BTReactiveSequence implementation
BTTask::Status BTReactiveSequence::_tick(Node *p_actor, const Ref<BTStore> &p_btstore) {
	for (int i = 0; i < children.size(); i++) {
		Status status = children.write[i]->execute(p_actor, p_btstore);

		if (status == RUNNING) {
			if (current_child_index != i) {
				if (current_child_index < children.size() && children.write[current_child_index]->get_status() == RUNNING) {
					children.write[current_child_index]->abort(p_actor, p_btstore);
				}
				current_child_index = i;
			}
			return RUNNING;
		}

		if (status == FAILURE) {
			if (current_child_index != i) {
				if (current_child_index < children.size() && children.write[current_child_index]->get_status() == RUNNING) {
					children.write[current_child_index]->abort(p_actor, p_btstore);
				}
				current_child_index = i;
			}
			return FAILURE;
		}
	}

	return SUCCESS;
}

} // namespace godot
