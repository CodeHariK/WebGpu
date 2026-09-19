#include "audio.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "game.h"
#include "ticker.h"
#include "time.h"
#include "view/view.h"

using namespace godot;

const char *FolioAudio::MUTE_CFG_PATH = "user://folio_audio.cfg";

FolioAudio::FolioAudio() {}
FolioAudio::~FolioAudio() {}

// folio utilities/maths remapClamp.
static double remap_clamp(double v, double in_min, double in_max, double out_min, double out_max) {
	double t = (in_max - in_min);
	double r = (t == 0.0) ? out_min : (out_min + (out_max - out_min) * (v - in_min) / t);
	return Math::clamp(r, Math::min(out_min, out_max), Math::max(out_min, out_max));
}

static double linear_to_db_safe(double v) {
	if (v <= 0.0001) {
		return -80.0;
	}
	return 20.0 * Math::log(v) / Math::log(10.0);
}

void FolioAudio::_ready() {
	// Listener pinned to the camera so positional panning is correct.
	listener = memnew(AudioListener3D);
	listener->set_name("Listener");
	add_child(listener);

	_load_mute();
	_apply_mute();

	set_process_input(true);

	FolioTicker *ticker = FolioTicker::get_singleton();
	if (ticker) {
		ticker->connect_tick(callable_mp(this, &FolioAudio::update), 14);
	}

	// Godot has no browser user-gesture requirement, so open the gate now.
	init();
}

void FolioAudio::init() {
	if (initiated) {
		return;
	}
	initiated = true;
	// Fire any autoplay items now that the gate is open (folio init()).
	for (int i = 0; i < items.size(); i++) {
		if (items[i].autoplay) {
			play(items[i].id);
		}
	}
}

int FolioAudio::register_sound(const Dictionary &p_options) {
	Item item;
	item.id = items.size();
	item.group = p_options.get("group", "all");
	item.volume = (double)p_options.get("volume", 0.5);
	item.rate = (double)p_options.get("rate", 1.0);
	item.anti_spam = (double)p_options.get("anti_spam", 0.1);
	item.distance_fade = (double)p_options.get("distance_fade", -1.0);
	item.autoplay = (bool)p_options.get("autoplay", false);
	const bool loop = (bool)p_options.get("loop", false);
	const String path = p_options.get("path", "");

	// Positions -> positional item (folio options.positions).
	if (p_options.has("positions")) {
		Variant pv = p_options["positions"];
		if (pv.get_type() == Variant::VECTOR3) {
			item.positions.push_back((Vector3)pv);
		} else if (pv.get_type() == Variant::PACKED_VECTOR3_ARRAY) {
			item.positions = (PackedVector3Array)pv;
		} else if (pv.get_type() == Variant::ARRAY) {
			Array a = pv;
			for (int i = 0; i < a.size(); i++) {
				item.positions.push_back((Vector3)a[i]);
			}
		}
		item.positional = item.positions.size() > 0;
	}

	Ref<AudioStream> stream;
	if (!path.is_empty()) {
		stream = ResourceLoader::get_singleton()->load(path);
		if (stream.is_null()) {
			UtilityFunctions::printerr("FolioAudio > load error > ", path);
		}
	}

	if (item.positional) {
		AudioStreamPlayer3D *p = memnew(AudioStreamPlayer3D);
		p->set_name(String("Sound3D_") + String::num_int64(item.id));
		if (stream.is_valid()) {
			p->set_stream(stream);
		}
		// folio governs volume with its own linear fade -> disable Godot's model.
		p->set_attenuation_model(AudioStreamPlayer3D::ATTENUATION_DISABLED);
		p->set_max_distance(0.0); // 0 = unlimited; our fade handles falloff
		add_child(p);
		item.player_3d = p;
	} else {
		AudioStreamPlayer *p = memnew(AudioStreamPlayer);
		p->set_name(String("Sound_") + String::num_int64(item.id));
		if (stream.is_valid()) {
			p->set_stream(stream);
		}
		add_child(p);
		item.player_2d = p;
	}

	// Loop: WAV/OGG loop is a stream property; set it when supported.
	if (loop && stream.is_valid() && stream->has_method("set_loop")) {
		stream->call("set_loop", true);
	}

	items.push_back(item);

	// Register into its group.
	if (!groups.has(item.group)) {
		groups.insert(item.group, Group());
	}
	groups[item.group].item_ids.push_back(item.id);

	if (initiated && item.autoplay) {
		play(item.id);
	}
	return item.id;
}

void FolioAudio::play(int p_id) {
	if (!initiated || p_id < 0 || p_id >= items.size()) {
		return;
	}
	Item &item = items.write[p_id];

	// Anti-spam (folio: skip if replayed within anti_spam seconds).
	FolioTicker *ticker = FolioTicker::get_singleton();
	const double elapsed = ticker ? ticker->get_elapsed() : 0.0;
	if (item.anti_spam > 0.0 && (elapsed - item.last_play) < item.anti_spam) {
		return;
	}

	if (item.player_3d) {
		item.player_3d->play();
	} else if (item.player_2d) {
		item.player_2d->play();
	}
	item.last_play = elapsed;

	if (groups.has(item.group)) {
		groups[item.group].last_played_id = item.id;
	}
}

void FolioAudio::stop(int p_id) {
	if (p_id < 0 || p_id >= items.size()) {
		return;
	}
	const Item &item = items[p_id];
	if (item.player_3d) {
		item.player_3d->stop();
	} else if (item.player_2d) {
		item.player_2d->stop();
	}
}

void FolioAudio::play_group(const String &p_group) {
	if (!groups.has(p_group)) {
		return;
	}
	Group &g = groups[p_group];
	if (g.item_ids.is_empty()) {
		return;
	}
	// folio group.play(): round-robin the next item.
	int idx = 0;
	for (int i = 0; i < g.item_ids.size(); i++) {
		if (g.item_ids[i] == g.last_played_id) {
			idx = (i + 1) % g.item_ids.size();
			break;
		}
	}
	play(g.item_ids[idx]);
}

double FolioAudio::_compute_fade(const Item &p_item, Vector3 &r_nearest) const {
	FolioGame *game = FolioGame::get_singleton();
	Vector3 cam_pos;
	if (game && game->get_view() && game->get_view()->get_camera()) {
		cam_pos = game->get_view()->get_camera()->get_global_position();
	}
	double closest = 1.0e30;
	for (int i = 0; i < p_item.positions.size(); i++) {
		const double d = cam_pos.distance_to(p_item.positions[i]);
		if (d < closest) {
			closest = d;
			r_nearest = p_item.positions[i];
		}
	}
	if (p_item.distance_fade > 0.0) {
		return remap_clamp(closest, 0.0, p_item.distance_fade, 1.0, 0.0);
	}
	return 1.0;
}

void FolioAudio::update() {
	// Keep the listener on the camera.
	FolioGame *game = FolioGame::get_singleton();
	if (listener && game && game->get_view() && game->get_view()->get_camera()) {
		listener->set_global_transform(game->get_view()->get_camera()->get_global_transform());
		if (!listener->is_current()) {
			listener->make_current();
		}
	}

	// folio globalRate = time.scale / time.defaultScale.
	FolioTime *time = game ? game->get_time() : nullptr;
	if (time && time->get_default_scale() != 0.0) {
		global_rate = time->get_scale() / time->get_default_scale();
	}

	for (int i = 0; i < items.size(); i++) {
		Item &item = items.write[i];
		const double pitch = Math::clamp(item.rate * global_rate, 0.5, 4.0);

		if (item.positional && item.player_3d) {
			Vector3 nearest;
			const double fade = _compute_fade(item, nearest);
			if (item.positions.size() > 0) {
				item.player_3d->set_global_position(nearest);
			}
			item.applied_volume = item.volume * fade;
			item.player_3d->set_volume_db(linear_to_db_safe(item.applied_volume));
			item.player_3d->set_pitch_scale((float)pitch);
		} else if (item.player_2d) {
			item.applied_volume = item.volume;
			item.player_2d->set_volume_db(linear_to_db_safe(item.volume));
			item.player_2d->set_pitch_scale((float)pitch);
		}
	}
}

double FolioAudio::get_item_volume(int p_id) const {
	if (p_id < 0 || p_id >= items.size()) {
		return 0.0;
	}
	return items[p_id].applied_volume;
}

// --- Mute -------------------------------------------------------------------

void FolioAudio::set_mute(bool p_active) {
	if (mute_active == p_active) {
		return;
	}
	mute_active = p_active;
	_apply_mute();
	_save_mute();
	emit_signal("mute_changed", mute_active);
}

void FolioAudio::_apply_mute() {
	AudioServer *server = AudioServer::get_singleton();
	if (server) {
		server->set_bus_mute(0, mute_active); // 0 = Master
	}
}

void FolioAudio::_load_mute() {
	Ref<ConfigFile> cfg;
	cfg.instantiate();
	if (cfg->load(MUTE_CFG_PATH) == OK) {
		mute_active = (bool)cfg->get_value("audio", "mute", false);
	}
}

void FolioAudio::_save_mute() {
	Ref<ConfigFile> cfg;
	cfg.instantiate();
	cfg->load(MUTE_CFG_PATH); // ignore result; may not exist yet
	cfg->set_value("audio", "mute", mute_active);
	cfg->save(MUTE_CFG_PATH);
}

void FolioAudio::_input(const Ref<InputEvent> &p_event) {
	// Convenience: 'L' toggles mute (folio binds Keyboard.l), until FolioInputs lands.
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->get_keycode() == KEY_L) {
		toggle_mute();
	}
}

void FolioAudio::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioAudio::update);
	ClassDB::bind_method(D_METHOD("init"), &FolioAudio::init);
	ClassDB::bind_method(D_METHOD("is_initiated"), &FolioAudio::is_initiated);
	ClassDB::bind_method(D_METHOD("register_sound", "options"), &FolioAudio::register_sound);
	ClassDB::bind_method(D_METHOD("play", "id"), &FolioAudio::play);
	ClassDB::bind_method(D_METHOD("stop", "id"), &FolioAudio::stop);
	ClassDB::bind_method(D_METHOD("play_group", "group"), &FolioAudio::play_group);
	ClassDB::bind_method(D_METHOD("set_mute", "active"), &FolioAudio::set_mute);
	ClassDB::bind_method(D_METHOD("toggle_mute"), &FolioAudio::toggle_mute);
	ClassDB::bind_method(D_METHOD("is_muted"), &FolioAudio::is_muted);
	ClassDB::bind_method(D_METHOD("get_item_volume", "id"), &FolioAudio::get_item_volume);

	ADD_SIGNAL(MethodInfo("mute_changed", PropertyInfo(Variant::BOOL, "active")));
}
