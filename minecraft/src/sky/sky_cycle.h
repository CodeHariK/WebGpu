/**
 * @file sky_cycle.h
 * @brief SkyCycle: a stylized procedural day / dusk / night sky and the light + ambient that go with it.
 */
#ifndef SKY_CYCLE_H
#define SKY_CYCLE_H

#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/node_path.hpp>

namespace godot {

/**
 * @class SkyCycle
 * @brief Drives a fully procedural, textureless toon sky (no HDRI, nothing Earth-like) and the scene's
 * lighting from a single `time_of_day` (0 = midnight, 0.5 = noon). Each frame it arcs the sun across
 * the sky, points the sibling DirectionalLight3D along it (becoming a dim coloured moon light after
 * dusk), ramps its energy and warmth, sets the WorldEnvironment's ambient, and feeds sun / moon
 * directions and the interpolated sky palette into the sky shader — so the toon terrain and props swing
 * into dusk and night for free. The sky itself is banded gradient + flat sun disc + an invented ringed
 * moon (any colour you like) + twinkling stars at night. Colours come from three tweakable phase
 * palettes (day / dusk / night); dawn reuses the warm dusk palette. `day_length` seconds per full
 * cycle (0 = frozen at `time_of_day`).
 */
class SkyCycle : public WorldEnvironment {
	GDCLASS(SkyCycle,
			WorldEnvironment)

private:
	// Time
	float time_of_day = 0.5f; // 0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset
	float day_length = 120.0f; // Seconds for a full cycle; 0 = paused
	bool paused = false;
	NodePath sun_light_path; // Empty = first DirectionalLight3D sibling

	// Palette — sky
	Color day_zenith = Color(0.16f, 0.44f, 0.78f);
	Color day_horizon = Color(0.66f, 0.85f, 0.95f);
	Color dusk_zenith = Color(0.20f, 0.16f, 0.42f);
	Color dusk_horizon = Color(0.98f, 0.52f, 0.30f);
	Color night_zenith = Color(0.03f, 0.05f, 0.14f);
	Color night_horizon = Color(0.08f, 0.13f, 0.26f);
	int bands = 4; // Gradient posterization (1 = smooth)

	// Sun / moon / stars
	Color sun_color = Color(1.0f, 0.95f, 0.80f);
	float sun_size = 0.035f;
	Color moon_color = Color(1.0f, 0.86f, 0.55f); // An invented moon — gold by default, set any colour
	float moon_size = 0.06f;
	bool moon_ring = true;
	Color star_color = Color(0.9f, 0.95f, 1.0f);
	float star_density = 0.5f;

	// Lighting
	Color moonlight_color = Color(0.55f, 0.66f, 1.0f);
	float sun_energy = 1.25f;
	float moon_energy = 0.09f; // Low, but enough to feel the moon
	Color night_ambient = Color(0.06f, 0.07f, 0.13f);
	float day_ambient_energy = 0.65f;
	float night_ambient_energy = 0.10f; // Moody-dark: silhouettes read, but the torch still matters
	// Fog: dark and dense at night so distance falls off to black; near-zero by day.
	bool fog = true;
	Color night_fog_color = Color(0.02f, 0.03f, 0.05f);
	float night_fog_density = 0.022f;

	Ref<ShaderMaterial> _sky_mat;
	DirectionalLight3D *_sun = nullptr;
	bool _initialized = false;

	void _ensure_sky();
	DirectionalLight3D *_find_sun();
	void _apply();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	SkyCycle();
	~SkyCycle();

	void _process(double p_delta) override;

	void set_time_of_day(float p_t);
	float get_time_of_day() const { return time_of_day; }

	/// Sun below the horizon — used by lights that switch on at night.
	bool is_night() const;
	/// The active SkyCycle in the tree, or null (lights fall back to "always on").
	static SkyCycle *find(Node *p_node);

	// clang-format off
#define SC_PROP(m_type, m_name)               \
	void set_##m_name(m_type p_value) {       \
		m_name = p_value;                     \
		_apply();                             \
	}                                         \
	m_type get_##m_name() const { return m_name; }

	SC_PROP(float, day_length)
	SC_PROP(bool, paused)
	SC_PROP(Color, day_zenith)
	SC_PROP(Color, day_horizon)
	SC_PROP(Color, dusk_zenith)
	SC_PROP(Color, dusk_horizon)
	SC_PROP(Color, night_zenith)
	SC_PROP(Color, night_horizon)
	SC_PROP(int, bands)
	SC_PROP(Color, sun_color)
	SC_PROP(float, sun_size)
	SC_PROP(Color, moon_color)
	SC_PROP(float, moon_size)
	SC_PROP(bool, moon_ring)
	SC_PROP(Color, star_color)
	SC_PROP(float, star_density)
	SC_PROP(Color, moonlight_color)
	SC_PROP(float, sun_energy)
	SC_PROP(float, moon_energy)
	SC_PROP(Color, night_ambient)
	SC_PROP(float, day_ambient_energy)
	SC_PROP(float, night_ambient_energy)
	SC_PROP(bool, fog)
	SC_PROP(Color, night_fog_color)
	SC_PROP(float, night_fog_density)
#undef SC_PROP
	// clang-format on

	void set_sun_light_path(const NodePath &p_path) {
		sun_light_path = p_path;
		_sun = nullptr;
		_apply();
	}
	NodePath get_sun_light_path() const { return sun_light_path; }
};

/// Release the cached sky Shader. Call once at extension deinitialization, before Godot's renderer
/// shuts down, or it reports the shared Shader as leaked at exit.
void clear_sky_shader_cache();

} // namespace godot

#endif // SKY_CYCLE_H
