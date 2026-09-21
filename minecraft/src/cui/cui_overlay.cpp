#include "cui_overlay.h"

#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "cui.h"
#include "cui_tween.h"

namespace godot {

CUIOverlay::CUIOverlay() {}
CUIOverlay::~CUIOverlay() {}

void CUIOverlay::_fit_parent() {
	Control *parent = Object::cast_to<Control>(get_parent());
	if (parent) {
		set_position(Vector2(0, 0));
		set_size(parent->get_size());
	}
}

void CUIOverlay::_ready() {
	set_anchors_preset(Control::PRESET_FULL_RECT);
	set_process_input(true);

	if (builder) {
		if (has_backdrop) {
			backdrop = builder->add_color_rect(this, backdrop_color);
			backdrop->set_mouse_filter(Control::MOUSE_FILTER_STOP);
		}
		center = builder->add_center_container(this);
	}

	_build_content();

	// Overlays start hidden; open() fades them in.
	set_visible(false);
	set_modulate(Color(1, 1, 1, 0));
}

void CUIOverlay::open() {
	if (state == STATE_OPEN || state == STATE_OPENING) {
		return;
	}
	state = STATE_OPENING;
	_fit_parent();
	set_visible(true);
	if (anim.is_valid()) {
		anim->kill();
	}
	anim = CUITween::fade_alpha(this, 1.0, fade_in_duration, callable_mp(this, &CUIOverlay::_on_finish_open));
}

void CUIOverlay::close() {
	if (state == STATE_CLOSED || state == STATE_CLOSING) {
		return;
	}
	state = STATE_CLOSING;
	if (anim.is_valid()) {
		anim->kill();
	}
	anim = CUITween::fade_alpha(this, 0.0, fade_out_duration, callable_mp(this, &CUIOverlay::_on_finish_close));
}

void CUIOverlay::toggle() {
	if (is_open()) {
		close();
	} else {
		open();
	}
}

void CUIOverlay::_on_finish_open() {
	state = STATE_OPEN;
}

void CUIOverlay::_on_finish_close() {
	state = STATE_CLOSED;
	set_visible(false);
}

void CUIOverlay::_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->get_keycode() == KEY_ESCAPE) {
		_on_escape();
	}
}

void CUIOverlay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open"), &CUIOverlay::open);
	ClassDB::bind_method(D_METHOD("close"), &CUIOverlay::close);
	ClassDB::bind_method(D_METHOD("toggle"), &CUIOverlay::toggle);
	ClassDB::bind_method(D_METHOD("is_open"), &CUIOverlay::is_open);
	ClassDB::bind_method(D_METHOD("get_state"), &CUIOverlay::get_state);

	BIND_ENUM_CONSTANT(STATE_CLOSED);
	BIND_ENUM_CONSTANT(STATE_OPENING);
	BIND_ENUM_CONSTANT(STATE_OPEN);
	BIND_ENUM_CONSTANT(STATE_CLOSING);
}

} // namespace godot
