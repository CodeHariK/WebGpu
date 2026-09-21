#include "menu.h"

#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "../audio.h"
#include "../game.h"
#include "../quality.h"
#include "ui.h"
#include "title.h"
#include "../../cui/cui.h"
#include "../../cui/cui_modal.h"

using namespace godot;

FolioMenu::FolioMenu() {
	backdrop_color = Color(0.0, 0.0, 0.0, 0.45);
	fade_in_duration = 0.18;
	fade_out_duration = 0.18;
}

FolioMenu::~FolioMenu() {}

void FolioMenu::_build_content() {
	if (!center || !builder) {
		return;
	}
	PanelContainer *panel = builder->add_panel_container(center);
	VBoxContainer *vbox = builder->add_vbox(panel, "", 12);

	builder->add_label(vbox, "Menu", "", 28, HORIZONTAL_ALIGNMENT_CENTER);
	const Vector2 row(240, 44);
	sound_button = builder->add_button(vbox, "Sound: On", callable_mp(this, &FolioMenu::_on_sound_pressed), "", row);
	quality_button = builder->add_button(vbox, "Quality: High", callable_mp(this, &FolioMenu::_on_quality_pressed), "", row);
	builder->add_button(vbox, "Resume", callable_mp((CUIOverlay *)this, &CUIOverlay::close), "", row);
	builder->add_button(vbox, "Quit to title", callable_mp(this, &FolioMenu::_on_quit_pressed), "", row);

	// Wire to game systems (this is the game-specific part).
	FolioGame *game = FolioGame::get_singleton();
	FolioAudio *audio = game ? game->get_audio() : nullptr;
	if (audio) {
		audio->connect("mute_changed", callable_mp(this, &FolioMenu::_on_mute_changed));
		_refresh_sound(audio->is_muted());
	}
	_refresh_quality();
}

void FolioMenu::_on_escape() {
	toggle();
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

void FolioMenu::_on_quit_pressed() {
	FolioUI *ui = FolioUI::get_singleton();
	if (ui && ui->get_modal()) {
		ui->get_modal()->open_confirm("Quit to title?", "You'll return to the start screen.",
				"Quit", "Cancel", callable_mp(this, &FolioMenu::_do_quit));
	}
}

void FolioMenu::_do_quit() {
	close();
	FolioUI *ui = FolioUI::get_singleton();
	if (ui) {
		ui->set_hud_visible(false);
		if (ui->get_title()) {
			ui->get_title()->open();
		}
	}
}

void FolioMenu::_bind_methods() {}
