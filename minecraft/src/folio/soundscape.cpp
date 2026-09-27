#include "soundscape.h"

#include "game.h"
#include "weather.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

static float lin2db(double v) {
	if (v <= 0.0001) {
		return -80.0f;
	}
	return (float)(Math::linear_to_db(v));
}

static Ref<AudioStream> load_stream(const String &p_path) {
	Ref<AudioStream> s = ResourceLoader::get_singleton()->load(p_path);
	if (s.is_null()) {
		UtilityFunctions::printerr("FolioSoundscape > load error > ", p_path);
	}
	return s;
}

FolioSoundscape::FolioSoundscape() {}
FolioSoundscape::~FolioSoundscape() {}

void FolioSoundscape::_ready() {
	add_to_group("folio_soundscape");

	// --- Music playlist (folio: 3 songs, switch on end) ---
	songs.push_back(load_stream("res://assets/folio/audio/music/sudo.mp3"));
	songs.push_back(load_stream("res://assets/folio/audio/music/boy.mp3"));
	songs.push_back(load_stream("res://assets/folio/audio/music/baguira.mp3"));
	music = memnew(AudioStreamPlayer);
	music->set_name("Music");
	music->set_volume_db(lin2db(music_volume));
	add_child(music);
	int64_t block = Time::get_singleton()->get_unix_time_from_system() / 180;
	song_index = (int)(block % (int64_t)songs.size());
	if (songs[song_index].is_valid()) {
		music->set_stream(songs[song_index]);
		music->play();
	}
	music->connect("finished", callable_mp(this, &FolioSoundscape::_on_music_finished));

	// --- Wind ambience (folio: forest loop, volume from weather wind) ---
	wind = memnew(AudioStreamPlayer);
	wind->set_name("Wind");
	Ref<AudioStream> ws = load_stream("res://assets/folio/audio/ambient/wind.mp3");
	if (ws.is_valid()) {
		if (ws->has_method("set_loop")) {
			ws->call("set_loop", true);
		}
		wind->set_stream(ws);
	}
	wind->set_volume_db(-80.0f);
	add_child(wind);
	wind->play();

	// --- Impact banks ---
	hits_explosion.push_back(load_stream("res://assets/folio/audio/hits/explosion_1.mp3"));
	hits_explosion.push_back(load_stream("res://assets/folio/audio/hits/explosion_2.mp3"));
	for (int i = 1; i <= 4; i++) {
		hits_impact.push_back(load_stream("res://assets/folio/audio/hits/impact_" + String::num_int64(i) + ".mp3"));
	}
}

void FolioSoundscape::_on_music_finished() {
	if (songs.is_empty()) {
		return;
	}
	song_index = (song_index + 1) % songs.size();
	if (songs[song_index].is_valid()) {
		music->set_stream(songs[song_index]);
		music->play();
	}
}

void FolioSoundscape::_process(double p_delta) {
	if (!wind) {
		return;
	}
	// folio: volume = pow(remapClamp(wind, 0.3, 1, 0, 1), 3) * wind_max_volume
	// Read the live wind from FolioWeather (global_shader_parameter_get is
	// editor-only and errors + hurts perf at runtime).
	double w = 0.5;
	FolioGame *g = FolioGame::get_singleton();
	if (g && g->get_weather()) {
		w = g->get_weather()->get_wind();
	}
	double t = CLAMP((w - 0.3) / 0.7, 0.0, 1.0);
	double target = t * t * t * wind_max_volume;
	double ease = (target > wind_vol) ? 2.0 : 1.0;
	wind_vol += (target - wind_vol) * p_delta * ease;
	wind->set_volume_db(lin2db(wind_vol));
}

Ref<AudioStream> FolioSoundscape::_pick(const Vector<Ref<AudioStream>> &p_bank) const {
	if (p_bank.is_empty()) {
		return Ref<AudioStream>();
	}
	int i = (int)(UtilityFunctions::randi() % p_bank.size());
	return p_bank[i];
}

AudioStreamPlayer3D *FolioSoundscape::_free_hit_player() {
	for (int i = 0; i < hit_pool.size(); i++) {
		if (hit_pool[i] && !hit_pool[i]->is_playing()) {
			return hit_pool[i];
		}
	}
	if (hit_pool.size() >= 24) {
		return hit_pool[0];
	}
	AudioStreamPlayer3D *p = memnew(AudioStreamPlayer3D);
	p->set_unit_size((float)hit_carry_distance);
	add_child(p);
	hit_pool.push_back(p);
	return p;
}

void FolioSoundscape::play_hit(const String &p_group, const Vector3 &p_position, double p_rate) {
	Ref<AudioStream> s;
	double gain = impact_gain;
	if (p_group == "explosion") {
		s = _pick(hits_explosion);
		gain = explosion_gain;
	} else {
		s = _pick(hits_impact);
	}
	if (s.is_null()) {
		return;
	}
	AudioStreamPlayer3D *p = _free_hit_player();
	if (!p) {
		return;
	}
	p->set_unit_size((float)hit_carry_distance);
	p->set_stream(s);
	p->set_global_position(p_position);
	p->set_pitch_scale((float)CLAMP(p_rate, 0.5, 2.0));
	p->set_volume_db(lin2db(gain));
	p->play();
}

void FolioSoundscape::set_music_volume(double v) {
	music_volume = v;
	if (music) {
		music->set_volume_db(lin2db(music_volume));
	}
}

void FolioSoundscape::_bind_methods() {
	ClassDB::bind_method(D_METHOD("play_hit", "group", "position", "rate"), &FolioSoundscape::play_hit);

	ClassDB::bind_method(D_METHOD("set_music_volume", "v"), &FolioSoundscape::set_music_volume);
	ClassDB::bind_method(D_METHOD("get_music_volume"), &FolioSoundscape::get_music_volume);
	ClassDB::bind_method(D_METHOD("set_wind_max_volume", "v"), &FolioSoundscape::set_wind_max_volume);
	ClassDB::bind_method(D_METHOD("get_wind_max_volume"), &FolioSoundscape::get_wind_max_volume);
	ClassDB::bind_method(D_METHOD("set_explosion_gain", "v"), &FolioSoundscape::set_explosion_gain);
	ClassDB::bind_method(D_METHOD("get_explosion_gain"), &FolioSoundscape::get_explosion_gain);
	ClassDB::bind_method(D_METHOD("set_impact_gain", "v"), &FolioSoundscape::set_impact_gain);
	ClassDB::bind_method(D_METHOD("get_impact_gain"), &FolioSoundscape::get_impact_gain);
	ClassDB::bind_method(D_METHOD("set_hit_carry_distance", "v"), &FolioSoundscape::set_hit_carry_distance);
	ClassDB::bind_method(D_METHOD("get_hit_carry_distance"), &FolioSoundscape::get_hit_carry_distance);

	ADD_GROUP("Mix", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "music_volume", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_music_volume", "get_music_volume");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wind_max_volume", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_wind_max_volume", "get_wind_max_volume");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "explosion_gain", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_explosion_gain", "get_explosion_gain");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "impact_gain", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_impact_gain", "get_impact_gain");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hit_carry_distance", PROPERTY_HINT_RANGE, "1,60,0.5"), "set_hit_carry_distance", "get_hit_carry_distance");
}
