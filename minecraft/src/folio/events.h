#ifndef FOLIO_EVENTS_H
#define FOLIO_EVENTS_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string_name.hpp>

namespace godot {

/**
 * Folio port — FolioEvents
 * -------------------
 * Faithful port of folio-2025 `Game/Events.js`: a tiny named pub/sub bus.
 *
 * Every folio "system" owns an `FolioEvents` instance and other systems subscribe to
 * it by name — `stop`/`start`, `change`, `enter`/`leave`, `muteChange`, etc. It
 * is the generic message bus that sits underneath (and is separate from) the
 * per-frame `FolioTicker` tick.
 *
 * Semantics (identical to the JS original):
 *   - `on(name, callable, order)` registers a listener. Listeners fire in
 *     ASCENDING integer `order` (default 1), stable within an order.
 *   - `trigger(name, args)` calls every listener with `args` applied
 *     (JS `fn.apply(this, args)` -> Godot `callable.callv(args)`).
 *   - `off(name, callable)` removes one listener; `off(name)` removes them all.
 *
 * Why RefCounted (not a Node): folio's systems are plain objects, not scene
 * nodes, and each simply holds an `FolioEvents`. A RefCounted mirrors that — create
 * with `FolioEvents.new()`, store it, no scene tree involvement. (This is the same
 * ordered-dispatch idea the `FolioTicker` uses internally; `FolioTicker` keeps its own
 * copy to stay dependency-free, but any other system uses this class.)
 *
 * Dispatch safety: `trigger` iterates over a snapshot, so a listener may
 * `on`/`off` during the callback without invalidating the running dispatch.
 */
class FolioEvents : public RefCounted {
	GDCLASS(FolioEvents,
			RefCounted)

private:
	struct Subscriber {
		int order = 1;
		Callable callable;
	};

	// name -> listeners kept sorted ascending by order.
	HashMap<StringName, Vector<Subscriber>> callbacks;

protected:
	static void _bind_methods();

public:
	FolioEvents();
	~FolioEvents();

	void on(const StringName &p_name,
			const Callable &p_callable,
			int p_order);
	void
	off(const StringName &p_name,
		const Callable &p_callable);
	void
	trigger(const StringName &p_name,
			const Array &p_arguments);

	bool has(const StringName &p_name) const;
	void clear();
};

} // namespace godot

#endif // FOLIO_EVENTS_H
