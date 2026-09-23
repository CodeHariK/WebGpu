#include "year_cycles.h"

#include "../game.h"
#include "../ticker.h"
#include "../weather.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

FolioYearCycles::FolioYearCycles() {}
FolioYearCycles::~FolioYearCycles() {}

// folio YearCycles season presets (Game/Cycles/YearCycles.js). Stops sit at the
// centre of each season (0.125 apart from the quarter boundaries); FolioCycle's
// finalize() wraps fall -> winter for a seamless loop.
void FolioYearCycles::_build_keyframes() {
	Vector<double> stops;
	stops.push_back(0.125); // winter
	stops.push_back(0.375); // spring
	stops.push_back(0.625); // summer
	stops.push_back(0.875); // fall
	cycle.set_stops(stops);

	// Each track parallels the stops: [winter, spring, summer, fall].
	Vector<double> leaves;
	leaves.push_back(0.25);
	leaves.push_back(0.0);
	leaves.push_back(0.25);
	leaves.push_back(1.0);
	cycle.add_float_track("leaves", leaves);

	Vector<double> temperature;
	temperature.push_back(winter_temperature); // winter: below freezing so it snows on its own
	temperature.push_back(15.0);
	temperature.push_back(25.0);
	temperature.push_back(15.0);
	cycle.add_float_track("temperature", temperature);

	Vector<double> humidity;
	humidity.push_back(0.8);
	humidity.push_back(0.65);
	humidity.push_back(0.5);
	humidity.push_back(0.65);
	cycle.add_float_track("humidity", humidity);

	Vector<double> clouds;
	clouds.push_back(0.65);
	clouds.push_back(0.45);
	clouds.push_back(0.3);
	clouds.push_back(0.65);
	cycle.add_float_track("clouds", clouds);

	Vector<double> wind;
	wind.push_back(0.3);
	wind.push_back(0.2);
	wind.push_back(0.1);
	wind.push_back(0.25);
	cycle.add_float_track("wind", wind);

	// Seasonal vegetation tint (multiplied into grass/foliage albedo).
	Vector<Color> tint;
	tint.push_back(Color(0.80, 0.88, 1.00)); // winter: cool / frosty
	tint.push_back(Color(0.85, 1.06, 0.80)); // spring: fresh green
	tint.push_back(Color(1.00, 1.00, 0.82)); // summer: warm, bright
	tint.push_back(Color(1.18, 0.86, 0.52)); // fall: golden / amber
	cycle.add_color_track("seasonTint", tint);

	// folio: a real year. Here a short, game-friendly loop so seasons actually
	// cycle in play (override with set_duration / set_progress_override).
	cycle.set_duration(20.0 * 60.0); // 20 min per year (~5 min per season)
	cycle.finalize();
}

void FolioYearCycles::_ready() {
	_build_keyframes();
	_register_globals();

	FolioTicker *ticker = FolioTicker::get_singleton();
	if (ticker) {
		ticker->connect_tick(callable_mp(this, &FolioYearCycles::update), 7);
	}
}

void FolioYearCycles::update() {
	if (!enabled) {
		return;
	}
	FolioTicker *ticker = FolioTicker::get_singleton();
	const double elapsed = ticker ? ticker->get_elapsed() : 0.0;
	cycle.update(elapsed);
	_push();
}

// Feed the seasonal baseline into Weather (retires its base_* stand-ins). Leaves
// pulls its own density from get_leaves().
void FolioYearCycles::_push() {
	FolioGame *game = FolioGame::get_singleton();
	if (!game) {
		return;
	}
	if (FolioWeather *weather = game->get_weather()) {
		weather->set_base_temperature(get_temperature());
		weather->set_base_humidity(get_humidity());
		weather->set_base_clouds(get_clouds());
	}

	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs && tint_registered) {
		Color t = cycle.get_color("seasonTint");
		rs->global_shader_parameter_set("folio_season_tint", Vector3(t.r, t.g, t.b));
	}
}

void FolioYearCycles::_register_globals() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs || tint_registered) {
		return;
	}
	// Default white so grass/foliage look unchanged until the first push.
	// (Do NOT global_shader_parameter_get to check — it is editor-only and errors
	// at runtime; register once, guarded by the flag, like the other systems.)
	rs->global_shader_parameter_add("folio_season_tint", RenderingServer::GLOBAL_VAR_TYPE_VEC3, Vector3(1, 1, 1));
	tint_registered = true;
}

void FolioYearCycles::seek_season(double p_phase) {
	FolioTicker *ticker = FolioTicker::get_singleton();
	cycle.seek(p_phase, ticker ? ticker->get_elapsed() : 0.0);
}

// Re-tune the winter cold snap live. FolioCycle bakes wrap-around "fake steps" in
// finalize(), so the winter keyframe ends up duplicated at shifted indices — rather
// than poke those, reset the cycle to a clean instance and rebuild, then restore the
// duration and re-seek to the current phase so the year does not visibly jump.
void FolioYearCycles::set_winter_temperature(double p_celsius) {
	winter_temperature = p_celsius;
	const double dur = cycle.get_duration();
	const double cur = cycle.get_progress();
	cycle = FolioCycle();
	_build_keyframes();
	cycle.set_duration(dur);
	FolioTicker *ticker = FolioTicker::get_singleton();
	cycle.seek(cur, ticker ? ticker->get_elapsed() : 0.0);
}

void FolioYearCycles::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioYearCycles::update);
	ClassDB::bind_method(D_METHOD("set_progress_override", "progress"), &FolioYearCycles::set_progress_override);
	ClassDB::bind_method(D_METHOD("seek_season", "phase"), &FolioYearCycles::seek_season);
	ClassDB::bind_method(D_METHOD("set_winter_temperature", "celsius"), &FolioYearCycles::set_winter_temperature);
	ClassDB::bind_method(D_METHOD("get_winter_temperature"), &FolioYearCycles::get_winter_temperature);
	ClassDB::bind_method(D_METHOD("set_duration", "seconds"), &FolioYearCycles::set_duration);
	ClassDB::bind_method(D_METHOD("get_progress"), &FolioYearCycles::get_progress);
	ClassDB::bind_method(D_METHOD("get_leaves"), &FolioYearCycles::get_leaves);
	ClassDB::bind_method(D_METHOD("get_temperature"), &FolioYearCycles::get_temperature);
	ClassDB::bind_method(D_METHOD("get_humidity"), &FolioYearCycles::get_humidity);
	ClassDB::bind_method(D_METHOD("get_clouds"), &FolioYearCycles::get_clouds);
	ClassDB::bind_method(D_METHOD("get_wind"), &FolioYearCycles::get_wind);
}
