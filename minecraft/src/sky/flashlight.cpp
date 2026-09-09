/**
 * @file flashlight.cpp
 * @brief Flashlight: torch defaults, toggle input, flicker / battery.
 */
#include "flashlight.h"
#include "sky_cycle.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void Flashlight::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "on"), &Flashlight::set_enabled);
	ClassDB::bind_method(D_METHOD("get_enabled"), &Flashlight::get_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "get_enabled");
	ClassDB::bind_method(D_METHOD("set_auto_night", "v"), &Flashlight::set_auto_night);
	ClassDB::bind_method(D_METHOD("get_auto_night"), &Flashlight::get_auto_night);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_night"), "set_auto_night", "get_auto_night");
	ClassDB::bind_method(D_METHOD("toggle"), &Flashlight::toggle);
	ClassDB::bind_method(D_METHOD("set_toggle_key", "key"), &Flashlight::set_toggle_key);
	ClassDB::bind_method(D_METHOD("get_toggle_key"), &Flashlight::get_toggle_key);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "toggle_key", PROPERTY_HINT_NONE, ""), "set_toggle_key", "get_toggle_key");
	ClassDB::bind_method(D_METHOD("set_flicker", "v"), &Flashlight::set_flicker);
	ClassDB::bind_method(D_METHOD("get_flicker"), &Flashlight::get_flicker);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "flicker", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_flicker", "get_flicker"
	);
	ClassDB::bind_method(D_METHOD("set_flicker_speed", "v"), &Flashlight::set_flicker_speed);
	ClassDB::bind_method(D_METHOD("get_flicker_speed"), &Flashlight::get_flicker_speed);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "flicker_speed", PROPERTY_HINT_RANGE, "0,60,0.5"), "set_flicker_speed",
			"get_flicker_speed"
	);
	ClassDB::bind_method(D_METHOD("set_use_battery", "v"), &Flashlight::set_use_battery);
	ClassDB::bind_method(D_METHOD("get_use_battery"), &Flashlight::get_use_battery);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_battery"), "set_use_battery", "get_use_battery");
	ClassDB::bind_method(D_METHOD("set_battery", "v"), &Flashlight::set_battery);
	ClassDB::bind_method(D_METHOD("get_battery"), &Flashlight::get_battery);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "battery", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_battery", "get_battery"
	);
	ClassDB::bind_method(D_METHOD("set_battery_drain", "v"), &Flashlight::set_battery_drain);
	ClassDB::bind_method(D_METHOD("get_battery_drain"), &Flashlight::get_battery_drain);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "battery_drain", PROPERTY_HINT_RANGE, "0,1,0.001,suffix:/s"),
			"set_battery_drain", "get_battery_drain"
	);
}

Flashlight::Flashlight() {
	// Torch defaults (overridable in the inspector like any SpotLight3D).
	set_param(Light3D::PARAM_RANGE, 45.0f);
	set_param(Light3D::PARAM_SPOT_ANGLE, 30.0f);
	set_param(Light3D::PARAM_SPOT_ATTENUATION, 0.6f);
	set_param(Light3D::PARAM_ENERGY, 4.0f);
	set_color(Color(1.0f, 0.93f, 0.78f));
	set_shadow(true);
}
Flashlight::~Flashlight() {}

void Flashlight::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_base_energy = (float)get_param(Light3D::PARAM_ENERGY);
		set_visible(enabled);
		set_process(true);
		set_process_unhandled_input(true);
	}
}

void Flashlight::set_enabled(bool p_on) {
	enabled = p_on;
	set_visible(enabled);
}

void Flashlight::_unhandled_input(const Ref<InputEvent> &p_event) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (auto_night) {
		return; // Auto lights ignore the manual toggle key
	}
	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && !k->is_echo() && (int)k->get_keycode() == toggle_key) {
		toggle();
	}
}

void Flashlight::_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (auto_night) { // Headlights / lamps: track the day-night cycle
		if (!_sky) {
			_sky = SkyCycle::find(this);
		}
		const bool want = _sky ? _sky->is_night() : true;
		if (want != enabled) {
			set_enabled(want);
		}
	}
	if (!enabled) {
		return;
	}
	if (use_battery) {
		battery = MAX(0.0f, battery - battery_drain * (float)p_delta);
		if (battery <= 0.0f) {
			set_enabled(false);
			return;
		}
	}
	// Flicker worsens as the battery drains; two out-of-phase sines + a rare stutter.
	float f = flicker;
	if (use_battery) {
		f = CLAMP(flicker + (1.0f - battery) * 0.6f, 0.0f, 1.0f);
	}
	float mult = 1.0f;
	if (f > 0.001f) {
		_phase += (float)p_delta * flicker_speed;
		const float wobble = 0.5f * Math::sin(_phase) + 0.5f * Math::sin(_phase * 2.37f + 1.3f);
		const float stutter = Math::sin(_phase * 0.7f) > 0.96f ? -0.6f : 0.0f;
		mult = CLAMP(1.0f + f * (wobble * 0.3f + stutter), 0.15f, 1.15f);
	}
	set_param(Light3D::PARAM_ENERGY, _base_energy * mult * (use_battery ? MAX(0.25f, battery) : 1.0f));
}

} // namespace godot
