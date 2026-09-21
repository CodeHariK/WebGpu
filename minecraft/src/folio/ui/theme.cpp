#include "theme.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

const Color FolioTheme::BG = Color(0.12, 0.14, 0.20, 0.78);
const Color FolioTheme::BG_HOVER = Color(0.20, 0.23, 0.32, 0.90);
const Color FolioTheme::PANEL_BG = Color(0.10, 0.12, 0.18, 0.96);
const Color FolioTheme::TEXT = Color(1, 1, 1, 1);

// Canonical, editor-editable theme shared with the rest of the game (CUI).
static const char *THEME_PATH = "res://scripts/ui/menu/ui_theme.tres";

static Ref<StyleBoxFlat> pill(const Color &c, int radius, int margin) {
	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	sb->set_bg_color(c);
	sb->set_corner_radius_all(radius);
	sb->set_content_margin_all(margin);
	return sb;
}

// Code fallback, used only if the .tres is missing/unreadable, so the HUD never
// renders unstyled.
static Ref<Theme> build_fallback() {
	Ref<Theme> theme;
	theme.instantiate();
	theme->set_stylebox("normal", "Button", pill(FolioTheme::BG, 14, 10));
	theme->set_stylebox("hover", "Button", pill(FolioTheme::BG_HOVER, 14, 10));
	theme->set_stylebox("pressed", "Button", pill(FolioTheme::BG_HOVER, 14, 10));
	theme->set_stylebox("focus", "Button", pill(FolioTheme::BG_HOVER, 14, 10));
	theme->set_color("font_color", "Button", FolioTheme::TEXT);
	theme->set_color("font_hover_color", "Button", FolioTheme::TEXT);
	theme->set_color("font_pressed_color", "Button", FolioTheme::TEXT);
	theme->set_font_size("font_size", "Button", 18);
	theme->set_stylebox("panel", "PanelContainer", pill(FolioTheme::PANEL_BG, 20, 24));
	theme->set_color("font_color", "Label", FolioTheme::TEXT);
	theme->set_font_size("font_size", "Label", 18);
	return theme;
}

Ref<Theme> FolioTheme::build() {
	Ref<Theme> theme = ResourceLoader::get_singleton()->load(THEME_PATH);
	if (theme.is_valid()) {
		return theme;
	}
	UtilityFunctions::push_warning("FolioTheme: '", THEME_PATH, "' not found; using code fallback.");
	return build_fallback();
}
