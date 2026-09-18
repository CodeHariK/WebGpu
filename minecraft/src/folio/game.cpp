#include "game.h"

#include "fog.h"
#include "lighting.h"
#include "reveal.h"
#include "ticker.h"
#include "view/view.h"

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
}

} // namespace godot
