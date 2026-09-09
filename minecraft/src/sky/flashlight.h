/**
 * @file flashlight.h
 * @brief Flashlight: a toggleable, flickering SpotLight3D for dark / horror scenes.
 */
#ifndef FLASHLIGHT_H
#define FLASHLIGHT_H

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/spot_light3d.hpp>

namespace godot {
class SkyCycle;
}

namespace godot {

/**
 * @class Flashlight
 * @brief A hand-torch cone. Parent it under the camera (or the vehicle) and it points where that
 * points; press `toggle_key` to switch it on/off. `flicker` adds an unsteady, failing-battery wobble to
 * the beam; `battery` (optional) drains while on and makes the flicker worse as it runs low. Just a
 * SpotLight3D with sensible torch defaults (warm cone, shadows) plus the toggle / flicker logic — set
 * range, angle and colour like any spot light.
 */
class Flashlight : public SpotLight3D {
	GDCLASS(Flashlight,
			SpotLight3D)

private:
	bool enabled = true;
	bool auto_night = false; // Follow the SkyCycle: on at night, off by day (headlights, lamps)
	int toggle_key = 70; // KEY_F
	float flicker = 0.0f; // 0 = steady, 1 = strong wobble
	float flicker_speed = 14.0f;
	bool use_battery = false;
	float battery = 1.0f; // 0..1
	float battery_drain = 0.01f; // Per second while on

	float _base_energy = 4.0f;
	float _phase = 0.0f;
	SkyCycle *_sky = nullptr;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	Flashlight();
	~Flashlight();

	void _process(double p_delta) override;
	void _unhandled_input(const Ref<InputEvent> &p_event) override;

	void set_enabled(bool p_on);
	bool get_enabled() const { return enabled; }
	void toggle() { set_enabled(!enabled); }

	void set_auto_night(bool p_v) { auto_night = p_v; }
	bool get_auto_night() const { return auto_night; }
	void set_toggle_key(int p_key) { toggle_key = p_key; }
	int get_toggle_key() const { return toggle_key; }
	void set_flicker(float p_v) { flicker = CLAMP(p_v, 0.0f, 1.0f); }
	float get_flicker() const { return flicker; }
	void set_flicker_speed(float p_v) { flicker_speed = MAX(0.0f, p_v); }
	float get_flicker_speed() const { return flicker_speed; }
	void set_use_battery(bool p_v) { use_battery = p_v; }
	bool get_use_battery() const { return use_battery; }
	void set_battery(float p_v) { battery = CLAMP(p_v, 0.0f, 1.0f); }
	float get_battery() const { return battery; }
	void set_battery_drain(float p_v) { battery_drain = MAX(0.0f, p_v); }
	float get_battery_drain() const { return battery_drain; }
};

} // namespace godot

#endif // FLASHLIGHT_H
