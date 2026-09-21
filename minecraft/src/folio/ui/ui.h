#ifndef FOLIO_UI_H
#define FOLIO_UI_H

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/control.hpp>

namespace godot { class FolioMenu; class FolioTitle; class CUIModal; class CUIToast; class CUI; }

namespace godot {

/**
 * Folio port — FolioUI  (folio `Game/Menu.js` + DOM HUD, re-imagined for Godot)
 * -----------------------------------------------
 * folio's interface is HTML/CSS DOM (a `.js-menu` overlay + portfolio tabs), so
 * there's nothing to translate 1:1 — this is the reusable Godot foundation the
 * later HUD/menu build on: a screen-space CanvasLayer with a full-rect Control
 * root that other systems parent their widgets to, plus a generic open/closed
 * state (folio Menu.OPEN/CLOSED) for overlays.
 *
 * The one concrete widget in this slice is a mute button wired to FolioAudio:
 * pressing it toggles mute, and it re-labels itself from FolioAudio's
 * `mute_changed` signal so it always reflects the real state.
 *
 * Singleton via FolioGame (`get_ui()`); created after FolioAudio in boot.
 */
class FolioUI : public CanvasLayer {
	GDCLASS(FolioUI,
			CanvasLayer)

public:
	enum State {
		STATE_CLOSED,
		STATE_OPENING,
		STATE_OPEN,
		STATE_CLOSING,
	};

private:
	Control *root = nullptr; // full-rect container for all HUD widgets
	Button *mute_button = nullptr;
	Button *menu_button = nullptr;
	FolioMenu *menu = nullptr;
	CUIToast *notifications = nullptr;
	FolioTitle *title = nullptr;
	CUIModal *modal = nullptr;
	CUI *builder = nullptr; // shared widget factory (view construction)
	State state = STATE_CLOSED;

	static FolioUI *singleton;

	void _build_mute_button();
	void _refresh_mute_button(bool p_muted);
	void _on_mute_pressed();
	void _on_mute_changed(bool p_active);
	void _build_menu_button();
	void _on_menu_pressed();
	void _on_started();

protected:
	static void _bind_methods();

public:
	FolioUI();
	~FolioUI();

	void _ready() override;

	Control *get_root() const { return root; }
	FolioMenu *get_menu() const { return menu; }
	CUIToast *get_notifications() const { return notifications; }
	FolioTitle *get_title() const { return title; }
	CUIModal *get_modal() const { return modal; }
	CUI *get_cui() const { return builder; }
	void set_hud_visible(bool p_v);
	int get_state() const { return (int)state; }

	static FolioUI *get_singleton() { return singleton; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::FolioUI::State);

#endif // FOLIO_UI_H
