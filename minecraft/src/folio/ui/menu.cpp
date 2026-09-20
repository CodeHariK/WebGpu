#include "menu.h"

#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "../audio.h"
#include "../game.h"
#include "../quality.h"

using namespace godot;

FolioMenu::FolioMenu() {}
FolioMenu::~FolioMenu() {}

static void fit_parent(Control *c) {
	Control *parent = Object::cast_to<Control>(c->get_parent());
	if (parent) {
		c->set_position(Vector2(0, 0));
		c->set_size(parent->get_size());
	}
}

void FolioMenu::_ready() {
	set_anchors_preset(Control::PRESET_FULL_RECT);
	set_process_input(true);
	_build();
	// Hidden until opened.
	set_visible(false);
	set_modulate(Color(1, 1, 1, 0));
}

static Button *make_row(const String &p_text) {
	Button *b = memnew(Button);
	b->set_text(p_text);
	b->set_focus_mode(Control::FOCUS_NONE);
	b->set_custom_minimum_size(Vector2(240, 44));
	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	sb->set_bg_color(Color(0.16, 0.18, 0.26, 1.0));
	sb->set_corner_radius_all(12);
	sb->set_content_margin_all(8);
	b->add_theme_stylebox_override("normal", sb);
	Ref<StyleBoxFlat> hov = sb->duplicate();
	hov->set_bg_color(Color(0.24, 0.27, 0.38, 1.0));
	b->add_theme_stylebox_override("hover", hov);
	b->add_theme_stylebox_override("pressed", hov);
	return b;
}

void FolioMenu::_build() {
	// Dim backdrop that also swallows clicks behind the panel.
	backdrop = memnew(ColorRect);
	backdrop->set_name("Backdrop");
	backdrop->set_anchors_preset(Control::PRESET_FULL_RECT);
	((ColorRect *)backdrop)->set_color(Color(0.0, 0.0, 0.0, 0.45));
	backdrop->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	add_child(backdrop);

	// Centre the panel with a full-rect CenterContainer.
	CenterContainer *center = memnew(CenterContainer);
	center->set_name("Center");
	center->set_anchors_preset(Control::PRESET_FULL_RECT);
	center->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	add_child(center);

	PanelContainer *panel = memnew(PanelContainer);
	panel->set_name("Panel");
	Ref<StyleBoxFlat> psb;
	psb.instantiate();
	psb->set_bg_color(Color(0.10, 0.12, 0.18, 0.96));
	psb->set_corner_radius_all(20);
	psb->set_content_margin_all(24);
	panel->add_theme_stylebox_override("panel", psb);
	center->add_child(panel);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->add_theme_constant_override("separation", 12);
	panel->add_child(vbox);

	Label *title = memnew(Label);
	title->set_text("Menu");
	title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	title->add_theme_font_size_override("font_size", 28);
	vbox->add_child(title);

	sound_button = make_row("Sound: On");
	sound_button->connect("pressed", callable_mp(this, &FolioMenu::_on_sound_pressed));
	vbox->add_child(sound_button);

	quality_button = make_row("Quality: High");
	quality_button->connect("pressed", callable_mp(this, &FolioMenu::_on_quality_pressed));
	vbox->add_child(quality_button);

	Button *resume = make_row("Resume");
	resume->connect("pressed", callable_mp(this, &FolioMenu::close));
	vbox->add_child(resume);

	// Reflect current system state + subscribe.
	FolioGame *game = FolioGame::get_singleton();
	FolioAudio *audio = game ? game->get_audio() : nullptr;
	if (audio) {
		audio->connect("mute_changed", callable_mp(this, &FolioMenu::_on_mute_changed));
		_refresh_sound(audio->is_muted());
	}
	_refresh_quality();
}

void FolioMenu::_refresh_sound(bool p_muted) {
	if (sound_button) {
		sound_button->set_text(p_muted ? "Sound: Off" : "Sound: On");
	}
}

void FolioMenu::_refresh_quality() {
	FolioGame *game = FolioGame::get_singleton();
	FolioQuality *q = game ? game->get_quality() : nullptr;
	if (quality_button && q) {
		quality_button->set_text(q->get_level() == 0 ? "Quality: High" : "Quality: Low");
	}
}

void FolioMenu::_on_sound_pressed() {
	FolioGame *game = FolioGame::get_singleton();
	if (game && game->get_audio()) {
		game->get_audio()->toggle_mute();
	}
}

void FolioMenu::_on_quality_pressed() {
	FolioGame *game = FolioGame::get_singleton();
	FolioQuality *q = game ? game->get_quality() : nullptr;
	if (q) {
		q->change_level(q->get_level() == 0 ? 1 : 0);
		_refresh_quality();
	}
}

void FolioMenu::_on_mute_changed(bool p_active) {
	_refresh_sound(p_active);
}

void FolioMenu::open() {
	if (state == STATE_OPEN || state == STATE_OPENING) {
		return;
	}
	state = STATE_OPENING;
	fit_parent(this);
	set_visible(true);
	Ref<Tween> tw = create_tween();
	tw->tween_property(this, "modulate:a", 1.0, 0.18);
	tw->tween_callback(callable_mp(this, &FolioMenu::_on_finish_open));
}

void FolioMenu::close() {
	if (state == STATE_CLOSED || state == STATE_CLOSING) {
		return;
	}
	state = STATE_CLOSING;
	Ref<Tween> tw = create_tween();
	tw->tween_property(this, "modulate:a", 0.0, 0.18);
	tw->tween_callback(callable_mp(this, &FolioMenu::_on_finish_close));
}

void FolioMenu::_on_finish_open() {
	state = STATE_OPEN;
}

void FolioMenu::_on_finish_close() {
	state = STATE_CLOSED;
	set_visible(false);
}

void FolioMenu::toggle() {
	if (is_open()) {
		close();
	} else {
		open();
	}
}

void FolioMenu::_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->get_keycode() == KEY_ESCAPE) {
		toggle();
	}
}

void FolioMenu::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open"), &FolioMenu::open);
	ClassDB::bind_method(D_METHOD("close"), &FolioMenu::close);
	ClassDB::bind_method(D_METHOD("toggle"), &FolioMenu::toggle);
	ClassDB::bind_method(D_METHOD("is_open"), &FolioMenu::is_open);
	ClassDB::bind_method(D_METHOD("get_state"), &FolioMenu::get_state);

	BIND_ENUM_CONSTANT(STATE_CLOSED);
	BIND_ENUM_CONSTANT(STATE_OPENING);
	BIND_ENUM_CONSTANT(STATE_OPEN);
	BIND_ENUM_CONSTANT(STATE_CLOSING);
}
