#ifndef FOLIO_MENU_H
#define FOLIO_MENU_H

#include <godot_cpp/classes/button.hpp>

#include "../../cui/cui_overlay.h"

namespace godot {

/**
 * Folio port — FolioMenu  (game pause menu, on the CUIOverlay base)
 * -----------------------------------------------
 * A game-specific screen: the open/close/fade/backdrop machinery comes from
 * CUIOverlay; this class only builds its rows and wires them to game systems
 * (FolioAudio mute, FolioQuality level, Quit-to-title via the shared CUIModal).
 */
class FolioMenu : public CUIOverlay {
	GDCLASS(FolioMenu,
			CUIOverlay)

private:
	Button *sound_button = nullptr;
	Button *quality_button = nullptr;

	void _refresh_sound(bool p_muted);
	void _refresh_quality();
	void _on_sound_pressed();
	void _on_quality_pressed();
	void _on_quit_pressed();
	void _do_quit();
	void _on_mute_changed(bool p_active);

protected:
	void _build_content() override;
	void _on_escape() override;
	static void _bind_methods();

public:
	FolioMenu();
	~FolioMenu();
};

} // namespace godot

#endif // FOLIO_MENU_H
