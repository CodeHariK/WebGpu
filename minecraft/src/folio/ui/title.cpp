#include "title.h"

#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "../../cui/cui.h"

using namespace godot;

FolioTitle::FolioTitle() {
	backdrop_color = Color(0.0, 0.0, 0.0, 0.55);
	fade_in_duration = 0.25;
	fade_out_duration = 0.35;
}

FolioTitle::~FolioTitle() {}

void FolioTitle::_build_content() {
	if (!center || !builder) {
		return;
	}
	VBoxContainer *vbox = builder->add_vbox(center, "", 16);
	title_label = builder->add_label(vbox, title_text, "", 56, HORIZONTAL_ALIGNMENT_CENTER);
	subtitle_label = builder->add_label(vbox, subtitle_text, "", 20, HORIZONTAL_ALIGNMENT_CENTER);
	subtitle_label->set_modulate(Color(1, 1, 1, 0.7));
	play_button = builder->add_button(vbox, "Play", callable_mp(this, &FolioTitle::_on_play_pressed), "", Vector2(220, 52));
	play_button->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
}

void FolioTitle::_on_play_pressed() {
	emit_signal("started");
	close();
}

void FolioTitle::set_title_text(const String &p_t) {
	title_text = p_t;
	if (title_label) {
		title_label->set_text(p_t);
	}
}

void FolioTitle::set_subtitle_text(const String &p_t) {
	subtitle_text = p_t;
	if (subtitle_label) {
		subtitle_label->set_text(p_t);
	}
}

void FolioTitle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_title_text", "t"), &FolioTitle::set_title_text);
	ClassDB::bind_method(D_METHOD("get_title_text"), &FolioTitle::get_title_text);
	ClassDB::bind_method(D_METHOD("set_subtitle_text", "t"), &FolioTitle::set_subtitle_text);
	ClassDB::bind_method(D_METHOD("get_subtitle_text"), &FolioTitle::get_subtitle_text);
	ADD_SIGNAL(MethodInfo("started"));
}
