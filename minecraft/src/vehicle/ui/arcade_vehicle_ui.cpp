#include "arcade_vehicle_ui.h"
#include "../../cui/cui.h"
#include "../../cui/cui_line_graph.h"
#include "../arcade_vehicle.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

namespace godot {

ArcadeVehicleUI::ArcadeVehicleUI() {}
ArcadeVehicleUI::~ArcadeVehicleUI() {}

void ArcadeVehicleUI::setup(ArcadeVehicle *p_vehicle, CUI *p_ui_root) {
	vehicle = p_vehicle;
	ui_root = p_ui_root;

	if (!vehicle || !ui_root)
		return;

	// Toggle Button (Top Right, shifted left slightly to avoid overlapping Celeste Settings if both exist)
	Button *toggle_btn = ui_root->add_button(ui_root, "Vehicle Settings", Callable(vehicle, "_on_ui_toggle"));
	toggle_btn->set_anchors_and_offsets_preset(Control::PRESET_TOP_RIGHT, Control::PRESET_MODE_MINSIZE, 20);
	// Let's shift it left so it sits to the left of Celeste Settings (which offset 20)
	toggle_btn->set_position(Vector2(toggle_btn->get_position().x - 120, toggle_btn->get_position().y));

	// Main Panel (Hidden by default)
	main_panel = ui_root->add_panel(ui_root, "VehicleSettingsPanel", Control::PRESET_TOP_LEFT, Vector2(500, 480));
	main_panel->set_position(Vector2(main_panel->get_position().x, 50));
	main_panel->set_visible(false);

	TabContainer *tabs = ui_root->add_tab_container(main_panel, "Tabs");

	// Tab 1: Driving & Grip
	VBoxContainer *physics_tab = ui_root->add_vbox(tabs, "Driving & Grip");
	physics_tab->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 15);

	ui_root->add_label(physics_tab, "Vehicle Driving & Friction Settings");

	_add_variable_slider(physics_tab, "Max Speed", "max_speed", 5.0f, 100.0f, 0.5f);
	_add_variable_slider(physics_tab, "Max Accel Force", "max_accel_force", 1000.0f, 25000.0f, 100.0f);
	_add_variable_slider(physics_tab, "Brake Decel", "brake_decel", 1000.0f, 30000.0f, 100.0f);
	_add_variable_slider(physics_tab, "Arcade Assist", "arcade_assist", 0.0f, 10.0f, 0.1f);
	_add_variable_slider(physics_tab, "Max Steer Angle", "max_steer_angle_deg", 5.0f, 60.0f, 0.5f);
	_add_variable_slider(physics_tab, "Base Grip", "base_grip", 0.0f, 2.0f, 0.05f);
	_add_variable_slider(physics_tab, "Drift Grip", "drift_grip", 0.0f, 2.0f, 0.05f);
	_add_variable_slider(physics_tab, "Downforce", "downforce", 0.0f, 10000.0f, 50.0f);
	_add_variable_slider(physics_tab, "Yaw Damping", "angular_damping", 0.0f, 20.0f, 0.1f);
	_add_variable_slider(physics_tab, "Vel Alignment", "velocity_alignment", 0.0f, 10.0f, 0.1f);
	_add_variable_slider(physics_tab, "Roll Influence", "roll_influence", 0.0f, 1.0f, 0.05f);
	_add_variable_slider(physics_tab, "Pitch Influence", "pitch_influence", 0.0f, 1.0f, 0.05f);

	HBoxContainer *btn_box = ui_root->add_hbox(physics_tab, "ButtonBox");
	ui_root->add_button(btn_box, "Save Settings", Callable(vehicle, "save_settings"));
	ui_root->add_button(btn_box, "Load Settings", Callable(vehicle, "load_settings"));

	// Tab 2: Stunt Settings
	VBoxContainer *stunt_tab = ui_root->add_vbox(tabs, "Stunt");
	stunt_tab->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 15);
	ui_root->add_label(stunt_tab, "Vehicle Stunt Settings");

	_add_variable_slider(stunt_tab, "Stunt Torque", "stunt_torque_strength", 1000.0f, 25000.0f, 100.0f);
	_add_variable_slider(stunt_tab, "Stunt COM Interp", "stunt_com_interpolation_speed", 1.0f, 20.0f, 0.1f);
	_add_variable_slider(stunt_tab, "Ramp Threshold", "ramp_detection_threshold", 0.5f, 1.0f, 0.01f);
	_add_variable_slider(stunt_tab, "Stunt Recovery Ht", "stunt_recovery_height", 0.5f, 10.0f, 0.1f);

	// Tab 3: Info & Graph
	VBoxContainer *info_tab = ui_root->add_vbox(tabs, "Info");
	ui_root->add_label(info_tab, "Arcade Vehicle UI v1.0");
	ui_root->add_label(info_tab, "Real-time Velocity:");
	velocity_graph = ui_root->add_graph(info_tab, "SpeedGraph");
	if (velocity_graph) {
		float max_speed = 50.0f;
		if (vehicle) {
			Ref<VehicleConfig> cfg = vehicle->get_vehicle_config();
			if (cfg.is_valid()) {
				max_speed = cfg->get_max_speed() + cfg->get_drift_boost_max_speed_bonus();
			}
		}
		velocity_graph->set_range(0, max_speed);
		velocity_graph->set_max_points(120);
	}
}

void ArcadeVehicleUI::_add_variable_slider(Node *p_parent, const String &p_label, const String &p_property, float p_min, float p_max, float p_step) {
	HBoxContainer *hbox = ui_root->add_hbox(p_parent, p_property + String("_box"));

	Label *label = ui_root->add_label(hbox, p_label);
	label->set_custom_minimum_size(Vector2(140, 0));

	float current_val = vehicle->get_ui_var(p_property);

	ui_root->add_hslider(hbox, p_min, p_max, p_step, current_val,
			Callable(vehicle, "_on_ui_slider_value_changed").bind(p_property),
			p_property);
}

void ArcadeVehicleUI::toggle_visibility() {
	if (main_panel) {
		bool is_visible = !main_panel->is_visible();
		main_panel->set_visible(is_visible);

		Input *input = Input::get_singleton();
		if (is_visible) {
			input->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
		}
	}
}

void ArcadeVehicleUI::update_graph(float p_val) {
	if (velocity_graph) {
		float max_speed = 50.0f;
		if (vehicle) {
			Ref<VehicleConfig> cfg = vehicle->get_vehicle_config();
			if (cfg.is_valid()) {
				max_speed = cfg->get_max_speed() + cfg->get_drift_boost_max_speed_bonus();
			}
		}
		velocity_graph->set_range(0.0f, max_speed);
		velocity_graph->add_value(p_val);
	}
}

bool ArcadeVehicleUI::is_visible() const {
	return main_panel && main_panel->is_visible();
}

} // namespace godot
