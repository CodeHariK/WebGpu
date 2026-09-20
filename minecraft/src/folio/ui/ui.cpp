#include "ui.h"

#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "../game.h"
#include "../audio.h"
#include "menu.h"

using namespace godot;

FolioUI *FolioUI::singleton = nullptr;

FolioUI::FolioUI() {
	singleton = this;
}

FolioUI::~FolioUI() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void FolioUI::_ready() {
	set_layer(200); // above FolioRendering's post (DOF) layer (100), so the HUD stays sharp

	// Full-rect root that HUD widgets attach to.
	root = memnew(Control);
	root->set_name("Root");
	root->set_anchors_preset(Control::PRESET_FULL_RECT);
	root->set_mouse_filter(Control::MOUSE_FILTER_IGNORE); // clicks pass unless a child grabs
	add_child(root);

	_build_mute_button();
	_build_menu_button();

	// Menu overlay lives under the root; toggled by the HUD button or Esc.
	menu = memnew(FolioMenu);
	menu->set_name("Menu");
	root->add_child(menu);
}

void FolioUI::_build_mute_button() {
	mute_button = memnew(Button);
	mute_button->set_name("MuteButton");
	mute_button->set_anchors_preset(Control::PRESET_BOTTOM_LEFT);
	mute_button->set_offset(Side::SIDE_LEFT, 20.0);
	mute_button->set_offset(Side::SIDE_TOP, -64.0);
	mute_button->set_offset(Side::SIDE_RIGHT, 160.0);
	mute_button->set_offset(Side::SIDE_BOTTOM, -20.0);
	mute_button->set_focus_mode(Control::FOCUS_NONE);

	// Soft rounded pill (cartoon-friendly).
	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	sb->set_bg_color(Color(0.12, 0.14, 0.20, 0.75));
	sb->set_corner_radius_all(16);
	sb->set_content_margin_all(8);
	mute_button->add_theme_stylebox_override("normal", sb);
	Ref<StyleBoxFlat> sb_hover = sb->duplicate();
	sb_hover->set_bg_color(Color(0.18, 0.21, 0.30, 0.85));
	mute_button->add_theme_stylebox_override("hover", sb_hover);
	mute_button->add_theme_stylebox_override("pressed", sb_hover);
	mute_button->add_theme_color_override("font_color", Color(1, 1, 1));

	root->add_child(mute_button);
	mute_button->connect("pressed", callable_mp(this, &FolioUI::_on_mute_pressed));

	// Reflect current audio state + subscribe to changes.
	FolioGame *game = FolioGame::get_singleton();
	FolioAudio *audio = game ? game->get_audio() : nullptr;
	bool muted = audio ? audio->is_muted() : false;
	if (audio) {
		audio->connect("mute_changed", callable_mp(this, &FolioUI::_on_mute_changed));
	}
	_refresh_mute_button(muted);
}

void FolioUI::_refresh_mute_button(bool p_muted) {
	if (!mute_button) {
		return;
	}
	mute_button->set_text(p_muted ? "Sound: Off" : "Sound: On");
}

void FolioUI::_on_mute_pressed() {
	FolioGame *game = FolioGame::get_singleton();
	FolioAudio *audio = game ? game->get_audio() : nullptr;
	if (audio) {
		audio->toggle_mute();
	}
}

void FolioUI::_on_mute_changed(bool p_active) {
	_refresh_mute_button(p_active);
}

void FolioUI::_build_menu_button() {
	menu_button = memnew(Button);
	menu_button->set_name("MenuButton");
	menu_button->set_anchors_preset(Control::PRESET_TOP_RIGHT);
	menu_button->set_offset(Side::SIDE_LEFT, -160.0);
	menu_button->set_offset(Side::SIDE_TOP, 20.0);
	menu_button->set_offset(Side::SIDE_RIGHT, -20.0);
	menu_button->set_offset(Side::SIDE_BOTTOM, 64.0);
	menu_button->set_focus_mode(Control::FOCUS_NONE);
	menu_button->set_text("Menu");

	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	sb->set_bg_color(Color(0.12, 0.14, 0.20, 0.75));
	sb->set_corner_radius_all(16);
	sb->set_content_margin_all(8);
	menu_button->add_theme_stylebox_override("normal", sb);
	Ref<StyleBoxFlat> hov = sb->duplicate();
	hov->set_bg_color(Color(0.18, 0.21, 0.30, 0.85));
	menu_button->add_theme_stylebox_override("hover", hov);
	menu_button->add_theme_stylebox_override("pressed", hov);
	menu_button->add_theme_color_override("font_color", Color(1, 1, 1));

	root->add_child(menu_button);
	menu_button->connect("pressed", callable_mp(this, &FolioUI::_on_menu_pressed));
}

void FolioUI::_on_menu_pressed() {
	if (menu) {
		menu->toggle();
	}
}

void FolioUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_root"), &FolioUI::get_root);
	ClassDB::bind_method(D_METHOD("get_menu"), &FolioUI::get_menu);
	ClassDB::bind_method(D_METHOD("get_state"), &FolioUI::get_state);

	BIND_ENUM_CONSTANT(STATE_CLOSED);
	BIND_ENUM_CONSTANT(STATE_OPENING);
	BIND_ENUM_CONSTANT(STATE_OPEN);
	BIND_ENUM_CONSTANT(STATE_CLOSING);
}
