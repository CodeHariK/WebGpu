#include "marching_cubes/mc.h"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/viewport.hpp>

#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "cui/cui.h"

namespace godot {

void MCNode::setup_ui() {
	if (!ui_manager)
		return;

	// 0. Verification Panel (Glass Effect)
	Panel *bg = ui_manager->add_panel(ui_manager, "VerificationBG", Control::PRESET_FULL_RECT, Vector2(0, 0));
	bg->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	bg->set_modulate(Color(1, 1, 1, 0.1f)); // Subly visible

	// 1. Create Main Horizontal Container
	HBoxContainer *main_hbox = ui_manager->add_hbox(ui_manager, "MainLayout");
	main_hbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	main_hbox->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	// 2. Create Side Panel
	side_panel = ui_manager->add_panel(main_hbox, "SidePanel", Control::PRESET_TOP_LEFT, Vector2(250, 0));
	side_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	side_panel->hide();

	// 3. Create Buttons
	Button *stats_btn = ui_manager->add_button(main_hbox, "Stats", Callable(this, "_on_toggle_ui"));
	stats_btn->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
	stats_btn->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	stats_btn->set_custom_minimum_size(Vector2(80, 40));

	Button *help_btn = ui_manager->add_button(main_hbox, "Help", Callable(this, "_on_show_help"));
	help_btn->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
	help_btn->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	help_btn->set_custom_minimum_size(Vector2(80, 40));

	// 4. Create Help Dialog
	help_dialog = ui_manager->add_dialog(ui_manager, "Marching Cubes Info",
			"This project visualizes the 256 voxel configurations of the Marching Cubes algorithm.\n\n"
			"- 21 base meshes generate all 256 variants via rotations/reflections.\n"
			"- Transformations are applied via bitwise-mapped rotation matrices.\n"
			"- The 'Stats' panel shows currently registered variants vs expected counts.\n\n"
			"Controls:\n"
			"- WASD: Fly Camera\n"
			"- Mouse Wheel: Zoom (works when NOT hovering UI)\n"
			"- Stats Button: Toggle sidebar visibility");
	help_dialog->set_min_size(Vector2(400, 300));

	// 5. Create Scroll Container (inside SidePanel)
	ScrollContainer *scroll = ui_manager->add_scroll(side_panel, "ScrollList");
	scroll->set_offset(Side::SIDE_TOP, 10);
	scroll->set_offset(Side::SIDE_LEFT, 5);
	scroll->set_offset(Side::SIDE_RIGHT, -5);
	scroll->set_offset(Side::SIDE_BOTTOM, -5);

	// Connect signals for input interception
	side_panel->connect("gui_input", Callable(this, "_on_gui_input"));

	// 5. Create List Container
	stats_vbox = ui_manager->add_vbox(scroll, "StatsList");
}

void MCNode::_on_toggle_ui() {
	if (side_panel) {
		bool is_visible = !side_panel->is_visible();
		side_panel->set_visible(is_visible);
		UtilityFunctions::print("MCNode: Toggle UI -> ", is_visible ? "Visible" : "Hidden");
	}
}

void MCNode::_on_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::MOUSE_BUTTON_WHEEL_UP || mb->get_button_index() == MouseButton::MOUSE_BUTTON_WHEEL_DOWN) {
			get_viewport()->set_input_as_handled();
		}
	}
}

void MCNode::_on_show_help() {
	if (help_dialog) {
		help_dialog->popup_centered();
	}
}

void MCNode::update_ui_counts() {
	if (!stats_vbox || !ui_manager)
		return;

	// Clear previous labels
	TypedArray<Node> children = stats_vbox->get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		child->queue_free();
	}

	// Calculate counts per base mesh
	HashMap<String, int> counts;
	int total = 0;
	for (const KeyValue<String, MeshConfig> &E : mesh_library) {
		counts[E.value.source_mesh]++;
		total++;
	}

	ui_manager->add_label(stats_vbox, "--- Sorted Variants ---");
	for (const String &base_name : base_mesh_order) {
		int count = counts.has(base_name) ? counts[base_name] : 0;
		int bit_count = 0;
		for (int i = 0; i < base_name.length(); i++) {
			if (base_name[i] == '1')
				bit_count++;
		}
		ui_manager->add_label(stats_vbox, String::num_int64(bit_count) + " : " + base_name + String(": ") + String::num_int64(count));
	}
	ui_manager->add_label(stats_vbox, "-----------------------");
	ui_manager->add_label(stats_vbox, String("Cumulative: ") + String::num_int64(total));
}

} //namespace godot
