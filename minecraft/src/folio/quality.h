#ifndef FOLIO_QUALITY_H
#define FOLIO_QUALITY_H

#include "events.h"

#include <godot_cpp/classes/node.hpp>

namespace godot {

/**
 * Folio port — Quality  (folio `Game/Quality.js`)
 * -----------------------------------------------
 * The global quality tier. `level` is 0 = highest, 1 = low; folio defaults it to
 * 1 on mobile and 0 elsewhere. Many systems branch on `quality.level` (bloom mip
 * count, cheap-DOF, pre-render, etc.) and subscribe to the `change` event to
 * rebuild when it flips.
 *
 * Web -> Godot: `navigator.userAgent` mobile sniff -> `OS::has_feature("mobile")`.
 *
 * Emits on the owned `Events` bus: `change` with args `[ level ]`.
 * (Name kept as `Quality` — no core Godot class collides.)
 */
class Quality : public Node {
	GDCLASS(Quality,
			Node)

private:
	int level = 0; // 0 = highest quality, 1 = low
	Ref<Events> events;

protected:
	static void _bind_methods();

public:
	Quality();
	~Quality();

	void _ready() override;

	// Change the quality tier; no-op if unchanged, else fires `change`.
	void change_level(int p_level);

	int get_level() const { return level; }

	Ref<Events> get_events() const { return events; }
};

} // namespace godot

#endif // FOLIO_QUALITY_H
