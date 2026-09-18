#include "time.h"

#include "ticker.h"

#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

FolioTime::FolioTime() {}

FolioTime::~FolioTime() {}

void FolioTime::_ready() {
	_scale = default_scale;

	// Apply the starting scale immediately (folio sets ticker.scale in ctor).
	FolioTicker *ticker = FolioTicker::get_singleton();
	if (ticker) {
		ticker->set_scale(_scale);
	}

	// Subscribe to the tick at priority 0 (before physics). If the FolioTicker is not
	// ready yet (sibling order), retry once next frame.
	if (!_subscribe()) {
		call_deferred("_subscribe");
	}
}

bool FolioTime::_subscribe() {
	if (subscribed) {
		return true;
	}
	FolioTicker *ticker = FolioTicker::get_singleton();
	if (!ticker) {
		return false;
	}
	ticker->connect_tick(callable_mp(this, &FolioTime::update), 0);
	subscribed = true;
	return true;
}

void FolioTime::update() {
	FolioTicker *ticker = FolioTicker::get_singleton();
	if (!ticker) {
		return;
	}

	const uint64_t now = Time::get_singleton()->get_ticks_msec();
	if (now > bt_end_time_ms) {
		bt_active = false;
	}

	// Ramp progress toward 1 while active, back toward 0 otherwise.
	const double speed = bt_active ? bt_in_speed : bt_out_speed;
	bt_progress += (bt_active ? 1.0 : -1.0) * ticker->get_delta() * speed;
	bt_progress = CLAMP(bt_progress, 0.0, 1.0);

	// Remap progress 0..1 -> default_scale..bullet_scale.
	set_scale(default_scale + bt_progress * (bt_scale - default_scale));
}

void FolioTime::activate_bullet_time(double p_duration) {
	const uint64_t now = Time::get_singleton()->get_ticks_msec();
	const uint64_t new_end = now + (uint64_t)(p_duration * 1000.0);

	// Extend the window if already active, otherwise start fresh.
	bt_end_time_ms = bt_active ? MAX(bt_end_time_ms, new_end) : new_end;
	bt_active = true;
}

void FolioTime::set_scale(double p_scale) {
	_scale = p_scale;
	FolioTicker *ticker = FolioTicker::get_singleton();
	if (ticker) {
		ticker->set_scale(_scale);
	}
	// NOTE: folio also does gsap.globalTimeline.timeScale(value) here — no Godot
	// equivalent yet; wire a tween layer's scale in when that is ported.
}

void FolioTime::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioTime::update);
	ClassDB::bind_method(D_METHOD("_subscribe"), &FolioTime::_subscribe);
	ClassDB::bind_method(D_METHOD("activate_bullet_time", "duration"), &FolioTime::activate_bullet_time, DEFVAL(1.5));

	ClassDB::bind_method(D_METHOD("set_scale", "scale"), &FolioTime::set_scale);
	ClassDB::bind_method(D_METHOD("get_scale"), &FolioTime::get_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale"), "set_scale", "get_scale");

	ClassDB::bind_method(D_METHOD("set_default_scale", "scale"), &FolioTime::set_default_scale);
	ClassDB::bind_method(D_METHOD("get_default_scale"), &FolioTime::get_default_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_scale"), "set_default_scale", "get_default_scale");

	ClassDB::bind_method(D_METHOD("is_bullet_time_active"), &FolioTime::is_bullet_time_active);
	ClassDB::bind_method(D_METHOD("get_bullet_progress"), &FolioTime::get_bullet_progress);
}

} // namespace godot
