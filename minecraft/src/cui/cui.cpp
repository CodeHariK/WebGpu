#include "cui.h"
#include "cui_line_graph.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/theme.hpp>

#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

CUI::CUI() {}
CUI::~CUI() {}

void CUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_slider_value_changed", "p_value", "p_label"), &CUI::_on_slider_value_changed);
}

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

Control *CUI::get_element(const String &p_name) const {
	if (elements.has(p_name)) {
		return elements[p_name];
	}
	return nullptr;
}

void CUI::set_text(const String &p_name, const String &p_text) {
	Label *label = Object::cast_to<Label>(get_element(p_name));
	if (label) {
		label->set_text(p_text);
	}
}

void CUI::set_value(const String &p_name, float p_value) {
	HSlider *slider = Object::cast_to<HSlider>(get_element(p_name));
	if (slider) {
		slider->set_value(p_value);
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

	// Professional centering logic
	panel->set("layout_mode", 1); // ANCHORS
	panel->set_anchors_and_offsets_preset(p_preset);
	
	// Set Grow Direction to BOTH so it expands from the anchor point
	panel->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
	panel->set_v_grow_direction(Control::GROW_DIRECTION_BOTH);

	if (p_min_size != Vector2(0, 0)) {
		panel->set_size(p_min_size);
		if (p_preset == PRESET_CENTER) {
			float offset_x = p_min_size.x * 0.5f;
			float offset_y = p_min_size.y * 0.5f;
			panel->set_offset(Side::SIDE_LEFT, -offset_x);
			panel->set_offset(Side::SIDE_TOP, -offset_y);
			panel->set_offset(Side::SIDE_RIGHT, offset_x);
			panel->set_offset(Side::SIDE_BOTTOM, offset_y);
		}
	} else if (p_preset == PRESET_CENTER) {
		// If no size provided, ensure it starts at the center point with 0 offsets
		panel->set_offset(Side::SIDE_LEFT, 0);
		panel->set_offset(Side::SIDE_TOP, 0);
		panel->set_offset(Side::SIDE_RIGHT, 0);
		panel->set_offset(Side::SIDE_BOTTOM, 0);
	}

	panel->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	elements[p_name] = panel;
	return panel;
}

PanelContainer *CUI::add_panel_container(Node *p_parent, const String &p_name, LayoutPreset p_preset) {
	PanelContainer *panel = memnew(PanelContainer);
	panel->set_name(p_name);
	if (p_parent) {
		p_parent->add_child(panel);
	} else {
		add_child(panel);
	}

	// Centering logic for auto-sizing container
	panel->set("layout_mode", 1); // ANCHORS
	panel->set_anchors_preset(p_preset);
	panel->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
	panel->set_v_grow_direction(Control::GROW_DIRECTION_BOTH);

	if (p_preset == PRESET_CENTER) {
		panel->set_offset(Side::SIDE_LEFT, 0);
		panel->set_offset(Side::SIDE_TOP, 0);
		panel->set_offset(Side::SIDE_RIGHT, 0);
		panel->set_offset(Side::SIDE_BOTTOM, 0);
	}

	panel->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	elements[p_name] = panel;
	return panel;
}

Button *CUI::add_button(Node *p_parent, const String &p_text, const Callable &p_callback, const String &p_name) {
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
	button->set_focus_mode(Control::FOCUS_NONE);
	if (!p_name.is_empty()) {
		button->set_name(p_name);
		elements[p_name] = button;
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
	elements[p_name] = hbox;
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
	elements[p_name] = vbox;
	return vbox;
}

Label *CUI::add_label(Node *p_parent, const String &p_text, const String &p_name) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	if (p_parent) {
		p_parent->add_child(label);
	} else {
		add_child(label);
	}
	if (!p_name.is_empty()) {
		label->set_name(p_name);
		elements[p_name] = label;
	}
	return label;
}

ProgressBar *CUI::add_progress_bar(Node *p_parent, const String &p_name) {
	ProgressBar *bar = memnew(ProgressBar);
	if (p_parent) {
		p_parent->add_child(bar);
	} else {
		add_child(bar);
	}
	if (!p_name.is_empty()) {
		bar->set_name(p_name);
		elements[p_name] = bar;
	}
	return bar;
}

LineEdit *CUI::add_line_edit(Node *p_parent, const String &p_placeholder, const String &p_name) {
	LineEdit *edit = memnew(LineEdit);
	edit->set_placeholder(p_placeholder);
	if (p_parent) {
		p_parent->add_child(edit);
	} else {
		add_child(edit);
	}
	if (!p_name.is_empty()) {
		edit->set_name(p_name);
		elements[p_name] = edit;
	}
	return edit;
}

OptionButton *CUI::add_option_button(Node *p_parent, const String &p_name) {
	OptionButton *opt = memnew(OptionButton);
	if (p_parent) {
		p_parent->add_child(opt);
	} else {
		add_child(opt);
	}
	if (!p_name.is_empty()) {
		opt->set_name(p_name);
		elements[p_name] = opt;
	}
	return opt;
}

SpinBox *CUI::add_spin_box(Node *p_parent, double p_min, double p_max, double p_step, double p_value, const String &p_name) {
	SpinBox *spin = memnew(SpinBox);
	spin->set_min(p_min);
	spin->set_max(p_max);
	spin->set_step(p_step);
	spin->set_value(p_value);
	if (p_parent) {
		p_parent->add_child(spin);
	} else {
		add_child(spin);
	}
	if (!p_name.is_empty()) {
		spin->set_name(p_name);
		elements[p_name] = spin;
	}
	return spin;
}

HSlider *CUI::add_hslider(Node *p_parent, float p_min, float p_max, float p_step, float p_value, const Callable &p_callback, const String &p_name) {
	HSlider *slider = memnew(HSlider);
	slider->set_min(p_min);
	slider->set_max(p_max);
	slider->set_step(p_step);
	slider->set_value(p_value);
	slider->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	if (p_parent) {
		p_parent->add_child(slider);
	} else {
		add_child(slider);
	}
	if (p_callback.is_valid()) {
		slider->connect("value_changed", p_callback);
	}

	if (!p_name.is_empty()) {
		slider->set_name(p_name);
		elements[p_name] = slider;
	}

	// Automated value label
	Label *val_label = add_label(p_parent, String::num_real(p_value));
	val_label->set_custom_minimum_size(Vector2(50, 0));
	slider->connect("value_changed", Callable(this, "_on_slider_value_changed").bind(val_label));

	if (!p_name.is_empty()) {
		val_label->set_name(p_name + String("_val"));
		elements[p_name + String("_val")] = val_label;
	}

	return slider;
}

void CUI::_on_slider_value_changed(double p_value, Label *p_label) {
	if (p_label) {
		p_label->set_text(String::num_real(p_value));
	}
}

CUILineGraph *CUI::add_graph(Node *p_parent, const String &p_name) {
	CUILineGraph *graph = memnew(CUILineGraph);
	graph->set_name(p_name);
	if (p_parent) {
		p_parent->add_child(graph);
	}
	return graph;
}

TabContainer *CUI::add_tab_container(Node *p_parent, const String &p_name) {
	TabContainer *tabs = memnew(TabContainer);
	tabs->set_name(p_name);
	if (p_parent) {
		p_parent->add_child(tabs);
	} else {
		add_child(tabs);
	}
	tabs->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	return tabs;
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
