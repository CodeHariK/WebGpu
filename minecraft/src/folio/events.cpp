#include "events.h"

#include <godot_cpp/core/class_db.hpp>

namespace godot {

Events::Events() {}

Events::~Events() {}

void Events::on(
		const StringName &p_name,
		const Callable &p_callable,
		int p_order
) {
	if (!callbacks.has(p_name)) {
		callbacks.insert(p_name, Vector<Subscriber>());
	}

	Vector<Subscriber> &list = callbacks[p_name];

	Subscriber sub;
	sub.order = p_order;
	sub.callable = p_callable;

	// Insert keeping the list sorted ascending by order, stable within an order.
	int index = list.size();
	for (int i = 0; i < list.size(); i++) {
		if (list[i].order > p_order) {
			index = i;
			break;
		}
	}
	list.insert(index, sub);
}

void Events::off(
		const StringName &p_name,
		const Callable &p_callable
) {
	if (!callbacks.has(p_name)) {
		return;
	}

	// off(name) with an empty callable -> remove every listener for this event.
	if (!p_callable.is_valid()) {
		callbacks.erase(p_name);
		return;
	}

	// off(name, callable) -> remove matching listener(s) only.
	Vector<Subscriber> &list = callbacks[p_name];
	for (int i = 0; i < list.size(); i++) {
		if (list[i].callable == p_callable) {
			list.remove_at(i);
			i--;
		}
	}
	if (list.is_empty()) {
		callbacks.erase(p_name);
	}
}

void Events::trigger(
		const StringName &p_name,
		const Array &p_arguments
) {
	if (!callbacks.has(p_name)) {
		return;
	}

	// Snapshot so a listener may on()/off() during dispatch without breaking us.
	const Vector<Subscriber> &list = callbacks[p_name];
	Vector<Callable> snapshot;
	snapshot.resize(list.size());
	for (int i = 0; i < list.size(); i++) {
		snapshot.write[i] = list[i].callable;
	}

	for (int i = 0; i < snapshot.size(); i++) {
		const Callable &cb = snapshot[i];
		if (cb.is_valid()) {
			cb.callv(p_arguments);
		}
	}
}

bool Events::has(const StringName &p_name) const { return callbacks.has(p_name); }

void Events::clear() { callbacks.clear(); }

void Events::_bind_methods() {
	ClassDB::bind_method(D_METHOD("on", "name", "callable", "order"), &Events::on, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("off", "name", "callable"), &Events::off, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("trigger", "name", "arguments"), &Events::trigger, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("has", "name"), &Events::has);
	ClassDB::bind_method(D_METHOD("clear"), &Events::clear);
}

} // namespace godot
