#include "celeste_ui.h"
#include "../cui/cui.h"
#include "../cui/cui_line_graph.h"
#include "celeste_controller.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

namespace godot {

CelesteUI::CelesteUI() {}
CelesteUI::~CelesteUI() {}

void CelesteUI::setup(CelesteController *p_controller, CUI *p_ui_root) {
	controller = p_controller;
	ui_root = p_ui_root;

	if (!controller || !ui_root)
		return;

	// Toggle Button (Top Right)
	Button *toggle_btn = ui_root->add_button(ui_root, "Settings", Callable(controller, "_on_ui_toggle"));
	toggle_btn->set_anchors_and_offsets_preset(Control::PRESET_TOP_RIGHT, Control::PRESET_MODE_MINSIZE, 20);

	// Main Panel (Hidden by default)
	main_panel = ui_root->add_panel(ui_root, "SettingsPanel", Control::PRESET_TOP_LEFT, Vector2(500, 450));
	main_panel->set_position(Vector2(main_panel->get_position().x, 50));
	main_panel->set_visible(false);

	TabContainer *tabs = ui_root->add_tab_container(main_panel, "Tabs");

	// Tab 1: Controller
	VBoxContainer *controller_tab = ui_root->add_vbox(tabs, "Controller");
	controller_tab->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 15);

	ui_root->add_label(controller_tab, "Movement & Physics");

	_add_variable_slider(controller_tab, "Max Speed", "max_speed", 1.0f, 50.0f, 0.1f);
	_add_variable_slider(controller_tab, "Acceleration", "acceleration", 1.0f, 200.0f, 1.0f);
	_add_variable_slider(controller_tab, "Sprint Multiplier", "sprint_multiplier", 1.0f, 3.0f, 0.1f);
	_add_variable_slider(controller_tab, "Friction", "friction", 1.0f, 200.0f, 1.0f);
	_add_variable_slider(controller_tab, "Air Resistance", "air_resistance", 0.0f, 100.0f, 0.5f);
	_add_variable_slider(controller_tab, "Jump Height", "jump_height", 0.5f, 10.0f, 0.1f);
	_add_variable_slider(controller_tab, "Time to Peak", "jump_time_to_peak", 0.1f, 1.0f, 0.01f);
	_add_variable_slider(controller_tab, "Time to Descent", "jump_time_to_descent", 0.1f, 1.0f, 0.01f);
	_add_variable_slider(controller_tab, "Max Fall Speed", "max_fall_velocity", 5.0f, 100.0f, 0.5f);

	HBoxContainer *btn_box = ui_root->add_hbox(controller_tab, "ButtonBox");
	ui_root->add_button(btn_box, "Save Settings", Callable(controller, "save_settings"));
	ui_root->add_button(btn_box, "Load Settings", Callable(controller, "load_settings"));

	// Tab 2: Info
	VBoxContainer *info_tab = ui_root->add_vbox(tabs, "Info");
	ui_root->add_label(info_tab, "Celeste Controller UI v1.1");
	ui_root->add_label(info_tab, "Refactored to regular C++ class.");

	ui_root->add_label(info_tab, "Real-time Velocity:");
	velocity_graph = ui_root->add_graph(info_tab, "VelocityGraph");
	if (velocity_graph) {
		velocity_graph->set_range(0, 30);
		velocity_graph->set_max_points(120);
	}
}

void CelesteUI::_add_variable_slider(Node *p_parent, const String &p_label, const String &p_property, float p_min, float p_max, float p_step) {
	HBoxContainer *hbox = ui_root->add_hbox(p_parent, p_property + String("_box"));

	Label *label = ui_root->add_label(hbox, p_label);
	label->set_custom_minimum_size(Vector2(120, 0));

	float current_val = controller->get_ui_var(p_property);

	ui_root->add_hslider(hbox, p_min, p_max, p_step, current_val,
			Callable(controller, "_on_ui_slider_value_changed").bind(p_property),
			p_property);
}

void CelesteUI::toggle_visibility() {
	if (main_panel) {
		bool is_visible = !main_panel->is_visible();
		main_panel->set_visible(is_visible);

		Input *input = Input::get_singleton();
		if (is_visible) {
			input->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
		}
	}
}

void CelesteUI::update_graph(float p_val) {
	if (velocity_graph) {
		velocity_graph->add_value(p_val);
	}
}

bool CelesteUI::is_visible() const {
	return main_panel && main_panel->is_visible();
}

} // namespace godot
