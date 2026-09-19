#include "weather.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include "game.h"
#include "ticker.h"
#include "wind.h"
#include "cycles/day_cycles.h"

using namespace godot;

static const char *SG_TEMPERATURE = "folio_weather_temperature";
static const char *SG_HUMIDITY = "folio_weather_humidity";
static const char *SG_CLOUDS = "folio_weather_clouds";
static const char *SG_WIND = "folio_weather_wind";
static const char *SG_RAIN = "folio_weather_rain";
static const char *SG_SNOW = "folio_weather_snow";

// folio absoluteProgress is deferred (needs day/year cycles counting up); we
// accumulate a slow "day count" here so the noise drifts over ~4 min per day.
static const double DAY_DURATION = 240.0;

FolioWeather::FolioWeather() {}
FolioWeather::~FolioWeather() {}

// folio Weather.noise: sin(x)·sin(1.678x)·sin(2.345x)  (range ~[-1,1])
double FolioWeather::noise(double x) {
	return Math::sin(x) * Math::sin(x * 1.678) * Math::sin(x * 2.345);
}

// folio utilities/maths remapClamp: linear remap then clamp to the out range
// (handles inverted out ranges, e.g. remapClamp(t, 0, 10, 0, -1)).
static double remap_clamp(double v, double in_min, double in_max, double out_min, double out_max) {
	double t = (in_max - in_min);
	double r = (t == 0.0) ? out_min : (out_min + (out_max - out_min) * (v - in_min) / t);
	double lo = Math::min(out_min, out_max);
	double hi = Math::max(out_min, out_max);
	return Math::clamp(r, lo, hi);
}

void FolioWeather::_ready() {
	_register_globals();
	update(); // seed values before first frame

	FolioTicker *ticker = FolioTicker::get_singleton();
	if (ticker) {
		ticker->connect_tick(callable_mp(this, &FolioWeather::update), 8);
	}
}

void FolioWeather::_register_globals() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	static bool s_added = false;
	if (!s_added) {
		rs->global_shader_parameter_add(SG_TEMPERATURE, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 12.0f);
		rs->global_shader_parameter_add(SG_HUMIDITY, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.55f);
		rs->global_shader_parameter_add(SG_CLOUDS, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.0f);
		rs->global_shader_parameter_add(SG_WIND, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.5f);
		rs->global_shader_parameter_add(SG_RAIN, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.0f);
		rs->global_shader_parameter_add(SG_SNOW, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.0f);
		s_added = true;
	}
	globals_registered = true;
}

void FolioWeather::_push() {
	if (!globals_registered) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	rs->global_shader_parameter_set(SG_TEMPERATURE, (float)temperature);
	rs->global_shader_parameter_set(SG_HUMIDITY, (float)humidity);
	rs->global_shader_parameter_set(SG_CLOUDS, (float)clouds);
	rs->global_shader_parameter_set(SG_WIND, (float)wind);
	rs->global_shader_parameter_set(SG_RAIN, (float)rain);
	rs->global_shader_parameter_set(SG_SNOW, (float)snow);
}

// Apply the override toward a forced value (folio: lerp(value, overrideValue, strength)).
static double apply_override(const Dictionary &values, double strength, const char *name, double value) {
	if (strength > 0.0 && values.has(name)) {
		double forced = (double)values[name];
		value = Math::lerp(value, forced, strength);
	}
	return value;
}

void FolioWeather::update() {
	FolioTicker *ticker = FolioTicker::get_singleton();
	// folio drives the noise by dayCycles.absoluteProgress (a monotonic day count);
	// derive one from elapsed scaled time until YearCycles/absoluteProgress land.
	double progress = ticker ? (ticker->get_elapsed_scaled() / DAY_DURATION) : 0.0;

	// Temperature: base (year+day stand-in) + noise variation. folio amp 7.5.
	temperature = base_temperature + noise(progress * 0.4) * 7.5;
	temperature = apply_override(override_values, override_strength, "temperature", temperature);

	// Humidity: base + noise variation. folio freq 0.36, amp 0.2.
	humidity = base_humidity + noise(progress * 0.36) * 0.2;
	humidity = apply_override(override_values, override_strength, "humidity", humidity);

	// Clouds: pure noise (folio freq 0.44, range ~[-1,1]).
	clouds = noise(progress * 0.44);
	clouds = apply_override(override_values, override_strength, "clouds", clouds);

	// Wind: noise remapped to [0,1] (folio freq 1, *0.5+0.5).
	wind = noise(progress * 1.0) * 0.5 + 0.5;
	wind = apply_override(override_values, override_strength, "wind", wind);

	// Rain: only when humid AND cloudy (folio remapClamp product).
	rain = remap_clamp(humidity, 0.65, 1.0, 0.0, 1.0) * remap_clamp(clouds, 0.0, 1.0, 0.0, 1.0);
	rain = apply_override(override_values, override_strength, "rain", rain);

	// Snow: rain that freezes, minus a melt term above 0°C (folio formula).
	double rain_ratio = remap_clamp(rain, 0.05, 0.3, 0.0, 1.0);
	double freeze_ratio = remap_clamp(temperature, 0.0, -5.0, 0.0, 1.0);
	double melt_ratio = remap_clamp(temperature, 0.0, 10.0, 0.0, -1.0);
	snow = rain_ratio * freeze_ratio + melt_ratio;
	snow = apply_override(override_values, override_strength, "snow", snow);

	_push();

	// Drive the shared wind field strength from weather (folio: wind.value scales sway).
	FolioGame *game = FolioGame::get_singleton();
	if (game && game->get_wind()) {
		game->get_wind()->set_strength(remap_clamp(wind, 0.0, 1.0, 0.15, 1.0));
	}
}

void FolioWeather::set_override(const Dictionary &p_values, double p_strength) {
	override_values = p_values;
	override_strength = Math::clamp(p_strength, 0.0, 1.0);
}

void FolioWeather::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioWeather::update);
	ClassDB::bind_method(D_METHOD("set_override", "values", "strength"), &FolioWeather::set_override);
	ClassDB::bind_method(D_METHOD("clear_override"), &FolioWeather::clear_override);
	ClassDB::bind_method(D_METHOD("set_base_temperature", "v"), &FolioWeather::set_base_temperature);
	ClassDB::bind_method(D_METHOD("set_base_humidity", "v"), &FolioWeather::set_base_humidity);
	ClassDB::bind_method(D_METHOD("get_temperature"), &FolioWeather::get_temperature);
	ClassDB::bind_method(D_METHOD("get_humidity"), &FolioWeather::get_humidity);
	ClassDB::bind_method(D_METHOD("get_clouds"), &FolioWeather::get_clouds);
	ClassDB::bind_method(D_METHOD("get_wind"), &FolioWeather::get_wind);
	ClassDB::bind_method(D_METHOD("get_rain"), &FolioWeather::get_rain);
	ClassDB::bind_method(D_METHOD("get_snow"), &FolioWeather::get_snow);
}
