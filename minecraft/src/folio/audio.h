#ifndef FOLIO_AUDIO_H
#define FOLIO_AUDIO_H

#include <godot_cpp/classes/audio_listener3d.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

/**
 * Folio port — FolioAudio  (folio `Game/Audio.js`)
 * -----------------------------------------------
 * The reusable core of folio's Howler-based audio manager, backed by Godot's
 * audio nodes. folio keeps named *groups* of sound *items*; each item wraps one
 * clip with volume / rate / loop / optional world positions + a linear distance
 * fade, plus anti-spam. `register()` builds an item; `play()` respects the init
 * gate and anti-spam; the tick (order 14) applies spatial fade + a global rate
 * derived from time scale; a persisted `mute` toggle mirrors folio's soundToggle.
 *
 * Backing: non-positional items -> AudioStreamPlayer; positional items ->
 * AudioStreamPlayer3D (attenuation disabled so folio's own linear fade governs
 * volume) moved to the nearest of its positions each tick, with an
 * AudioListener3D pinned to the view camera so panning is correct.
 *
 * Out of scope (portfolio content, per the port): the concrete playlist /
 * ambiances / one-offs and their area/day-night logic. This is the engine slice
 * other systems call `register()` on. Ticks at order 14.
 */
class FolioAudio : public Node {
	GDCLASS(FolioAudio,
			Node)

private:
	struct Item {
		AudioStreamPlayer *player_2d = nullptr;
		AudioStreamPlayer3D *player_3d = nullptr;
		bool positional = false;
		PackedVector3Array positions;
		double volume = 0.5; // linear base volume
		double rate = 1.0; // base pitch
		double distance_fade = -1.0; // <=0 -> no linear fade
		double anti_spam = 0.1; // seconds
		double last_play = -1.0e30;
		double applied_volume = 0.0; // debug: last volume*fade actually set
		bool autoplay = false;
		String group;
		int id = -1;
	};

	struct Group {
		Vector<int> item_ids;
		int last_played_id = -1;
	};

	Vector<Item> items;
	HashMap<String, Group> groups;

	bool initiated = false;
	double global_rate = 1.0;

	// Mute (folio soundToggle; persisted to user://folio_audio.cfg).
	bool mute_active = false;
	static const char *MUTE_CFG_PATH;
	void _load_mute();
	void _save_mute();
	void _apply_mute();

	AudioListener3D *listener = nullptr;

	double _compute_fade(const Item &p_item, Vector3 &r_nearest) const;

protected:
	static void _bind_methods();

public:
	FolioAudio();
	~FolioAudio();

	void _ready() override;
	void update(); // tick 14

	// folio Audio.init(): open the gate + fire autoplays.
	void init();
	bool is_initiated() const { return initiated; }

	// folio register(options): build one item, return its integer handle.
	// options keys: path, group, volume, rate, loop, autoplay, anti_spam,
	//               positions (PackedVector3Array or Vector3), distance_fade.
	int register_sound(const Dictionary &p_options);

	void play(int p_id);
	void stop(int p_id);
	// Round-robin the next item in a group (folio group.play()).
	void play_group(const String &p_group);

	// Mute (folio mute.activate/deactivate/toggle + persisted state).
	void set_mute(bool p_active);
	void toggle_mute() { set_mute(!mute_active); }
	bool is_muted() const { return mute_active; }

	// Debug/verification: last applied volume for a positional item.
	double get_item_volume(int p_id) const;

	void _input(const Ref<InputEvent> &p_event) override;
};

} // namespace godot

#endif // FOLIO_AUDIO_H
