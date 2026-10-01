#include "character_ui.h"

#include "../cui/cui.h"
#include "../cui/cui_line_graph.h"
#include "spring_character.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

namespace godot {

CharacterUI::CharacterUI() {}
CharacterUI::~CharacterUI() {}

void CharacterUI::setup(
		SpringCharacter *p_controller,
		CUI *p_ui_root
) {
	controller = p_controller;
	ui_root = p_ui_root;
	if (!controller || !ui_root) {
		return;
	}

	Button *toggle_btn = ui_root->add_button(ui_root, "Character", Callable(controller, "_on_ui_toggle"));
	toggle_btn->set_anchors_and_offsets_preset(Control::PRESET_TOP_RIGHT, Control::PRESET_MODE_MINSIZE, 20);
	// Offset below Celeste's "Settings" button so both are reachable while Celeste exists.
	toggle_btn->set_position(toggle_btn->get_position() + Vector2(0, 44));

	main_panel = ui_root->add_panel(ui_root, "CharPanel", Control::PRESET_TOP_LEFT, Vector2(520, 520));
	main_panel->set_position(Vector2(main_panel->get_position().x, 50));
	main_panel->set_visible(false);

	TabContainer *tabs = ui_root->add_tab_container(main_panel, "Tabs");

	// --- Tab 1: Move / Jump ---
	VBoxContainer *move_tab = ui_root->add_vbox(tabs, "Move/Jump");
	move_tab->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 15);
	ui_root->add_label(move_tab, "Movement");
	_slider(move_tab, "Max Speed", "max_speed", 1.0f, 40.0f, 0.5f);
	_slider(move_tab, "Acceleration", "acceleration", 10.0f, 300.0f, 5.0f);
	_slider(move_tab, "Sprint Mult", "sprint_multiplier", 1.0f, 3.0f, 0.1f);
	ui_root->add_label(move_tab, "Steering / Turn");
	_slider(move_tab, "Steer Rate", "steer_rate", 0.5f, 8.0f, 0.1f);
	_slider(move_tab, "Face Turn Rate", "face_turn_rate", 2.0f, 30.0f, 0.5f);
	ui_root->add_label(move_tab, "Jump / Gravity");
	_slider(move_tab, "Jump Height", "jump_height", 0.5f, 8.0f, 0.1f);
	_slider(move_tab, "Time to Peak", "jump_time_to_peak", 0.1f, 1.0f, 0.01f);
	_slider(move_tab, "Time to Descent", "jump_time_to_descent", 0.1f, 1.0f, 0.01f);
	_slider(move_tab, "Coyote Time", "coyote_time", 0.0f, 0.5f, 0.01f);
	_slider(move_tab, "Jump Buffer", "jump_buffer", 0.0f, 0.5f, 0.01f);

	// --- Tab 2: Ride / Abilities ---
	VBoxContainer *abil_tab = ui_root->add_vbox(tabs, "Ride/Abilities");
	abil_tab->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 15);
	ui_root->add_label(abil_tab, "Ride Servo");
	_slider(abil_tab, "Min Ride Height", "min_ride_height", 0.0f, 0.6f, 0.05f);
	_slider(abil_tab, "Max Ride Height", "max_ride_height", 0.2f, 1.2f, 0.05f);
	_slider(abil_tab, "Ride Adjust Speed", "ride_height_speed", 1.0f, 20.0f, 0.5f);
	_slider(abil_tab, "Ride Follow", "ride_follow", 2.0f, 40.0f, 1.0f);
	ui_root->add_label(abil_tab, "Dash / Pound");
	_slider(abil_tab, "Dash Speed", "dash_speed", 5.0f, 60.0f, 1.0f);
	_slider(abil_tab, "Dash Cooldown", "dash_cooldown", 0.1f, 1.5f, 0.05f);
	_slider(abil_tab, "Pound Speed", "pound_speed", 5.0f, 60.0f, 1.0f);
	ui_root->add_label(abil_tab, "Wall / Mantle");
	_slider(abil_tab, "Wall Jump Up", "wall_jump_up", 2.0f, 30.0f, 0.5f);
	_slider(abil_tab, "Wall Jump Out", "wall_jump_out", 0.0f, 20.0f, 0.5f);

	// --- Tab 3: Combat / Feel + graph ---
	VBoxContainer *feel_tab = ui_root->add_vbox(tabs, "Combat/Feel");
	feel_tab->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 15);
	ui_root->add_label(feel_tab, "Real-time Speed:");
	speed_graph = ui_root->add_graph(feel_tab, "SpeedGraph");
	if (speed_graph) {
		speed_graph->set_range(0.0f, 40.0f);
		speed_graph->set_max_points(120);
	}

	HBoxContainer *btn_box = ui_root->add_hbox(move_tab, "ButtonBox");
	ui_root->add_button(btn_box, "Save", Callable(controller, "save_settings"));
	ui_root->add_button(btn_box, "Load", Callable(controller, "load_settings"));
}

void CharacterUI::_slider(
		Node *p_parent,
		const String &p_label,
		const String &p_property,
		float p_min,
		float p_max,
		float p_step
) {
	HBoxContainer *hbox = ui_root->add_hbox(p_parent, p_property + String("_box"));
	Label *label = ui_root->add_label(hbox, p_label);
	label->set_custom_minimum_size(Vector2(130, 0));
	float current_val = controller->get_ui_var(p_property);
	ui_root->add_hslider(
			hbox, p_min, p_max, p_step, current_val,
			Callable(controller, "_on_ui_slider_value_changed").bind(p_property), p_property
	);
}

void CharacterUI::toggle_visibility() {
	if (main_panel) {
		bool vis = !main_panel->is_visible();
		main_panel->set_visible(vis);
		if (vis) {
			Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
		}
	}
}

void CharacterUI::update_graph(float p_val) {
	if (speed_graph) {
		speed_graph->add_value(p_val);
	}
}

bool CharacterUI::is_visible() const { return main_panel && main_panel->is_visible(); }

} // namespace godot
