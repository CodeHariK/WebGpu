#ifndef FOLIO_WEATHER_H
#define FOLIO_WEATHER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

/**
 * Folio port — FolioWeather  (folio `Game/Weather.js`)
 * -----------------------------------------------
 * Computes a handful of weather properties each tick from a deterministic noise
 * of time (temperature / humidity / clouds / wind / rain / snow) and publishes
 * them as global shader uniforms; also drives FolioWind's strength. Consumers
 * (water ripples/ice/splashes, foliage sway, future Snow/Rain) read the globals.
 *
 * folio blends in day+year cycle values; YearCycles isn't ported, so `base_*`
 * stand in for the annual baseline. An override (folio `override.start`) can force
 * conditions for testing/events.
 *
 * Globals: folio_weather_temperature/_humidity/_clouds/_wind/_rain/_snow. Tick 8.
 */
class FolioWeather : public Node {
	GDCLASS(FolioWeather,
			Node)

private:
	double base_temperature = 12.0; // stands in for YearCycles annual temperature
	double base_humidity = 0.55; // stands in for YearCycles annual humidity

	double temperature = 12.0;
	double humidity = 0.55;
	double clouds = 0.0;
	double wind = 0.5;
	double rain = 0.0;
	double snow = 0.0;

	Dictionary override_values; // name -> forced value
	double override_strength = 0.0;

	bool globals_registered = false;
	void _register_globals();
	void _push();

	static double noise(double x);

protected:
	static void _bind_methods();

public:
	FolioWeather();
	~FolioWeather();

	void _ready() override;
	void update(); // tick 8

	// Force conditions (folio override.start); clear with an empty dictionary.
	void set_override(const Dictionary &p_values, double p_strength);
	void clear_override() { override_strength = 0.0; }

	void set_base_temperature(double v) { base_temperature = v; }
	void set_base_humidity(double v) { base_humidity = v; }

	double get_temperature() const { return temperature; }
	double get_humidity() const { return humidity; }
	double get_clouds() const { return clouds; }
	double get_wind() const { return wind; }
	double get_rain() const { return rain; }
	double get_snow() const { return snow; }
};

} // namespace godot

#endif // FOLIO_WEATHER_H
