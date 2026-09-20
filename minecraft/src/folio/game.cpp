#include "game.h"

#include "cycles/day_cycles.h"
#include "fog.h"
#include "lighting.h"
#include "noises.h"
#include "rendering.h"
#include "reveal.h"
#include "terrain.h"
#include "ticker.h"
#include "view/view.h"
#include "water.h"
#include "audio.h"
#include "ui/ui.h"
#include "weather.h"
#include "wind.h"

#include <godot_cpp/core/class_db.hpp>

namespace godot {

FolioGame *FolioGame::singleton = nullptr;

FolioGame::FolioGame() {}

FolioGame::~FolioGame() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void FolioGame::_ready() {
	singleton = this;
	_boot();
}

void FolioGame::_boot() {
	// Order matters: a child's _ready runs on add_child (this node is in the tree),
	// so singletons/subscriptions resolve as we go. Deps first.

	// FolioQuality (no deps) — needed by FolioView for its orbit tilt + speed zoom.
	quality = memnew(FolioQuality);
	quality->set_name("FolioQuality");
	add_child(quality);

	// FolioTicker (the clock) — must exist before anything that subscribes to it.
	ticker = memnew(FolioTicker);
	ticker->set_name("FolioTicker");
	add_child(ticker);

	// Time (owns the FolioTicker's scale + bullet time).
	time = memnew(FolioTime);
	time->set_name("Time");
	add_child(time);

	// Viewport (size + DPR + resize events).
	viewport = memnew(FolioViewport);
	viewport->set_name("Viewport");
	add_child(viewport);

	// Resources (batch loader).
	resources.instantiate();

	// FolioView (camera rig). Feed quality BEFORE add_child so its _ready inits the
	// orbit with the correct tilt.
	view = memnew(FolioView);
	view->set_name("FolioView");
	view->set_quality_level(quality->get_level());
	add_child(view);

	// Lighting (the sun + shared lighting uniforms; reads View optimal area).
	lighting = memnew(FolioLighting);
	lighting->set_name("Lighting");
	add_child(lighting);

	// Fog (distance fog + sky-gradient uniforms; reads View near/far).
	fog = memnew(FolioFog);
	fog->set_name("Fog");
	add_child(fog);

	// Reveal (intro reveal-ring uniforms; fully revealed by default).
	reveal = memnew(FolioReveal);
	reveal->set_name("Reveal");
	add_child(reveal);

	// Water (water-line uniforms).
	water = memnew(FolioWater);
	water->set_name("Water");
	add_child(water);

	// Noises (shared voronoi/perlin/hash textures as global samplers).
	noises = memnew(FolioNoises);
	noises->set_name("Noises");
	add_child(noises);

	// Terrain (ground-bounce gradient + data map as global samplers).
	terrain = memnew(FolioTerrain);
	terrain->set_name("Terrain");
	add_child(terrain);

	// Wind (shared sway field for foliage).
	wind = memnew(FolioWind);
	wind->set_name("Wind");
	add_child(wind);

	// Weather (noise-driven temperature/humidity/clouds/rain/snow; drives wind strength).
	weather = memnew(FolioWeather);
	weather->set_name("Weather");
	add_child(weather);

	// Rendering (post: native glow bloom + cheap-DOF tilt-shift, quality-switched).
	rendering = memnew(FolioRendering);
	rendering->set_name("Rendering");
	add_child(rendering);
	rendering->apply_quality(quality->get_level());
	if (quality->get_events().is_valid()) {
		quality->get_events()->on("change", callable_mp(rendering, &FolioRendering::apply_quality), 1);
	}

	// Day cycles (interpolate day/dusk/night/dawn -> lighting/fog/reveal).
	day_cycles = memnew(FolioDayCycles);
	day_cycles->set_name("DayCycles");
	add_child(day_cycles);

	// Audio (folio Audio.js reusable core: groups/items registry + spatial fade + mute).
	audio = memnew(FolioAudio);
	audio->set_name("Audio");
	add_child(audio);

	// UI (screen-space HUD root + mute button wired to FolioAudio).
	ui = memnew(FolioUI);
	ui->set_name("UI");
	add_child(ui);

	// Keep FolioView's quality in sync when it changes.
	if (quality->get_events().is_valid()) {
		quality->get_events()->on("change", callable_mp(view, &FolioView::set_quality_level), 1);
	}

	// TODO (as tiers land, in folio order): staged resource load + loading screen,
	// Rendering/post, Lighting/Fog/Reveal/Water, Materials, Physics, World, Player,
	// Inputs, Audio, UI.
}

void FolioGame::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_quality"), &FolioGame::get_quality);
	ClassDB::bind_method(D_METHOD("get_ticker"), &FolioGame::get_ticker);
	ClassDB::bind_method(D_METHOD("get_time"), &FolioGame::get_time);
	ClassDB::bind_method(D_METHOD("get_viewport_system"), &FolioGame::get_viewport_system);
	ClassDB::bind_method(D_METHOD("get_view"), &FolioGame::get_view);
	ClassDB::bind_method(D_METHOD("get_resources"), &FolioGame::get_resources);
	ClassDB::bind_method(D_METHOD("get_lighting"), &FolioGame::get_lighting);
	ClassDB::bind_method(D_METHOD("get_fog"), &FolioGame::get_fog);
	ClassDB::bind_method(D_METHOD("get_reveal"), &FolioGame::get_reveal);
	ClassDB::bind_method(D_METHOD("get_water"), &FolioGame::get_water);
	ClassDB::bind_method(D_METHOD("get_noises"), &FolioGame::get_noises);
	ClassDB::bind_method(D_METHOD("get_terrain"), &FolioGame::get_terrain);
	ClassDB::bind_method(D_METHOD("get_rendering"), &FolioGame::get_rendering);
	ClassDB::bind_method(D_METHOD("get_day_cycles"), &FolioGame::get_day_cycles);
	ClassDB::bind_method(D_METHOD("get_wind"), &FolioGame::get_wind);
	ClassDB::bind_method(D_METHOD("get_weather"), &FolioGame::get_weather);
	ClassDB::bind_method(D_METHOD("get_audio"), &FolioGame::get_audio);
	ClassDB::bind_method(D_METHOD("get_ui"), &FolioGame::get_ui);
}

} // namespace godot
