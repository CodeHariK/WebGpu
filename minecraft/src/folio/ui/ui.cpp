#include "ui.h"

#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "../game.h"
#include "../audio.h"
#include "menu.h"
#include "theme.h"
#include "title.h"
#include "../../cui/cui.h"
#include "../../cui/cui_modal.h"
#include "../../cui/cui_toast.h"

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
	root->set_theme(FolioTheme::build()); // one shared look for every HUD widget
	add_child(root);

	// Shared widget factory used by every screen for view construction (labels,
	// buttons, containers). Screens keep the state/behaviour; CUI builds the nodes.
	builder = memnew(CUI);
	builder->set_name("Builder");
	builder->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	root->add_child(builder);

	_build_mute_button();
	_build_menu_button();

	// Menu overlay lives under the root; toggled by the HUD button or Esc.
	menu = memnew(FolioMenu);
	menu->set_name("Menu");
	menu->set_builder(builder);
	root->add_child(menu);

	// Transient toast stack.
	notifications = memnew(CUIToast);
	notifications->set_name("Notifications");
	notifications->set_builder(builder);
	root->add_child(notifications);

	// Title / start screen, shown just after boot once layout has a real size.
	title = memnew(FolioTitle);
	title->set_name("Title");
	title->set_builder(builder);
	root->add_child(title);
	title->connect("started", callable_mp(this, &FolioUI::_on_started));
	set_hud_visible(false); // hide the in-game HUD until the player starts
	// Confirm/alert dialog, on top of everything.
	modal = memnew(CUIModal);
	modal->set_name("Modal");
	modal->set_builder(builder);
	root->add_child(modal);

	Ref<SceneTreeTimer> t = get_tree()->create_timer(0.1);
	t->connect("timeout", callable_mp((CUIOverlay *)title, &CUIOverlay::open));
}

void FolioUI::set_hud_visible(bool p_v) {
	if (mute_button) {
		mute_button->set_visible(p_v);
	}
	if (menu_button) {
		menu_button->set_visible(p_v);
	}
}

void FolioUI::_on_started() {
	set_hud_visible(true);
	if (notifications) {
		notifications->notify("Welcome to the island");
	}
}

void FolioUI::_build_mute_button() {
	mute_button = memnew(Button);
	mute_button->set_name("MuteButton");
	mute_button->set_anchors_preset(Control::PRESET_BOTTOM_LEFT);
	mute_button->set_offset(Side::SIDE_LEFT, 20.0);
	mute_button->set_offset(Side::SIDE_TOP, -64.0);
	mute_button->set_offset(Side::SIDE_RIGHT, 160.0);
	mute_button->set_offset(Side::SIDE_BOTTOM, -20.0);
	mute_button->set_focus_mode(Control::FOCUS_NONE); // styling comes from the shared theme
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
	if (notifications) {
		notifications->notify(p_active ? "Sound muted" : "Sound on");
	}
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
	ClassDB::bind_method(D_METHOD("get_notifications"), &FolioUI::get_notifications);
	ClassDB::bind_method(D_METHOD("get_title"), &FolioUI::get_title);
	ClassDB::bind_method(D_METHOD("get_modal"), &FolioUI::get_modal);
	ClassDB::bind_method(D_METHOD("set_hud_visible", "v"), &FolioUI::set_hud_visible);
	ClassDB::bind_method(D_METHOD("get_state"), &FolioUI::get_state);

	BIND_ENUM_CONSTANT(STATE_CLOSED);
	BIND_ENUM_CONSTANT(STATE_OPENING);
	BIND_ENUM_CONSTANT(STATE_OPEN);
	BIND_ENUM_CONSTANT(STATE_CLOSING);
}
