#ifndef FOLIO_GAME_H
#define FOLIO_GAME_H

#include "quality.h"
#include "resources.h"
#include "time.h"
#include "viewport.h"

#include <godot_cpp/classes/node.hpp>

namespace godot {

class FolioTicker;
class FolioView;

/**
 * Folio port — Game  (registered as `FolioGame`)
 * ----------------------------------------------
 * The boot orchestrator and singleton, from folio-2025 `Game/Game.js`.
 *
 * folio's Game is the composition root: `Game.getInstance()` plus `this.ticker`,
 * `this.viewport`, `this.view`, ... that every system reaches through. This is
 * the Tier-0 slice: it constructs and wires the ported spine in folio's
 * dependency order, and replaces each system's standalone singleton/hook lookups
 * with a real owner.
 *
 * Construction order (deps first): FolioQuality → FolioTicker → Time → Viewport →
 * Resources → FolioView. Children are created in code (folio style: `new FolioTicker()`),
 * so their `_ready` runs on add_child and the singletons/subscriptions resolve in
 * order.
 *
 * Renamed to `FolioGame` (this project already has a `GameManager`).
 *
 * Not ported yet (folio boot continues here — TODO as tiers land): staged
 * resource loading + loading screen, Rendering/post, Lighting/Fog/Reveal/Water,
 * Materials, Physics/Rapier, World, Player, Inputs, Audio, UI. Add them to
 * `_boot()` in dependency order as each is ported.
 */
class FolioGame : public Node {
	GDCLASS(FolioGame,
			Node)

private:
	FolioQuality *quality = nullptr;
	FolioTicker *ticker = nullptr;
	FolioTime *time = nullptr;
	FolioViewport *viewport = nullptr;
	FolioView *view = nullptr;
	Ref<FolioResources> resources;

	static FolioGame *singleton;

	void _boot();

protected:
	static void _bind_methods();

public:
	FolioGame();
	~FolioGame();

	void _ready() override;

	FolioQuality *get_quality() const { return quality; }
	FolioTicker *get_ticker() const { return ticker; }
	FolioTime *get_time() const { return time; }
	FolioViewport *get_viewport_system() const { return viewport; }
	FolioView *get_view() const { return view; }
	Ref<FolioResources> get_resources() const { return resources; }

	static FolioGame *get_singleton() { return singleton; }
};

} // namespace godot

#endif // FOLIO_GAME_H
