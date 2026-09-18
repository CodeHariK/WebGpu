#include "ticker.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// Global shader uniform names (mirror folio's elapsed/delta TSL uniforms).
static const char *SG_ELAPSED = "folio_elapsed";
static const char *SG_DELTA = "folio_delta";
static const char *SG_ELAPSED_SCALED = "folio_elapsed_scaled";
static const char *SG_DELTA_SCALED = "folio_delta_scaled";

FolioTicker *FolioTicker::singleton = nullptr;

FolioTicker::FolioTicker() { last_deltas.reserve(delta_average_count); }

FolioTicker::~FolioTicker() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void FolioTicker::_ready() {
	singleton = this;
	_register_shader_globals();
	set_process(true);
}

void FolioTicker::_process(double p_delta) { update(p_delta); }

void FolioTicker::_register_shader_globals() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}

	// Add each global only if it does not already exist (idempotent).
	const char *names[] = { SG_ELAPSED, SG_DELTA, SG_ELAPSED_SCALED, SG_DELTA_SCALED };
	for (const char *name : names) {
		if (rs->global_shader_parameter_get(name).get_type() == Variant::NIL) {
			rs->global_shader_parameter_add(name, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.0f);
		}
	}
	globals_registered = true;
}

void FolioTicker::_update_shader_globals() {
	if (!globals_registered) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	rs->global_shader_parameter_set(SG_ELAPSED, (float)elapsed);
	rs->global_shader_parameter_set(SG_DELTA, (float)delta);
	rs->global_shader_parameter_set(SG_ELAPSED_SCALED, (float)elapsed_scaled);
	rs->global_shader_parameter_set(SG_DELTA_SCALED, (float)delta_scaled);
}

void FolioTicker::update(double p_frame_delta) {
	// Clamp the step to avoid a spiral of death after a stall (folio: maxDelta).
	delta = MIN(p_frame_delta, max_delta);
	elapsed += p_frame_delta; // elapsed tracks real wall time, like folio
	delta_scaled = delta * scale;
	elapsed_scaled += delta_scaled;

	// Rolling average of the last N deltas (newest first).
	last_deltas.insert(last_deltas.begin(), delta);
	if ((int)last_deltas.size() > delta_average_count) {
		last_deltas.resize(delta_average_count);
	}
	double sum = 0.0;
	for (double d : last_deltas) {
		sum += d;
	}
	delta_average = sum / (double)last_deltas.size();

	_update_shader_globals();

	// Frame-countdown scheduler: decrement, fire, and drop callbacks reaching 0.
	for (int i = 0; i < (int)waits.size(); i++) {
		waits[i].frames--;
		if (waits[i].frames <= 0) {
			Callable cb = waits[i].callable;
			waits.erase(waits.begin() + i);
			i--;
			if (cb.is_valid()) {
				cb.call();
			}
		}
	}

	// Dispatch the tick in ascending order. Iterate over a snapshot so that a
	// callback which subscribes/unsubscribes during the tick cannot invalidate us.
	std::vector<Callable> snapshot;
	snapshot.reserve(tick_subscribers.size());
	for (const TickSubscriber &sub : tick_subscribers) {
		snapshot.push_back(sub.callable);
	}
	for (const Callable &cb : snapshot) {
		if (cb.is_valid()) {
			cb.call();
		}
	}
}

void FolioTicker::connect_tick(
		const Callable &p_callable,
		int p_order
) {
	// Insert keeping the vector sorted ascending by order, stable within an order.
	TickSubscriber sub;
	sub.order = p_order;
	sub.callable = p_callable;

	int index = (int)tick_subscribers.size();
	for (int i = 0; i < (int)tick_subscribers.size(); i++) {
		if (tick_subscribers[i].order > p_order) {
			index = i;
			break;
		}
	}
	tick_subscribers.insert(tick_subscribers.begin() + index, sub);
}

void FolioTicker::disconnect_tick(const Callable &p_callable) {
	for (int i = 0; i < (int)tick_subscribers.size(); i++) {
		if (tick_subscribers[i].callable == p_callable) {
			tick_subscribers.erase(tick_subscribers.begin() + i);
			i--;
		}
	}
}

void FolioTicker::wait(
		int p_frames,
		const Callable &p_callable
) {
	Wait w;
	w.frames = p_frames;
	w.callable = p_callable;
	waits.push_back(w);
}

void FolioTicker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update", "frame_delta"), &FolioTicker::update);
	ClassDB::bind_method(
			D_METHOD("connect_tick", "callable", "order"), &FolioTicker::connect_tick, DEFVAL(PRIORITY_DEFAULT)
	);
	ClassDB::bind_method(D_METHOD("disconnect_tick", "callable"), &FolioTicker::disconnect_tick);
	ClassDB::bind_method(D_METHOD("wait", "frames", "callable"), &FolioTicker::wait);

	ClassDB::bind_method(D_METHOD("get_elapsed"), &FolioTicker::get_elapsed);
	ClassDB::bind_method(D_METHOD("get_delta"), &FolioTicker::get_delta);
	ClassDB::bind_method(D_METHOD("get_delta_scaled"), &FolioTicker::get_delta_scaled);
	ClassDB::bind_method(D_METHOD("get_elapsed_scaled"), &FolioTicker::get_elapsed_scaled);
	ClassDB::bind_method(D_METHOD("get_delta_average"), &FolioTicker::get_delta_average);

	ClassDB::bind_method(D_METHOD("set_scale", "scale"), &FolioTicker::set_scale);
	ClassDB::bind_method(D_METHOD("get_scale"), &FolioTicker::get_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale"), "set_scale", "get_scale");

	BIND_ENUM_CONSTANT(PRIORITY_DEFAULT);
	BIND_ENUM_CONSTANT(PRIORITY_PHYSICS_PRE);
	BIND_ENUM_CONSTANT(PRIORITY_PHYSICS_POST);
	BIND_ENUM_CONSTANT(PRIORITY_RENDER);
}

} // namespace godot
