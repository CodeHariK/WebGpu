#include "marching_cubes/mc.h"
#include "marching_cubes/terrain.h"
#include "marching_cubes/mc_manager.h"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/classes/engine.hpp>

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

void MCManager::setup_ui() {
	if (!ui.manager)
		return;

	// 0. Verification Panel (Glass Effect)
	Panel *bg = ui.manager->add_panel(ui.manager, "VerificationBG", Control::PRESET_FULL_RECT, Vector2(0, 0));
	bg->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	bg->set_modulate(Color(1, 1, 1, 0.1f));

	// 1. Create Main Horizontal Container
	HBoxContainer *main_hbox = ui.manager->add_hbox(ui.manager, "MainLayout");
	main_hbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	main_hbox->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	// 2. Create Side Panel
	ui.side_panel = ui.manager->add_panel(main_hbox, "SidePanel", Control::PRESET_TOP_LEFT, Vector2(320, 0));
	ui.side_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	ui.side_panel->hide();

	// 3. Create Buttons
	Button *stats_btn = ui.manager->add_button(main_hbox, "Stats", Callable(this, "_on_toggle_ui"));
	stats_btn->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
	stats_btn->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	stats_btn->set_custom_minimum_size(Vector2(80, 40));

	Button *help_btn = ui.manager->add_button(main_hbox, "Help", Callable(this, "_on_show_help"));
	help_btn->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
	help_btn->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	help_btn->set_custom_minimum_size(Vector2(80, 40));

	// 4. Create Help Dialog
	ui.help_dialog = ui.manager->add_dialog(ui.manager, "Marching Cubes Info",
			"This project visualizes the 256 voxel configurations of the Marching Cubes algorithm.\n\n"
			"- 21 base meshes generate all 256 variants via rotations/reflections.\n"
			"- Transformations are applied via bitwise-mapped rotation matrices.\n"
			"- Current UI tracks real-time performance and generation stats.\n\n"
			"Controls:\n"
			"- WASDQE: Move Debug Cursor (Green Cube)\n"
			"- Mouse Wheel: Zoom (works when NOT hovering UI)\n"
			"- Stats Button: Toggle sidebar visibility");
	ui.help_dialog->set_min_size(Vector2(400, 300));

	// 5. Create Scroll Container (inside SidePanel)
	ScrollContainer *scroll = ui.manager->add_scroll(ui.side_panel, "ScrollList");
	scroll->set_offset(Side::SIDE_TOP, 10);
	scroll->set_offset(Side::SIDE_LEFT, 10);
	scroll->set_offset(Side::SIDE_RIGHT, -10);
	scroll->set_offset(Side::SIDE_BOTTOM, -10);

	// Connect signals for input interception
	ui.side_panel->connect("gui_input", Callable(this, "_on_gui_input"));

	// 6. Create List Container
	ui.stats_vbox = ui.manager->add_vbox(scroll, "StatsList");

	ui.manager->add_label(ui.stats_vbox, "--- PERFORMANCE ---");
	perf.fps_label = ui.manager->add_label(ui.stats_vbox, "FPS: 0");
	perf.draw_calls_label = ui.manager->add_label(ui.stats_vbox, "Draw Calls: 0");
	perf.meshes_label = ui.manager->add_label(ui.stats_vbox, "Engine Objects: 0");
	perf.collision_label = ui.manager->add_label(ui.stats_vbox, "Collision Pairs: 0");
	perf.memory_label = ui.manager->add_label(ui.stats_vbox, "Memory: 0 MB");

	ui.manager->add_label(ui.stats_vbox, "--- DIAGNOSTICS ---");
	Button *diag_btn = ui.manager->add_button(ui.stats_vbox, "Toggle Collision Debug", Callable(this, "_on_toggle_collision_debug"));
	diag_btn->set_custom_minimum_size(Vector2(0, 30));
	
	Button *vis_btn = ui.manager->add_button(ui.stats_vbox, "Toggle Visual Corners", Callable(this, "_on_toggle_visual_corners"));
	vis_btn->set_custom_minimum_size(Vector2(0, 30));
	
	ui.manager->add_label(ui.stats_vbox, "--- TERRAIN STATS ---");
	terrain.stats_label = ui.manager->add_label(ui.stats_vbox, "MC Meshes: 0\nMC Cells: 0\nDebug Corners: 0");
	debug_cursor_label = ui.manager->add_label(ui.stats_vbox, "Cursor: (0,0,0) Hash: 00000000");

	Button *save_btn = ui.manager->add_button(ui.stats_vbox, "Save State", Callable(this, "_on_save_terrain"));
	save_btn->set_custom_minimum_size(Vector2(0, 30));

	Button *load_btn = ui.manager->add_button(ui.stats_vbox, "Load State", Callable(this, "_on_load_terrain"));
	load_btn->set_custom_minimum_size(Vector2(0, 30));

	ui.manager->add_label(ui.stats_vbox, "--- MESH VARIANTS ---");
	ui.variant_stats_vbox = ui.manager->add_vbox(ui.stats_vbox, "VariantStats");
}

void MCManager::_on_toggle_ui() {
	if (ui.side_panel) {
		bool is_visible = !ui.side_panel->is_visible();
		ui.side_panel->set_visible(is_visible);
		if (is_visible) {
			update_ui();
		}
	}
}

void MCManager::_on_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::MOUSE_BUTTON_WHEEL_UP || mb->get_button_index() == MouseButton::MOUSE_BUTTON_WHEEL_DOWN) {
			get_viewport()->set_input_as_handled();
		}
	}
}

void MCManager::_on_show_help() {
	if (ui.help_dialog) {
		ui.help_dialog->popup_centered();
	}
}

void MCManager::update_ui() {
	if (!ui.stats_vbox || !ui.manager || !is_inside_tree())
		return;

	Performance *perf_singleton = Performance::get_singleton();
	
	if (perf.fps_label) perf.fps_label->set_text("FPS: " + String::num(perf_singleton->get_monitor(Performance::TIME_FPS), 1));
	if (perf.draw_calls_label) perf.draw_calls_label->set_text("Draw Calls: " + String::num_int64((int64_t)perf_singleton->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME)));
	if (perf.meshes_label) perf.meshes_label->set_text("Engine Objects: " + String::num_int64((int64_t)perf_singleton->get_monitor(Performance::RENDER_TOTAL_OBJECTS_IN_FRAME)));
	if (perf.collision_label) perf.collision_label->set_text("Collision Pairs: " + String::num_int64((int64_t)perf_singleton->get_monitor(Performance::PHYSICS_3D_COLLISION_PAIRS)));
	if (perf.memory_label) perf.memory_label->set_text("Static Memory: " + String::num(perf_singleton->get_monitor(Performance::MEMORY_STATIC) / 1024.0 / 1024.0, 2) + " MB");

	MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));
	if (terrain_node && terrain.stats_label) {
		terrain.stats_label->set_text("MC Meshes: " + String::num_int64(terrain_node->get_total_mc_meshes()) + 
								 "\nMC Cells: " + String::num_int64(terrain_node->get_total_cells()) +
								 "\nDebug Corners: " + String::num_int64(terrain_node->get_total_debug_corners()));
		
		if (debug_cursor_label) {
			uint8_t hash = terrain_node->get_cell_hash_at_global_coord(debug_cursor_pos);
			String h_str = MCNode::hash_to_binary(hash);
			debug_cursor_label->set_text("Cursor: (" + String::num_int64(debug_cursor_pos.x) + "," + 
										String::num_int64(debug_cursor_pos.y) + "," + 
										String::num_int64(debug_cursor_pos.z) + ") Hash: " + h_str);
		}
	}

	// Update variant counts if side panel is visible
	if (ui.side_panel && ui.side_panel->is_visible() && ui.variant_stats_vbox) {
		MCNode *mc_node = Object::cast_to<MCNode>(get_node_or_null(mc_node_path));
		if (mc_node) {
			// Clear children of variant_stats_vbox
			TypedArray<Node> children = ui.variant_stats_vbox->get_children();
			for (int i = 0; i < children.size(); i++) {
				Node *child = Object::cast_to<Node>(children[i]);
				child->queue_free();
			}

			Dictionary counts = mc_node->get_variant_counts();
			std::vector<uint8_t> base_order = mc_node->get_base_mesh_order();
			int total = 0;
			Array keys = counts.keys();
			for (int i = 0; i < keys.size(); i++) {
				total += (int)counts[keys[i]];
			}

			for (uint8_t base_h : base_order) {
				String base_name = MCNode::hash_to_binary(base_h);
				int count = counts.has(base_name) ? (int)counts[base_name] : 0;
				int bit_count = 0;
				for (int i = 0; i < 8; i++) {
					if ((base_h >> i) & 1) bit_count++;
				}
				ui.manager->add_label(ui.variant_stats_vbox, String::num_int64(bit_count) + " : " + base_name + " : " + String::num_int64(count));
			}
			ui.manager->add_label(ui.variant_stats_vbox, "Cumulative: " + String::num_int64(total));
		}
	}
}


} //namespace godot
