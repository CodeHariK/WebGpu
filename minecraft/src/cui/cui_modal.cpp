#include "cui_modal.h"

#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "cui.h"

namespace godot {

CUIModal::CUIModal() {
	has_backdrop = true;
	backdrop_color = Color(0.0, 0.0, 0.0, 0.55);
	fade_in_duration = 0.15;
	fade_out_duration = 0.2;
}

CUIModal::~CUIModal() {}

void CUIModal::_build_content() {
	if (!center || !builder) {
		return;
	}
	PanelContainer *panel = builder->add_panel_container(center);
	// Outer column: header band, then a gap, then the body.
	VBoxContainer *vbox = builder->add_vbox(panel, "", 14);
	vbox->set_custom_minimum_size(Vector2(340, 0));

	// Header band: an accent-tinted backing behind the title so the dialog
	// reads with a clear titlebar rather than a bare line of text.
	PanelContainer *header = builder->add_panel_container(vbox, "ModalHeader");
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	Ref<StyleBoxFlat> header_sb;
	header_sb.instantiate();
	header_sb->set_bg_color(Color(0.80, 0.62, 0.95, 0.16));
	header_sb->set_corner_radius(CORNER_TOP_LEFT, 10);
	header_sb->set_corner_radius(CORNER_TOP_RIGHT, 10);
	header_sb->set_corner_radius(CORNER_BOTTOM_LEFT, 0);
	header_sb->set_corner_radius(CORNER_BOTTOM_RIGHT, 0);
	header_sb->set_content_margin(SIDE_LEFT, 18);
	header_sb->set_content_margin(SIDE_RIGHT, 18);
	header_sb->set_content_margin(SIDE_TOP, 12);
	header_sb->set_content_margin(SIDE_BOTTOM, 12);
	header->add_theme_stylebox_override("panel", header_sb);

	title_label = builder->add_label(header, "", "", 22, HORIZONTAL_ALIGNMENT_CENTER);
	title_label->add_theme_color_override("font_color", Color(0.92, 0.86, 0.99, 1.0));

	// Body: message + buttons, given their own padded column beneath the band.
	VBoxContainer *body = builder->add_vbox(vbox, "ModalBody", 16);

	message_label = builder->add_label(body, "", "", 0, HORIZONTAL_ALIGNMENT_CENTER);
	message_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);

	button_row = builder->add_hbox(body, "", 12);
	button_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
}

void CUIModal::open_confirm(const String &p_title, const String &p_message,
		const String &p_confirm_label, const String &p_cancel_label,
		const Callable &p_on_confirm) {
	if (!button_row || !builder) {
		return;
	}
	pending_confirm = p_on_confirm;
	title_label->set_text(p_title);
	message_label->set_text(p_message);

	Array kids = button_row->get_children();
	for (int i = 0; i < kids.size(); i++) {
		Node *n = Object::cast_to<Node>(kids[i]);
		if (n) {
			n->queue_free();
		}
	}
	builder->add_button(button_row, p_cancel_label, callable_mp(this, &CUIModal::_cancel), "", Vector2(120, 44));
	builder->add_button(button_row, p_confirm_label, callable_mp(this, &CUIModal::_confirm), "", Vector2(120, 44));
	open();
}

void CUIModal::open_alert(const String &p_title, const String &p_message, const String &p_ok_label) {
	if (!button_row || !builder) {
		return;
	}
	pending_confirm = Callable();
	title_label->set_text(p_title);
	message_label->set_text(p_message);

	Array kids = button_row->get_children();
	for (int i = 0; i < kids.size(); i++) {
		Node *n = Object::cast_to<Node>(kids[i]);
		if (n) {
			n->queue_free();
		}
	}
	builder->add_button(button_row, p_ok_label, callable_mp(this, &CUIModal::_confirm), "", Vector2(120, 44));
	open();
}

void CUIModal::_confirm() {
	Callable cb = pending_confirm;
	pending_confirm = Callable();
	close();
	emit_signal("confirmed");
	if (cb.is_valid()) {
		cb.call();
	}
}

void CUIModal::_cancel() {
	pending_confirm = Callable();
	close();
	emit_signal("cancelled");
}

void CUIModal::_on_escape() {
	if (is_open()) {
		_cancel();
	}
}

void CUIModal::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_confirm", "title", "message", "confirm_label", "cancel_label", "on_confirm"),
			&CUIModal::open_confirm, DEFVAL("Confirm"), DEFVAL("Cancel"), DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("open_alert", "title", "message", "ok_label"),
			&CUIModal::open_alert, DEFVAL("OK"));
	ADD_SIGNAL(MethodInfo("confirmed"));
	ADD_SIGNAL(MethodInfo("cancelled"));
}

} // namespace godot
