#ifndef FOLIO_TITLE_H
#define FOLIO_TITLE_H

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/variant/string.hpp>

#include "../../cui/cui_overlay.h"

namespace godot {

/**
 * Folio port — FolioTitle  (game start screen, on the CUIOverlay base)
 * -----------------------------------------------
 * Open/close/fade/backdrop come from CUIOverlay; this class adds the branded
 * title/subtitle/Play content and emits `started` when Play is pressed. Title +
 * subtitle text are settable so branding can change without touching layout.
 */
class FolioTitle : public CUIOverlay {
	GDCLASS(FolioTitle,
			CUIOverlay)

private:
	Label *title_label = nullptr;
	Label *subtitle_label = nullptr;
	Button *play_button = nullptr;
	String title_text = "Island Demo";
	String subtitle_text = "A tiny open world";

	void _on_play_pressed();

protected:
	void _build_content() override;
	static void _bind_methods();

public:
	FolioTitle();
	~FolioTitle();

	void set_title_text(const String &p_t);
	String get_title_text() const { return title_text; }
	void set_subtitle_text(const String &p_t);
	String get_subtitle_text() const { return subtitle_text; }
};

} // namespace godot

#endif // FOLIO_TITLE_H
