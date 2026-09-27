#ifndef FOLIO_SOUNDSCAPE_H
#define FOLIO_SOUNDSCAPE_H

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * Folio port — FolioSoundscape  (folio `Game/Audio.js` playlist + wind + impacts)
 * -----------------------------------------------
 * The concrete "content" layer on top of FolioAudio's core: a rotating music
 * playlist (folio 3 songs, switch when one ends), a forest wind loop whose volume
 * tracks the live FolioWeather wind value (folio's wind onPlaying), and a pooled
 * one-shot `play_hit(group, pos, rate)` other systems call on collisions
 * (explosions, prop impacts). Mix is exposed as inspector-editable properties.
 * Added to group "folio_soundscape" so GDScript can find it.
 */
class FolioSoundscape : public Node {
	GDCLASS(FolioSoundscape,
			Node)

private:
	AudioStreamPlayer *music = nullptr;
	Vector<Ref<AudioStream>> songs;
	int song_index = 0;

	AudioStreamPlayer *wind = nullptr;
	double wind_vol = 0.0;

	Vector<Ref<AudioStream>> hits_explosion;
	Vector<Ref<AudioStream>> hits_impact;
	Vector<AudioStreamPlayer3D *> hit_pool;

	// --- Tunable mix (inspector-editable) ---
	double music_volume = 0.2; // playlist loudness (linear)
	double wind_max_volume = 0.7; // wind loudness at full weather wind
	double explosion_gain = 1.0; // crate blast loudness
	double impact_gain = 1.0; // prop impact loudness
	double hit_carry_distance = 12.0; // one-shot AudioStreamPlayer3D unit_size

	void _on_music_finished();
	Ref<AudioStream> _pick(const Vector<Ref<AudioStream>> &p_bank) const;
	AudioStreamPlayer3D *_free_hit_player();

protected:
	static void _bind_methods();

public:
	FolioSoundscape();
	~FolioSoundscape();

	void _ready() override;
	void _process(double p_delta) override;

	// Play a random clip from a named bank ("explosion" | "impact") at a world
	// point, pitched by p_rate. Called by crates / props on collision.
	void play_hit(const String &p_group, const Vector3 &p_position, double p_rate);

	void set_music_volume(double v);
	double get_music_volume() const { return music_volume; }
	void set_wind_max_volume(double v) { wind_max_volume = v; }
	double get_wind_max_volume() const { return wind_max_volume; }
	void set_explosion_gain(double v) { explosion_gain = v; }
	double get_explosion_gain() const { return explosion_gain; }
	void set_impact_gain(double v) { impact_gain = v; }
	double get_impact_gain() const { return impact_gain; }
	void set_hit_carry_distance(double v) { hit_carry_distance = v; }
	double get_hit_carry_distance() const { return hit_carry_distance; }
};

} // namespace godot

#endif // FOLIO_SOUNDSCAPE_H
