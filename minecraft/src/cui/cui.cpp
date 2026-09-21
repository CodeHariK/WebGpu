#include "cui.h"
#include "cui_line_graph.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/theme.hpp>

#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/range.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static const char *THEME_PATH = "res://scripts/ui/menu/ui_theme.tres";

CUI::CUI() {}
CUI::~CUI() {}

void CUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_element", "name"), &CUI::get_element);
	ClassDB::bind_method(D_METHOD("set_text", "name", "text"), &CUI::set_text);
	ClassDB::bind_method(D_METHOD("set_value", "name", "value"), &CUI::set_value);
}

CUI *CUI::create_on_new_layer(Node *p_owner_node) {
	if (!p_owner_node || !p_owner_node->get_viewport()) {
		return nullptr;
	}

	CanvasLayer *layer = memnew(CanvasLayer);
	layer->set_name("CUILayer");

	CUI *ui = memnew(CUI);
	layer->add_child(ui);
	ui->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	ui->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	// Defer adding the fully-built layer so this is safe to call mid-notification
	// (e.g. from _ready/_enter_tree); the CUI is already parented + presented.
	p_owner_node->get_viewport()->call_deferred("add_child", layer);

	return ui;
}

void CUI::_ready() {
	Ref<Theme> theme = ResourceLoader::get_singleton()->load(THEME_PATH);
	if (theme.is_valid()) {
		set_theme(theme);
	} else {
		UtilityFunctions::push_error("CUI: failed to load theme from ", THEME_PATH);
	}
}

// --- internal helpers -------------------------------------------------------

void CUI::_reparent(Node *p_parent, Control *p_child) {
	if (p_parent) {
		p_parent->add_child(p_child);
	} else {
		add_child(p_child);
	}
}

void CUI::_register(const String &p_name, Control *p_control) {
	if (p_name.is_empty() || !p_control) {
		return;
	}
	if (element_ids.has(p_name)) {
		Object *existing = UtilityFunctions::instance_from_id(element_ids[p_name]);
		if (existing && existing != p_control && !collision_warned.has(p_name)) {
			collision_warned.insert(p_name);
			UtilityFunctions::push_warning("CUI: element name '", p_name,
					"' registered more than once while both are live; get_element() will only return the latest. "
					"Give list/row elements unique names. (Further collisions on this name are silenced.)");
		}
	}
	element_ids[p_name] = p_control->get_instance_id();
}

// --- registry accessors -----------------------------------------------------

Control *CUI::get_element(const String &p_name) const {
	HashMap<String, uint64_t>::ConstIterator it = element_ids.find(p_name);
	if (it == element_ids.end()) {
		return nullptr;
	}
	Object *obj = UtilityFunctions::instance_from_id(it->value);
	Control *ctrl = Object::cast_to<Control>(obj);
	if (!ctrl) {
		// Element was freed since registration; prune the stale id.
		const_cast<CUI *>(this)->element_ids.erase(p_name);
	}
	return ctrl;
}

void CUI::set_text(
		const String &p_name,
		const String &p_text
) {
	Label *label = Object::cast_to<Label>(get_element(p_name));
	if (label) {
		label->set_text(p_text);
	}
}

void CUI::set_value(
		const String &p_name,
		float p_value
) {
	Range *range = Object::cast_to<Range>(get_element(p_name));
	if (range) {
		range->set_value(p_value); // HSlider / ProgressBar / SpinBox all derive Range
	}
}

// --- element builders -------------------------------------------------------

Panel *CUI::add_panel(
		Node *p_parent,
		const String &p_name,
		LayoutPreset p_preset,
		const Vector2 &p_min_size
) {
	Panel *panel = memnew(Panel);
	panel->set_name(p_name);
	_reparent(p_parent, panel);
	panel->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	_register(p_name, panel);

	// Only anchor when NOT inside a Container (containers own their layout).
	if (!Object::cast_to<Container>(p_parent)) {
		panel->set_anchors_and_offsets_preset(p_preset);
		panel->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
		panel->set_v_grow_direction(Control::GROW_DIRECTION_BOTH);

		if (p_min_size != Vector2(0, 0)) {
			panel->set_size(p_min_size);
			if (p_preset == PRESET_CENTER) {
				const float ox = p_min_size.x * 0.5f;
				const float oy = p_min_size.y * 0.5f;
				panel->set_offset(Side::SIDE_LEFT, -ox);
				panel->set_offset(Side::SIDE_TOP, -oy);
				panel->set_offset(Side::SIDE_RIGHT, ox);
				panel->set_offset(Side::SIDE_BOTTOM, oy);
			}
		}
	} else {
		panel->set_custom_minimum_size(p_min_size);
	}

	return panel;
}

PanelContainer *CUI::add_panel_container(
		Node *p_parent,
		const String &p_name,
		LayoutPreset p_preset
) {
	PanelContainer *panel = memnew(PanelContainer);
	if (!p_name.is_empty()) {
		panel->set_name(p_name);
	}
	_reparent(p_parent, panel);
	panel->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	_register(p_name, panel);

	if (!Object::cast_to<Container>(p_parent)) {
		panel->set_anchors_preset(p_preset);
		panel->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
		panel->set_v_grow_direction(Control::GROW_DIRECTION_BOTH);
		if (p_preset == PRESET_CENTER) {
			panel->set_offset(Side::SIDE_LEFT, 0);
			panel->set_offset(Side::SIDE_TOP, 0);
			panel->set_offset(Side::SIDE_RIGHT, 0);
			panel->set_offset(Side::SIDE_BOTTOM, 0);
		}
	}

	return panel;
}

Button *CUI::add_button(
		Node *p_parent,
		const String &p_text,
		const Callable &p_callback,
		const String &p_name,
		const Vector2 &p_min_size
) {
	Button *button = memnew(Button);
	button->set_text(p_text);
	_reparent(p_parent, button);
	if (p_callback.is_valid()) {
		button->connect("pressed", p_callback);
	}
	button->set_focus_mode(Control::FOCUS_NONE);
	if (p_min_size != Vector2()) {
		button->set_custom_minimum_size(p_min_size);
	}
	if (!p_name.is_empty()) {
		button->set_name(p_name);
		_register(p_name, button);
	}
	return button;
}

HBoxContainer *CUI::add_hbox(
		Node *p_parent,
		const String &p_name,
		int p_separation,
		LayoutPreset p_preset
) {
	HBoxContainer *hbox = memnew(HBoxContainer);
	if (!p_name.is_empty()) {
		hbox->set_name(p_name);
	}
	_reparent(p_parent, hbox);
	hbox->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	_register(p_name, hbox);

	if (p_separation > 0) {
		hbox->add_theme_constant_override("separation", p_separation);
	}
	if (!Object::cast_to<Container>(p_parent)) {
		hbox->set_anchors_and_offsets_preset(p_preset);
	}
	return hbox;
}

ScrollContainer *CUI::add_scroll(
		Node *p_parent,
		const String &p_name
) {
	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_name(p_name);
	_reparent(p_parent, scroll);
	scroll->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	scroll->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	_register(p_name, scroll);
	return scroll;
}

VBoxContainer *CUI::add_vbox(
		Node *p_parent,
		const String &p_name,
		int p_separation,
		LayoutPreset p_preset
) {
	VBoxContainer *vbox = memnew(VBoxContainer);
	if (!p_name.is_empty()) {
		vbox->set_name(p_name);
	}
	_reparent(p_parent, vbox);

	if (p_separation > 0) {
		vbox->add_theme_constant_override("separation", p_separation);
	}
	if (!Object::cast_to<Container>(p_parent)) {
		vbox->set_anchors_and_offsets_preset(p_preset);
	}
	_register(p_name, vbox);
	return vbox;
}

Label *CUI::add_label(
		Node *p_parent,
		const String &p_text,
		const String &p_name,
		int p_font_size,
		int p_align
) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->set_horizontal_alignment((HorizontalAlignment)p_align);
	if (p_font_size > 0) {
		label->add_theme_font_size_override("font_size", p_font_size);
	}
	_reparent(p_parent, label);
	if (!p_name.is_empty()) {
		label->set_name(p_name);
		_register(p_name, label);
	}
	return label;
}

Label *CUI::add_header(
		Node *p_parent,
		const String &p_text,
		const String &p_name
) {
	Label *label = add_label(p_parent, p_text.to_upper(), p_name, 13, 0);
	label->add_theme_color_override("font_color", Color(0.80, 0.62, 0.95, 1.0));
	// A touch of breathing room above each section.
	label->add_theme_constant_override("line_spacing", 2);
	label->set_custom_minimum_size(Vector2(0, 26));
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_BOTTOM);
	return label;
}

ProgressBar *CUI::add_progress_bar(
		Node *p_parent,
		const String &p_name
) {
	ProgressBar *bar = memnew(ProgressBar);
	_reparent(p_parent, bar);
	if (!p_name.is_empty()) {
		bar->set_name(p_name);
		_register(p_name, bar);
	}
	return bar;
}

LineEdit *CUI::add_line_edit(
		Node *p_parent,
		const String &p_placeholder,
		const String &p_name
) {
	LineEdit *edit = memnew(LineEdit);
	edit->set_placeholder(p_placeholder);
	_reparent(p_parent, edit);
	if (!p_name.is_empty()) {
		edit->set_name(p_name);
		_register(p_name, edit);
	}
	return edit;
}

OptionButton *CUI::add_option_button(
		Node *p_parent,
		const String &p_name
) {
	OptionButton *opt = memnew(OptionButton);
	_reparent(p_parent, opt);
	if (!p_name.is_empty()) {
		opt->set_name(p_name);
		_register(p_name, opt);
	}
	return opt;
}

SpinBox *CUI::add_spin_box(
		Node *p_parent,
		double p_min,
		double p_max,
		double p_step,
		double p_value,
		const String &p_name
) {
	SpinBox *spin = memnew(SpinBox);
	spin->set_min(p_min);
	spin->set_max(p_max);
	spin->set_step(p_step);
	spin->set_value(p_value);
	_reparent(p_parent, spin);
	if (!p_name.is_empty()) {
		spin->set_name(p_name);
		_register(p_name, spin);
	}
	return spin;
}

HSlider *CUI::add_hslider(
		Node *p_parent,
		float p_min,
		float p_max,
		float p_step,
		float p_value,
		const Callable &p_callback,
		const String &p_name
) {
	HSlider *slider = memnew(HSlider);
	slider->set_min(p_min);
	slider->set_max(p_max);
	slider->set_step(p_step);
	slider->set_value(p_value);
	slider->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_reparent(p_parent, slider);
	if (p_callback.is_valid()) {
		slider->connect("value_changed", p_callback);
	}
	if (!p_name.is_empty()) {
		slider->set_name(p_name);
		_register(p_name, slider);
	}

	// Automatic value label kept in sync via a typed callback (no string names).
	Label *val_label = add_label(p_parent, String::num_real(p_value));
	val_label->set_custom_minimum_size(Vector2(50, 0));
	slider->connect("value_changed", callable_mp(this, &CUI::_on_slider_value_changed).bind(val_label));

	if (!p_name.is_empty()) {
		val_label->set_name(p_name + String("_val"));
		_register(p_name + String("_val"), val_label);
	}

	return slider;
}

void CUI::_on_slider_value_changed(
		double p_value,
		Label *p_label
) {
	if (p_label) {
		p_label->set_text(String::num_real(p_value));
	}
}

CUILineGraph *CUI::add_graph(
		Node *p_parent,
		const String &p_name
) {
	CUILineGraph *graph = memnew(CUILineGraph);
	graph->set_name(p_name);
	_reparent(p_parent, graph);
	_register(p_name, graph);
	return graph;
}

TabContainer *CUI::add_tab_container(
		Node *p_parent,
		const String &p_name
) {
	TabContainer *tabs = memnew(TabContainer);
	tabs->set_name(p_name);
	_reparent(p_parent, tabs);
	tabs->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	_register(p_name, tabs);
	return tabs;
}

AcceptDialog *CUI::add_dialog(
		Node *p_parent,
		const String &p_name,
		const String &p_text
) {
	AcceptDialog *dialog = memnew(AcceptDialog);
	dialog->set_name(p_name);
	dialog->set_title(p_name);
	dialog->set_text(p_text);

	// Themed frame so dialogs match the rest of the HUD (dark rounded card).
	Ref<StyleBoxFlat> frame;
	frame.instantiate();
	frame->set_bg_color(Color(0.10, 0.12, 0.18, 0.98));
	frame->set_border_width_all(1);
	frame->set_border_color(Color(1, 1, 1, 0.08));
	frame->set_corner_radius_all(16);
	frame->set_content_margin_all(6);
	dialog->add_theme_stylebox_override("embedded_border", frame);
	dialog->add_theme_stylebox_override("embedded_unfocused_border", frame);

	Ref<StyleBoxFlat> body;
	body.instantiate();
	body->set_bg_color(Color(0, 0, 0, 0)); // frame already paints the background
	body->set_content_margin(Side::SIDE_TOP, 16);
	body->set_content_margin(Side::SIDE_LEFT, 20);
	body->set_content_margin(Side::SIDE_RIGHT, 20);
	body->set_content_margin(Side::SIDE_BOTTOM, 12);
	dialog->add_theme_stylebox_override("panel", body);

	dialog->add_theme_color_override("title_color", Color(0.80, 0.62, 0.95, 1.0));
	dialog->add_theme_constant_override("title_height", 40);
	dialog->add_theme_constant_override("buttons_separation", 12);
	// AcceptDialog is a Window, not a Control -> attach directly (not via _reparent).
	if (p_parent) {
		p_parent->add_child(dialog);
	} else {
		add_child(dialog);
	}
	return dialog;
}

ColorRect *CUI::add_color_rect(
		Node *p_parent,
		const Color &p_color,
		const String &p_name
) {
	ColorRect *rect = memnew(ColorRect);
	rect->set_color(p_color);
	_reparent(p_parent, rect);
	// Fill the parent when free-standing (a dim/backdrop layer).
	if (!Object::cast_to<Container>(p_parent)) {
		rect->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	}
	if (!p_name.is_empty()) {
		rect->set_name(p_name);
		_register(p_name, rect);
	}
	return rect;
}

CenterContainer *CUI::add_center_container(
		Node *p_parent,
		const String &p_name
) {
	CenterContainer *center = memnew(CenterContainer);
	if (!p_name.is_empty()) {
		center->set_name(p_name);
	}
	_reparent(p_parent, center);
	if (!Object::cast_to<Container>(p_parent)) {
		center->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	}
	center->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	_register(p_name, center);
	return center;
}

} // namespace godot
