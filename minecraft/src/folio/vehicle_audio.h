#ifndef FOLIO_VEHICLE_AUDIO_H
#define FOLIO_VEHICLE_AUDIO_H

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>

namespace godot {

/**
 * Folio port — FolioVehicleAudio  (folio `Game/Player.js` engine/spin/boost/horn)
 * -----------------------------------------------
 * A child of the car (an ArcadeVehicle / RigidBody3D). Owns four positional
 * players that follow the car and are driven each physics frame from its motion,
 * matching folio's onPlaying easing:
 *   - engine: idle loop, volume/pitch rise with throttle (+boost)
 *   - spin:   rolling loop, volume/pitch rise with ground speed
 *   - boost:  force-field loop while boosting
 *   - horn:   one-shot on H
 * The mix is exposed as inspector-editable properties so it can be balanced live
 * without a rebuild. Spatialised via the AudioListener3D FolioAudio pins to the
 * camera.
 */
class FolioVehicleAudio : public Node3D {
	GDCLASS(FolioVehicleAudio,
			Node3D)

private:
	AudioStreamPlayer3D *engine = nullptr;
	AudioStreamPlayer3D *spin = nullptr;
	AudioStreamPlayer3D *boost = nullptr;
	AudioStreamPlayer3D *horn = nullptr;

	RigidBody3D *car = nullptr;

	// --- Tunable mix (inspector-editable) ---
	double engine_gain = 0.8; // engine output loudness (linear)
	double engine_pitch_min = 0.6; // pitch at idle
	double engine_pitch_max = 1.1; // pitch at full throttle
	double spin_speed_sensitivity = 0.1; // how fast the rolling sound ramps with speed
	double spin_gain = 0.3; // rolling output loudness
	double spin_pitch_max = 2.0; // rolling pitch at top speed
	double boost_gain = 0.3; // boost whoosh loudness
	double carry_distance = 14.0; // AudioStreamPlayer3D unit_size (how far it carries)
	double attack_ease = 10.0; // volume ramp-up speed
	double release_ease = 2.5; // volume ramp-down speed
	double pitch_ease = 5.0; // pitch smoothing speed

	// --- Eased running state (not tunable; the live value each frame) ---
	double engine_vol = 0.05;
	double engine_rate = 0.6;
	double spin_vol = 0.0;
	double spin_rate = 1.0;
	double boost_vol = 0.0;
	double boost_rate = 1.0;

	AudioStreamPlayer3D *_make_loop(const String &p_path);
	void _apply_carry();

protected:
	static void _bind_methods();

public:
	FolioVehicleAudio();
	~FolioVehicleAudio();

	void _ready() override;
	void _physics_process(double p_delta) override;
	void _unhandled_key_input(const Ref<InputEvent> &p_event) override;

	void set_engine_gain(double v) { engine_gain = v; }
	double get_engine_gain() const { return engine_gain; }
	void set_engine_pitch_min(double v) { engine_pitch_min = v; }
	double get_engine_pitch_min() const { return engine_pitch_min; }
	void set_engine_pitch_max(double v) { engine_pitch_max = v; }
	double get_engine_pitch_max() const { return engine_pitch_max; }
	void set_spin_speed_sensitivity(double v) { spin_speed_sensitivity = v; }
	double get_spin_speed_sensitivity() const { return spin_speed_sensitivity; }
	void set_spin_gain(double v) { spin_gain = v; }
	double get_spin_gain() const { return spin_gain; }
	void set_spin_pitch_max(double v) { spin_pitch_max = v; }
	double get_spin_pitch_max() const { return spin_pitch_max; }
	void set_boost_gain(double v) { boost_gain = v; }
	double get_boost_gain() const { return boost_gain; }
	void set_carry_distance(double v);
	double get_carry_distance() const { return carry_distance; }
	void set_attack_ease(double v) { attack_ease = v; }
	double get_attack_ease() const { return attack_ease; }
	void set_release_ease(double v) { release_ease = v; }
	double get_release_ease() const { return release_ease; }
	void set_pitch_ease(double v) { pitch_ease = v; }
	double get_pitch_ease() const { return pitch_ease; }
};

} // namespace godot

#endif // FOLIO_VEHICLE_AUDIO_H
