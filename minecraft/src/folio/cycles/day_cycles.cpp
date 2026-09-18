#include "day_cycles.h"

#include "../fog.h"
#include "../game.h"
#include "../lighting.h"
#include "../reveal.h"
#include "../ticker.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

FolioDayCycles::FolioDayCycles() {}

FolioDayCycles::~FolioDayCycles() {}

// folio DayCycles presets + keyframe stops (Game/Cycles/DayCycles.js).
// Sequence: day(0) day(0.15) dusk(0.25) night(0.35) night(0.6) dawn(0.8) day(0.9).
void FolioDayCycles::_build_keyframes() {
	Vector<double> stops;
	stops.push_back(0.0);
	stops.push_back(0.15);
	stops.push_back(0.25);
	stops.push_back(0.35);
	stops.push_back(0.6);
	stops.push_back(0.8);
	stops.push_back(0.9);
	cycle.set_stops(stops);

	// Preset colours (sRGB hex, as in folio).
	const Color day_light = Color::html("ffd2c2");
	const Color dusk_light = Color::html("ff8181");
	const Color night_light = Color::html("3240ff");
	const Color dawn_light = Color::html("ffa882");

	const Color day_shadow = Color::html("6d3fff");
	const Color dusk_shadow = Color::html("4e009c");
	const Color night_shadow = Color::html("2f00db");
	const Color dawn_shadow = Color::html("db004f");

	const Color day_fog_a = Color::html("00ffff");
	const Color dusk_fog_a = Color::html("3e53ff");
	const Color night_fog_a = Color::html("10266f");
	const Color dawn_fog_a = Color::html("f885ff");

	const Color day_fog_b = Color::html("9b89ff");
	const Color dusk_fog_b = Color::html("ff4ce4");
	const Color night_fog_b = Color::html("490a42");
	const Color dawn_fog_b = Color::html("ff7d24");

	const Color day_reveal = Color::html("5f7dff");
	const Color dusk_reveal = Color::html("ff86d9");
	const Color night_reveal = Color::html("b678ff");
	const Color dawn_reveal = Color::html("ff9d9d");

	// Each track parallels the stops: [day, day, dusk, night, night, dawn, day].
	Vector<Color> light_color;
	light_color.push_back(day_light);
	light_color.push_back(day_light);
	light_color.push_back(dusk_light);
	light_color.push_back(night_light);
	light_color.push_back(night_light);
	light_color.push_back(dawn_light);
	light_color.push_back(day_light);
	cycle.add_color_track("lightColor", light_color);

	Vector<double> light_intensity;
	light_intensity.push_back(1.2);
	light_intensity.push_back(1.2);
	light_intensity.push_back(1.2);
	light_intensity.push_back(3.8);
	light_intensity.push_back(3.8);
	light_intensity.push_back(1.2);
	light_intensity.push_back(1.2);
	cycle.add_float_track("lightIntensity", light_intensity);

	Vector<Color> shadow_color;
	shadow_color.push_back(day_shadow);
	shadow_color.push_back(day_shadow);
	shadow_color.push_back(dusk_shadow);
	shadow_color.push_back(night_shadow);
	shadow_color.push_back(night_shadow);
	shadow_color.push_back(dawn_shadow);
	shadow_color.push_back(day_shadow);
	cycle.add_color_track("shadowColor", shadow_color);

	Vector<Color> fog_a;
	fog_a.push_back(day_fog_a);
	fog_a.push_back(day_fog_a);
	fog_a.push_back(dusk_fog_a);
	fog_a.push_back(night_fog_a);
	fog_a.push_back(night_fog_a);
	fog_a.push_back(dawn_fog_a);
	fog_a.push_back(day_fog_a);
	cycle.add_color_track("fogColorA", fog_a);

	Vector<Color> fog_b;
	fog_b.push_back(day_fog_b);
	fog_b.push_back(day_fog_b);
	fog_b.push_back(dusk_fog_b);
	fog_b.push_back(night_fog_b);
	fog_b.push_back(night_fog_b);
	fog_b.push_back(dawn_fog_b);
	fog_b.push_back(day_fog_b);
	cycle.add_color_track("fogColorB", fog_b);

	Vector<double> fog_near;
	fog_near.push_back(0.315);
	fog_near.push_back(0.315);
	fog_near.push_back(0.0);
	fog_near.push_back(-0.85);
	fog_near.push_back(-0.85);
	fog_near.push_back(0.3);
	fog_near.push_back(0.315);
	cycle.add_float_track("fogNearRatio", fog_near);

	Vector<double> fog_far;
	fog_far.push_back(1.25);
	fog_far.push_back(1.25);
	fog_far.push_back(1.25);
	fog_far.push_back(1.0);
	fog_far.push_back(1.0);
	fog_far.push_back(1.25);
	fog_far.push_back(1.25);
	cycle.add_float_track("fogFarRatio", fog_far);

	Vector<Color> reveal_color;
	reveal_color.push_back(day_reveal);
	reveal_color.push_back(day_reveal);
	reveal_color.push_back(dusk_reveal);
	reveal_color.push_back(night_reveal);
	reveal_color.push_back(night_reveal);
	reveal_color.push_back(dawn_reveal);
	reveal_color.push_back(day_reveal);
	cycle.add_color_track("revealColor", reveal_color);

	Vector<double> reveal_intensity;
	reveal_intensity.push_back(12.0);
	reveal_intensity.push_back(12.0);
	reveal_intensity.push_back(5.55);
	reveal_intensity.push_back(10.0);
	reveal_intensity.push_back(10.0);
	reveal_intensity.push_back(4.85);
	reveal_intensity.push_back(12.0);
	cycle.add_float_track("revealIntensity", reveal_intensity);

	cycle.set_duration(4.0 * 60.0); // folio: 4 minutes per day
	cycle.finalize();
}

void FolioDayCycles::_ready() {
	_build_keyframes();

	// Let Lighting rotate the sun from day_progress.
	FolioGame *game = FolioGame::get_singleton();
	if (game && game->get_lighting()) {
		game->get_lighting()->set_use_day_cycles(true);
	}

	FolioTicker *ticker = FolioTicker::get_singleton();
	if (ticker) {
		ticker->connect_tick(callable_mp(this, &FolioDayCycles::update), 8);
	}
}

void FolioDayCycles::update() {
	if (!enabled) {
		return;
	}

	FolioTicker *ticker = FolioTicker::get_singleton();
	const double elapsed = ticker ? ticker->get_elapsed() : 0.0;
	cycle.update(elapsed);
	_push();
}

// Feed the interpolated values into the systems that own the uniforms.
void FolioDayCycles::_push() {
	FolioGame *game = FolioGame::get_singleton();
	if (!game) {
		return;
	}

	if (FolioLighting *lighting = game->get_lighting()) {
		lighting->set_day_progress(cycle.get_progress());
		lighting->set_day_light_color(cycle.get_color("lightColor"));
		lighting->set_day_light_intensity(cycle.get_float("lightIntensity"));
		lighting->set_day_shadow_color(cycle.get_color("shadowColor"));
	}

	if (FolioFog *fog = game->get_fog()) {
		fog->set_day_fog_colors(cycle.get_color("fogColorA"), cycle.get_color("fogColorB"));
		fog->set_day_fog_ratios(cycle.get_float("fogNearRatio"), cycle.get_float("fogFarRatio"));
	}

	if (FolioReveal *reveal = game->get_reveal()) {
		reveal->set_day_reveal_color(cycle.get_color("revealColor"));
		reveal->set_day_reveal_intensity(cycle.get_float("revealIntensity"));
	}
}

void FolioDayCycles::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_progress_override", "progress"), &FolioDayCycles::set_progress_override);
	ClassDB::bind_method(D_METHOD("set_duration", "seconds"), &FolioDayCycles::set_duration);
	ClassDB::bind_method(D_METHOD("get_progress"), &FolioDayCycles::get_progress);
	ClassDB::bind_method(D_METHOD("update"), &FolioDayCycles::update);
}
