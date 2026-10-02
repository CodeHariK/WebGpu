#include "character_audio.h"

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

AudioStreamPlayer3D *CharacterAudio::_make_oneshot(
		Node *p_parent,
		const String &p_path
) {
	Ref<AudioStream> stream = ResourceLoader::get_singleton()->load(p_path);
	if (!stream.is_valid()) {
		UtilityFunctions::printerr("CharacterAudio > load error > ", p_path);
		return nullptr;
	}
	AudioStreamPlayer3D *p = memnew(AudioStreamPlayer3D);
	p->set_stream(stream);
	p->set_unit_size(14.0f);
	p->set_max_db(3.0f);
	p->set_attenuation_model(AudioStreamPlayer3D::ATTENUATION_DISABLED); // always audible, still pans
	p_parent->add_child(p);
	return p;
}

void CharacterAudio::setup(
		Node *p_parent,
		const String &p_dir
) {
	if (!p_parent) {
		return;
	}
	jump = _make_oneshot(p_parent, p_dir + String("robot_jump.wav"));
	land = _make_oneshot(p_parent, p_dir + String("robot_land.wav"));
	steps.clear();
	for (int i = 1; i <= 5; i++) {
		String name = String("robot_step_0") + String::num_int64(i) + String(".wav");
		AudioStreamPlayer3D *s = _make_oneshot(p_parent, p_dir + name);
		if (s) {
			steps.push_back(s);
		}
	}
}

void CharacterAudio::update(
		bool p_grounded,
		float p_vy,
		float p_h_speed,
		bool p_jumped,
		float p_delta
) {
	// --- Jump: the frame a launch happens. ---
	if (p_jumped && jump) {
		jump->set_pitch_scale(UtilityFunctions::randf_range(0.95f, 1.05f));
		jump->play();
	}

	// --- Airtime bookkeeping + landing on the airborne -> grounded edge. ---
	if (!p_grounded) {
		fall_speed_peak = MAX(fall_speed_peak, -p_vy);
	} else if (!was_grounded) {
		if (land && fall_speed_peak > land_speed_quiet) {
			float t = CLAMP(
					(fall_speed_peak - land_speed_quiet) / MAX(0.01f, land_speed_loud - land_speed_quiet), 0.0f, 1.0f
			);
			land->set_volume_db(Math::linear_to_db(0.35f + 0.65f * t)); // soft taps stay soft
			land->set_pitch_scale(1.1f - 0.2f * t); // heavier landing = lower thud
			land->play();
		}
		fall_speed_peak = 0.0f;
		step_timer = 0.0f; // first step right away when you start walking off a landing
	}
	was_grounded = p_grounded;

	// --- Footsteps: grounded + moving, cadence tightens with speed. ---
	if (p_grounded && p_h_speed > step_speed_min && !steps.empty()) {
		step_timer -= p_delta;
		if (step_timer <= 0.0f) {
			int idx = (int)UtilityFunctions::randi_range(0, (int)steps.size() - 1);
			if (idx == last_step && steps.size() > 1) {
				idx = (idx + 1) % (int)steps.size();
			}
			last_step = idx;
			steps[idx]->set_pitch_scale(UtilityFunctions::randf_range(0.9f, 1.1f));
			steps[idx]->set_volume_db(Math::linear_to_db(0.6f));
			steps[idx]->play();
			step_timer = MAX(step_interval_min, stride_length / p_h_speed);
		}
	} else {
		step_timer = 0.0f; // next movement starts with an immediate step
	}
}

} // namespace godot
