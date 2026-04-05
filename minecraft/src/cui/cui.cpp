#include "cui.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/theme.hpp>

#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

CUI::CUI() {}
CUI::~CUI() {}

CUI *CUI::create_on_new_layer(Node *p_owner_node) {
	if (!p_owner_node) {
		return nullptr;
	}

	CanvasLayer *layer = memnew(CanvasLayer);
	p_owner_node->get_viewport()->call_deferred("add_child", layer);

	CUI *ui = memnew(CUI);
	layer->add_child(ui);
	ui->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	ui->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	return ui;
}

void CUI::_ready() {
	// Load and apply the theme
	Ref<Theme> theme = ResourceLoader::get_singleton()->load("res://scripts/ui/menu/ui_theme.tres");
	if (theme.is_valid()) {
		set_theme(theme);
		UtilityFunctions::print("CUI: Applied theme from res://scripts/ui/menu/ui_theme.tres");
	} else {
		UtilityFunctions::print("CUI: Warning, failed to load theme from res://scripts/ui/menu/ui_theme.tres");
	}
}

Panel *CUI::add_panel(Node *p_parent, const String &p_name, LayoutPreset p_preset, const Vector2 &p_min_size) {
	Panel *panel = memnew(Panel);
	panel->set_name(p_name);
	if (p_parent) {
		p_parent->add_child(panel);
	} else {
		add_child(panel);
	}
	panel->set_anchors_and_offsets_preset(p_preset);
	panel->set_custom_minimum_size(p_min_size);
	panel->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	return panel;
}

Button *CUI::add_button(Node *p_parent, const String &p_text, const Callable &p_callback) {
	Button *button = memnew(Button);
	button->set_text(p_text);
	if (p_parent) {
		p_parent->add_child(button);
	} else {
		add_child(button);
	}
	if (p_callback.is_valid()) {
		button->connect("pressed", p_callback);
	}
	return button;
}

HBoxContainer *CUI::add_hbox(Node *p_parent, const String &p_name) {
	HBoxContainer *hbox = memnew(HBoxContainer);
	hbox->set_name(p_name);
	if (p_parent) {
		p_parent->add_child(hbox);
	} else {
		add_child(hbox);
	}
	hbox->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	return hbox;
}

ScrollContainer *CUI::add_scroll(Node *p_parent, const String &p_name) {
	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_name(p_name);
	if (p_parent) {
		p_parent->add_child(scroll);
	} else {
		add_child(scroll);
	}
	scroll->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	scroll->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	return scroll;
}

VBoxContainer *CUI::add_vbox(Node *p_parent, const String &p_name) {
	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_name(p_name);
	if (p_parent) {
		p_parent->add_child(vbox);
	} else {
		add_child(vbox);
	}
	vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	return vbox;
}

Label *CUI::add_label(Node *p_parent, const String &p_text) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	if (p_parent) {
		p_parent->add_child(label);
	} else {
		add_child(label);
	}
	return label;
}

AcceptDialog *CUI::add_dialog(Node *p_parent, const String &p_name, const String &p_text) {
	AcceptDialog *dialog = memnew(AcceptDialog);
	dialog->set_name(p_name);
	dialog->set_title(p_name);
	dialog->set_text(p_text);
	if (p_parent) {
		p_parent->add_child(dialog);
	} else {
		add_child(dialog);
	}
	return dialog;
}

} // namespace godot
