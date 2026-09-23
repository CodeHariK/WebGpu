#include "time_warp.h"

#include "../../utils/spring/spring_dynamics.h"

#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void TimeWarp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_active", "active"), &TimeWarp::set_active);
	ClassDB::bind_method(D_METHOD("is_active"), &TimeWarp::is_active);
	ClassDB::bind_method(D_METHOD("toggle"), &TimeWarp::toggle);

	ClassDB::bind_method(D_METHOD("get_energy"), &TimeWarp::get_energy);
	ClassDB::bind_method(D_METHOD("set_energy", "e"), &TimeWarp::set_energy);
	ClassDB::bind_method(D_METHOD("set_warn", "warn"), &TimeWarp::set_warn);

	ClassDB::bind_method(D_METHOD("set_slow_scale", "v"), &TimeWarp::set_slow_scale);
	ClassDB::bind_method(D_METHOD("get_slow_scale"), &TimeWarp::get_slow_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slow_scale"), "set_slow_scale", "get_slow_scale");

	ClassDB::bind_method(D_METHOD("set_drain_rate", "v"), &TimeWarp::set_drain_rate);
	ClassDB::bind_method(D_METHOD("get_drain_rate"), &TimeWarp::get_drain_rate);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "drain_rate"), "set_drain_rate", "get_drain_rate");

	ClassDB::bind_method(D_METHOD("set_refill_rate", "v"), &TimeWarp::set_refill_rate);
	ClassDB::bind_method(D_METHOD("get_refill_rate"), &TimeWarp::get_refill_rate);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "refill_rate"), "set_refill_rate", "get_refill_rate");

	ClassDB::bind_method(D_METHOD("set_warn_threshold", "v"), &TimeWarp::set_warn_threshold);
	ClassDB::bind_method(D_METHOD("get_warn_threshold"), &TimeWarp::get_warn_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "warn_threshold"), "set_warn_threshold", "get_warn_threshold");
}

TimeWarp::TimeWarp() {}
TimeWarp::~TimeWarp() {}

void TimeWarp::_build_overlay() {
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/game/slowmo.gdshader");

	material.instantiate();
	if (shader.is_valid()) {
		material->set_shader(shader);
	}
	material->set_shader_parameter("amount", 0.0f);
	material->set_shader_parameter("warn", 0.0f);

	rect = memnew(ColorRect);
	rect->set_name("SlowMoRect");
	rect->set_anchors_preset(Control::PRESET_FULL_RECT);
	rect->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	rect->set_color(Color(0, 0, 0, 0));
	rect->set_material(material);
	rect->set_visible(false); // hidden until engaged (skips the backbuffer copy)
	add_child(rect);
}

void TimeWarp::_ready() {
	set_layer(layer_index);
	_build_overlay();
	last_usec = Time::get_singleton()->get_ticks_usec();

	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
	if (args.has("--fxtest")) {
		debug_keys = true;
	}
	if (args.has("--twshot")) {
		debug_force = true;
		Ref<SceneTreeTimer> t = get_tree()->create_timer(1.2);
		t->connect("timeout", callable_mp(this, &TimeWarp::_fx_capture));
	}
}

// Wall-clock delta so the meter and eases are independent of time_scale.
float TimeWarp::_real_delta() {
	uint64_t now = Time::get_singleton()->get_ticks_usec();
	float dt = (float)((double)(now - last_usec) / 1000000.0);
	last_usec = now;
	return CLAMP(dt, 0.0f, 0.1f); // guard against hitches / first frame
}

void TimeWarp::_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint() || material.is_null()) {
		return;
	}

	float rdt = _real_delta();

	// Energy: drain while active (real time), refill when off.
	if (active) {
		energy -= drain_rate * rdt;
		if (energy <= 0.0f) {
			energy = 0.0f;
			active = false; // out of juice -> disengage
		}
	} else {
		energy = MIN(1.0f, energy + refill_rate * rdt);
	}

	// Ease Engine.time_scale toward the target in wall time.
	float target_scale = active ? slow_scale : 1.0f;
	Engine *engine = Engine::get_singleton();
	float ts = (float)engine->get_time_scale();
	ts = Math::lerp(ts, target_scale, spring_damp_factor(engage_rate, rdt));
	engine->set_time_scale(ts);

	// Grade fades in with engagement.
	current_amount = Math::lerp(current_amount, active ? 1.0f : 0.0f, spring_damp_factor(grade_rate, rdt));

	// Warning: meter running low while active, or an external "low X" push.
	float meter_warn = (active && energy < warn_threshold && warn_threshold > 0.0f)
			? (1.0f - energy / warn_threshold)
			: 0.0f;
	float target_warn = MAX(meter_warn, external_warn);
	if (debug_force) {
		current_amount = MAX(current_amount, 1.0f);
		target_warn = MAX(target_warn, 0.75f);
	}
	current_warn = Math::lerp(current_warn, target_warn, spring_damp_factor(warn_rate, rdt));

	material->set_shader_parameter("amount", current_amount);
	material->set_shader_parameter("warn", current_warn);

	// Only draw (and pay for the backbuffer copy) when there is something to show.
	bool show = current_amount > 0.003f || current_warn > 0.003f;
	if (rect) {
		rect->set_visible(show);
	}
}

void TimeWarp::_exit_tree() {
	// Never leave the game running slow.
	if (!Engine::get_singleton()->is_editor_hint()) {
		Engine::get_singleton()->set_time_scale(1.0);
	}
}

void TimeWarp::_unhandled_key_input(const Ref<InputEvent> &p_event) {
	if (!debug_keys) {
		return;
	}
	Ref<InputEventKey> key = p_event;
	if (key.is_null() || !key->is_pressed() || key->is_echo()) {
		return;
	}
	switch (key->get_keycode()) {
		case KEY_T:
			toggle(); // debug: engage / release bullet-time
			break;
		case KEY_G:
			// debug: toggle the low-time flash directly, independent of the meter
			set_warn(external_warn > 0.5f ? 0.0f : 1.0f);
			break;
		case KEY_R:
			set_energy(1.0f); // debug: refill the meter
			break;
		default:
			break;
	}
}

void TimeWarp::set_active(bool p_active) {
	if (p_active && energy < min_energy_to_start) {
		return; // not enough meter to engage
	}
	active = p_active;
}

void TimeWarp::toggle() {
	set_active(!active);
}

void TimeWarp::set_energy(float p_e) {
	energy = CLAMP(p_e, 0.0f, 1.0f);
}

void TimeWarp::set_warn(float p_warn) {
	external_warn = CLAMP(p_warn, 0.0f, 1.0f);
}

void TimeWarp::_fx_capture() {
	Ref<Image> img = get_viewport()->get_texture()->get_image();
	if (img.is_valid()) {
		img->save_png("res://../docs/png/time_warp.png");
	}
	get_tree()->quit();
}

} // namespace godot
