#include "vehicle_audio.h"

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

static double remap_clamp(double v, double a, double b, double c, double d) {
	double t = (b - a);
	double r = (t == 0.0) ? c : (c + (d - c) * (v - a) / t);
	return CLAMP(r, MIN(c, d), MAX(c, d));
}

static float lin2db(double v) {
	if (v <= 0.0001) {
		return -80.0f;
	}
	return (float)(Math::linear_to_db(v));
}

FolioVehicleAudio::FolioVehicleAudio() {}
FolioVehicleAudio::~FolioVehicleAudio() {}

AudioStreamPlayer3D *FolioVehicleAudio::_make_loop(const String &p_path) {
	AudioStreamPlayer3D *p = memnew(AudioStreamPlayer3D);
	Ref<AudioStream> stream = ResourceLoader::get_singleton()->load(p_path);
	if (stream.is_valid()) {
		if (stream->has_method("set_loop")) {
			stream->call("set_loop", true);
		}
		p->set_stream(stream);
	} else {
		UtilityFunctions::printerr("FolioVehicleAudio > load error > ", p_path);
	}
	p->set_unit_size((float)carry_distance);
	p->set_max_db(3.0f);
	p->set_volume_db(-80.0f); // start silent; driven up each frame
	add_child(p);
	p->play();
	return p;
}

void FolioVehicleAudio::_apply_carry() {
	if (engine) {
		engine->set_unit_size((float)carry_distance);
	}
	if (spin) {
		spin->set_unit_size((float)carry_distance);
	}
	if (boost) {
		boost->set_unit_size((float)carry_distance);
	}
}

void FolioVehicleAudio::set_carry_distance(double v) {
	carry_distance = v;
	_apply_carry();
}

void FolioVehicleAudio::_ready() {
	car = Object::cast_to<RigidBody3D>(get_parent());
	engine = _make_loop("res://assets/folio/audio/vehicle/engine.mp3");
	spin = _make_loop("res://assets/folio/audio/vehicle/spin.mp3");
	boost = _make_loop("res://assets/folio/audio/vehicle/boost.mp3");

	horn = memnew(AudioStreamPlayer3D);
	Ref<AudioStream> hs = ResourceLoader::get_singleton()->load("res://assets/folio/audio/vehicle/horn.mp3");
	if (hs.is_valid()) {
		horn->set_stream(hs);
	}
	horn->set_unit_size(16.0f);
	add_child(horn);
}

void FolioVehicleAudio::_physics_process(double p_delta) {
	if (!engine) {
		return;
	}
	Input *in = Input::get_singleton();

	double throttle = 0.0;
	if (in->is_action_pressed("move_forward")) {
		throttle = 1.0;
	} else if (in->is_action_pressed("move_backward")) {
		throttle = 0.6;
	}
	const double accelerating = throttle;
	const double boosting = in->is_physical_key_pressed(KEY_SHIFT) ? 1.0 : 0.0;

	double xz_speed = 0.0;
	if (car) {
		Vector3 v = car->get_linear_velocity();
		xz_speed = Vector2(v.x, v.z).length();
	}

	// --- Engine ---
	{
		double target_vol = MAX(0.05, accelerating * (boosting + 1.0) * 0.8);
		double ease = (target_vol > engine_vol) ? attack_ease : release_ease;
		engine_vol += (target_vol - engine_vol) * p_delta * ease;
		double target_rate = remap_clamp(accelerating * (boosting + 1.0), 0.0, 1.0, engine_pitch_min, engine_pitch_max);
		engine_rate += (target_rate - engine_rate) * p_delta * pitch_ease;
		engine->set_volume_db(lin2db(engine_vol * engine_gain));
		engine->set_pitch_scale((float)CLAMP(engine_rate, 0.25, 4.0));
	}

	// --- Spin / rolling ---
	{
		double speed_effect = CLAMP(xz_speed * spin_speed_sensitivity, 0.0, 1.0);
		double target_vol = speed_effect * spin_gain;
		double ease = (target_vol > spin_vol) ? attack_ease : release_ease;
		spin_vol += (target_vol - spin_vol) * p_delta * ease;
		double target_rate = remap_clamp(speed_effect, 0.0, 1.0, 1.0, spin_pitch_max);
		spin_rate += (target_rate - spin_rate) * p_delta * pitch_ease;
		spin->set_volume_db(lin2db(spin_vol));
		spin->set_pitch_scale((float)CLAMP(spin_rate, 0.25, 4.0));
	}

	// --- Boost ---
	{
		double target_vol = (0.5 + 0.5 * accelerating) * boosting * boost_gain;
		double ease = (target_vol > boost_vol) ? attack_ease : 1.0;
		boost_vol += (target_vol - boost_vol) * p_delta * ease;
		double target_rate = 0.95 + accelerating * 2.0;
		boost_rate += (target_rate - boost_rate) * p_delta * pitch_ease;
		boost->set_volume_db(lin2db(boost_vol));
		boost->set_pitch_scale((float)CLAMP(boost_rate, 0.25, 4.0));
	}
}

void FolioVehicleAudio::_unhandled_key_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && !k->is_echo() && k->get_physical_keycode() == KEY_H) {
		if (horn && !horn->is_playing()) {
			horn->play();
		}
	}
}

void FolioVehicleAudio::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_engine_gain", "v"), &FolioVehicleAudio::set_engine_gain);
	ClassDB::bind_method(D_METHOD("get_engine_gain"), &FolioVehicleAudio::get_engine_gain);
	ClassDB::bind_method(D_METHOD("set_engine_pitch_min", "v"), &FolioVehicleAudio::set_engine_pitch_min);
	ClassDB::bind_method(D_METHOD("get_engine_pitch_min"), &FolioVehicleAudio::get_engine_pitch_min);
	ClassDB::bind_method(D_METHOD("set_engine_pitch_max", "v"), &FolioVehicleAudio::set_engine_pitch_max);
	ClassDB::bind_method(D_METHOD("get_engine_pitch_max"), &FolioVehicleAudio::get_engine_pitch_max);
	ClassDB::bind_method(D_METHOD("set_spin_speed_sensitivity", "v"), &FolioVehicleAudio::set_spin_speed_sensitivity);
	ClassDB::bind_method(D_METHOD("get_spin_speed_sensitivity"), &FolioVehicleAudio::get_spin_speed_sensitivity);
	ClassDB::bind_method(D_METHOD("set_spin_gain", "v"), &FolioVehicleAudio::set_spin_gain);
	ClassDB::bind_method(D_METHOD("get_spin_gain"), &FolioVehicleAudio::get_spin_gain);
	ClassDB::bind_method(D_METHOD("set_spin_pitch_max", "v"), &FolioVehicleAudio::set_spin_pitch_max);
	ClassDB::bind_method(D_METHOD("get_spin_pitch_max"), &FolioVehicleAudio::get_spin_pitch_max);
	ClassDB::bind_method(D_METHOD("set_boost_gain", "v"), &FolioVehicleAudio::set_boost_gain);
	ClassDB::bind_method(D_METHOD("get_boost_gain"), &FolioVehicleAudio::get_boost_gain);
	ClassDB::bind_method(D_METHOD("set_carry_distance", "v"), &FolioVehicleAudio::set_carry_distance);
	ClassDB::bind_method(D_METHOD("get_carry_distance"), &FolioVehicleAudio::get_carry_distance);
	ClassDB::bind_method(D_METHOD("set_attack_ease", "v"), &FolioVehicleAudio::set_attack_ease);
	ClassDB::bind_method(D_METHOD("get_attack_ease"), &FolioVehicleAudio::get_attack_ease);
	ClassDB::bind_method(D_METHOD("set_release_ease", "v"), &FolioVehicleAudio::set_release_ease);
	ClassDB::bind_method(D_METHOD("get_release_ease"), &FolioVehicleAudio::get_release_ease);
	ClassDB::bind_method(D_METHOD("set_pitch_ease", "v"), &FolioVehicleAudio::set_pitch_ease);
	ClassDB::bind_method(D_METHOD("get_pitch_ease"), &FolioVehicleAudio::get_pitch_ease);

	ADD_GROUP("Engine", "engine_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "engine_gain", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_engine_gain", "get_engine_gain");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "engine_pitch_min", PROPERTY_HINT_RANGE, "0.25,2,0.01"), "set_engine_pitch_min", "get_engine_pitch_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "engine_pitch_max", PROPERTY_HINT_RANGE, "0.25,4,0.01"), "set_engine_pitch_max", "get_engine_pitch_max");
	ADD_GROUP("Spin", "spin_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spin_speed_sensitivity", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_spin_speed_sensitivity", "get_spin_speed_sensitivity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spin_gain", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_spin_gain", "get_spin_gain");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spin_pitch_max", PROPERTY_HINT_RANGE, "1,4,0.01"), "set_spin_pitch_max", "get_spin_pitch_max");
	ADD_GROUP("Boost", "boost_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "boost_gain", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_boost_gain", "get_boost_gain");
	ADD_GROUP("Mix", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "carry_distance", PROPERTY_HINT_RANGE, "1,60,0.5"), "set_carry_distance", "get_carry_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attack_ease", PROPERTY_HINT_RANGE, "0.5,30,0.1"), "set_attack_ease", "get_attack_ease");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "release_ease", PROPERTY_HINT_RANGE, "0.5,30,0.1"), "set_release_ease", "get_release_ease");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch_ease", PROPERTY_HINT_RANGE, "0.5,30,0.1"), "set_pitch_ease", "get_pitch_ease");
}
