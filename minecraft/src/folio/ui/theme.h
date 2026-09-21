#ifndef FOLIO_THEME_H
#define FOLIO_THEME_H

#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/variant/color.hpp>

namespace godot {

/**
 * Folio port — FolioTheme
 * -----------------------------------------------
 * One shared Theme for the whole HUD, applied once on FolioUI's root Control so
 * every Button / PanelContainer / Label under it inherits the same look (folio's
 * DOM had a single CSS system; this is its Godot equivalent). Centralises the
 * palette + StyleBoxes that FolioUI and FolioMenu used to hand-build per widget.
 *
 * `build()` returns a ready Theme. The cartoon-soft look: rounded pills, a dim
 * translucent panel, white text.
 */
class FolioTheme {
public:
	// Palette (single source of truth for HUD colours).
	static const Color BG;
	static const Color BG_HOVER;
	static const Color PANEL_BG;
	static const Color TEXT;

	static Ref<Theme> build();
};

} // namespace godot

#endif // FOLIO_THEME_H
