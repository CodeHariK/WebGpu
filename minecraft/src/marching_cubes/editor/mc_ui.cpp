// MCManager: side panel in tabs (Stats / Water / Debug / Map), help modal.
#include "marching_cubes/core/mc_hash.h"
#include "marching_cubes/editor/mc_manager.h"
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/library/mc_library.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "cui/cui.h"
#include "cui/cui_modal.h"
#include <vector>

namespace godot {

// Build the side panel: stats, buttons, hash inspection, placement mode, variant stats.
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
	main_hbox->add_theme_constant_override("separation", 10);

	// 2. Create Side Panel
	ui.side_panel = ui.manager->add_panel(main_hbox, "SidePanel", Control::PRESET_TOP_LEFT, Vector2(520, 0));
	ui.side_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	// Sidebar background now comes from the shared theme (Panel style).
	// Debug: `--mcshow` reveals the sidebar on boot (for screenshots).
	PackedStringArray _args = OS::get_singleton()->get_cmdline_user_args();
	if (_args.has("--mcshow") || _args.has("--mcshot")) {
		ui.side_panel->show();
	} else {
		ui.side_panel->hide();
	}
	if (_args.has("--mcshot")) {
		Ref<SceneTreeTimer> _t = get_tree()->create_timer(1.2);
		_t->connect("timeout", callable_mp(this, &MCManager::_save_screenshot_and_quit));
	}

	// 3. Create Buttons
	Button *stats_btn = ui.manager->add_button(main_hbox, "Stats", Callable(this, "_on_toggle_ui"));
	stats_btn->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
	stats_btn->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	stats_btn->set_custom_minimum_size(Vector2(80, 40));

	Button *help_btn = ui.manager->add_button(main_hbox, "Help", Callable(this, "_on_show_help"));
	help_btn->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
	help_btn->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	help_btn->set_custom_minimum_size(Vector2(80, 40));

	_setup_tool_bar(main_hbox); // Terrain / Water / Place / Move, Undo / Redo (mc_manager_tools.cpp)

	// 4. Create Help Modal (reusable CUIModal — themed to match the sidebar,
	//    with a header band and focus-free buttons; opened via _on_show_help).
	ui.help_dialog = memnew(CUIModal);
	ui.help_dialog->set_builder(ui.manager);
	ui.manager->add_child(ui.help_dialog);

	// 5. Tabs (inside SidePanel), each a scrolling list:
	//   Stats   performance, terrain counts, the hovered cell's hash, mesh variants
	//   Water   water look (view, particles, builder) and its sliders, the pour replay
	//   Debug   inspect, terrain views, recording an inspected spot (mc_manager_record.cpp)
	//   Map     save / load, Copy for Blender, randomize, heightmap mode
	TabContainer *tabs = memnew(TabContainer);
	tabs->set_name("Tabs");
	tabs->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	tabs->set_offset(Side::SIDE_TOP, 10);
	tabs->set_offset(Side::SIDE_LEFT, 10);
	tabs->set_offset(Side::SIDE_RIGHT, -10);
	tabs->set_offset(Side::SIDE_BOTTOM, -10);
	ui.side_panel->add_child(tabs);
	auto add_tab = [&](const char *p_name) {
		ScrollContainer *scroll = ui.manager->add_scroll(tabs, p_name);
		scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
		VBoxContainer *list = ui.manager->add_vbox(scroll, String(p_name) + "List");
		list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		list->add_theme_constant_override("separation", 7);
		return list;
	};
	VBoxContainer *stats_tab = add_tab("Stats");
	VBoxContainer *water_tab = add_tab("Water");
	VBoxContainer *debug_tab = add_tab("Debug");
	VBoxContainer *map_tab = add_tab("Map");

	// Connect signals for input interception
	ui.side_panel->connect("gui_input", Callable(this, "_on_gui_input"));
	ui.stats_vbox = stats_tab;

	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	auto add_buttons = [&](VBoxContainer *p_list, const std::vector<std::pair<const char *, const char *>> &p_buttons) {
		for (const auto &entry : p_buttons) {
			Button *button = ui.manager->add_button(p_list, entry.first, Callable(this, entry.second));
			button->set_custom_minimum_size(Vector2(0, 30));
		}
	};

	// --- Stats
	ui.manager->add_header(stats_tab, "Performance");
	perf.fps_label = ui.manager->add_label(stats_tab, "FPS: 0");
	perf.draw_calls_label = ui.manager->add_label(stats_tab, "Draw Calls: 0");
	perf.meshes_label = ui.manager->add_label(stats_tab, "Engine Objects: 0");
	perf.collision_label = ui.manager->add_label(stats_tab, "Collision Pairs: 0");
	perf.memory_label = ui.manager->add_label(stats_tab, "Memory: 0 MB");
	ui.manager->add_header(stats_tab, "Terrain Stats");
	grid_stats.stats_label = ui.manager->add_label(stats_tab, "MC Meshes: 0\nMC Cells: 0\nDebug Corners: 0");
	ui.manager->add_header(stats_tab, "Hash Inspection");
	ui.hash_label = ui.manager->add_label(stats_tab, "Hash: N/A\nPos: (0, 0, 0)");
	ui.manager->add_header(stats_tab, "Mesh Variants");
	ui.variant_stats_vbox = ui.manager->add_vbox(stats_tab, "VariantStats");

	// --- Water
	ui.manager->add_header(water_tab, "Look");
	int view = grid_node ? grid_node->get_water_view() : 0;
	water_view_button = ui.manager->add_button(water_tab, String("Water view: ") + MCGrid::water_view_name(view),
			Callable(this, "_on_cycle_water_view"));
	water_view_button->set_custom_minimum_size(Vector2(0, 30));
	add_buttons(water_tab, { { "Surface: tiles / outline", "_on_toggle_water_outline_fill" },
								   { "Water particles", "_on_toggle_water_particles" },
								   { "Water: sampled / cells", "_on_toggle_water_sampled" } });
	ui.manager->add_header(water_tab, "Shape");
	// water surface height: metres below the bank one tier up
	ui.manager->add_label(water_tab, "Water drop (m below bank)");
	ui.manager->add_hslider(water_tab, 0.0f, 0.95f, 0.01f, grid_node ? grid_node->get_water_drop() : 0.15f,
			Callable(this, "_on_water_drop"), "WaterDrop");
	ui.manager->add_label(water_tab, "Water expand (m)");
	ui.manager->add_hslider(water_tab, 0.0f, 0.3f, 0.005f, grid_node ? grid_node->get_water_expand() : 0.0f,
			Callable(this, "_on_water_expand"), "WaterExpand");
	ui.manager->add_label(water_tab, "Waterfall lip radius (m)");
	ui.manager->add_hslider(water_tab, 0.0f, 0.4f, 0.01f, grid_node ? grid_node->get_water_fall_lip() : 0.2f,
			Callable(this, "_on_water_fall_lip"), "WaterFallLip");
	ui.manager->add_label(water_tab, "Spill reach (columns past a low corner)");
	ui.manager->add_hslider(water_tab, 0.0f, 8.0f, 1.0f,
			grid_node ? static_cast<float>(grid_node->get_water_spill_reach()) : 1.0f, Callable(this, "_on_water_spill"),
			"WaterSpill");
	ui.manager->add_label(water_tab, "Spill radius (m, circle; 0: column reach)");
	ui.manager->add_hslider(water_tab, 0.0f, 1.0f, 0.01f, grid_node ? grid_node->get_water_spill_radius() : 0.0f,
			Callable(this, "_on_water_spill_radius"), "WaterSpillRadius");
	ui.manager->add_label(water_tab, "Water samples per cell");
	ui.manager->add_hslider(water_tab, 2.0f, 32.0f, 1.0f,
			grid_node ? static_cast<float>(grid_node->get_water_sample_rate()) : 8.0f, Callable(this, "_on_water_samples"),
			"WaterSamples");
	ui.manager->add_header(water_tab, "Flood fill");
	add_buttons(water_tab, { { "Water debug", "_on_toggle_water_debug" }, { "Replay pour", "_on_replay_pour" } });
	// how the shape is built, one layer more per stage (mc_grid_debug_water_stages.cpp)
	ui.manager->add_label(water_tab, "Debug stage (0 off .. 6)");
	ui.manager->add_hslider(water_tab, 0.0f, 6.0f, 1.0f,
			grid_node ? static_cast<float>(grid_node->get_water_debug_stage()) : 0.0f,
			Callable(this, "_on_water_debug_stage"), "WaterDebugStage");
	water_stage_label = ui.manager->add_label(water_tab,
			MCGrid::water_debug_stage_text(grid_node ? grid_node->get_water_debug_stage() : 0));
	water_stage_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	water_stage_label->set_custom_minimum_size(Vector2(1, 0)); // wrap to the tab's width

	// --- Debug
	ui.manager->add_header(debug_tab, "Inspect");
	add_buttons(debug_tab, { { "Inspect", "_on_toggle_inspect" } });
	Label *record_hint = ui.manager->add_label(debug_tab,
			"Click a cell to inspect it: the camera, the cell, the map and a screenshot are recorded (user://inspect/last.*).");
	add_buttons(debug_tab, { { "Record view", "_on_record_view" } });
	record_label = ui.manager->add_label(debug_tab, "Nothing recorded yet");
	for (Label *label : { record_hint, record_label }) {
		label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		label->set_custom_minimum_size(Vector2(1, 0)); // wrap to the tab's width, never widen it
	}
	// the last pour, one flood-fill step at a time (the explanation floats over the column)
	ui.manager->add_header(debug_tab, "Water step by step");
	add_buttons(debug_tab, { { "Trace step 1: the water path (whole map)", "_on_trace_settle" },
								   { "Trace step 2a: spills", "_on_trace_spills" },
								   { "Trace step 2b: held / stream", "_on_trace_held" },
								   { "Trace step 2c: flow", "_on_trace_flow" } });
	add_buttons(debug_tab, { { "< Back", "_on_pour_back" }, { "Next >", "_on_pour_next" } });
	add_buttons(debug_tab, { { "<< 10", "_on_pour_back_10" }, { "10 >>", "_on_pour_next_10" },
								   { "Whole pour", "_on_pour_whole" } });
	pour_step_ui_label = ui.manager->add_label(debug_tab, "");
	pour_step_ui_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	pour_step_ui_label->set_custom_minimum_size(Vector2(1, 0));
	_step_pour(0);
	ui.manager->add_header(debug_tab, "Terrain views");
	add_buttons(debug_tab, { { "Wireframe", "_on_toggle_wireframe" }, { "Edge corners", "_on_toggle_edge_corners" },
								   { "Tile seams", "_on_toggle_tile_seams" } });

	// --- Map
	ui.manager->add_header(map_tab, "File");
	add_buttons(map_tab, { { "Save State", "_on_save_terrain" }, { "Load State", "_on_load_terrain" },
								 { "Reset", "_on_reset_map" }, { "Copy for Blender", "_on_copy_for_blender" } });
	ui.stats_vbox = map_tab; // _setup_randomize_ui adds to ui.stats_vbox
	_setup_randomize_ui();
	ui.stats_vbox = stats_tab;

	if (_args.has("--mcshot") && ui.help_dialog) {
		call_deferred("_on_show_help");
	}
}

// Show / hide the side panel.
void MCManager::_on_toggle_ui() {
	if (ui.side_panel) {
		bool is_visible = !ui.side_panel->is_visible();
		ui.side_panel->set_visible(is_visible);
		if (is_visible) {
			update_ui();
		}
	}
}

// Clicks on the panel stay on the panel.
void MCManager::_on_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::MOUSE_BUTTON_WHEEL_UP ||
			mb->get_button_index() == MouseButton::MOUSE_BUTTON_WHEEL_DOWN) {
			get_viewport()->set_input_as_handled();
		}
	}
}

// Open the help dialog.
void MCManager::_on_show_help() {
	if (ui.help_dialog) {
		ui.help_dialog->open_alert(
				"Marching Cubes Info",
				"This project visualizes the 256 voxel configurations of the Marching Cubes algorithm.\n\n"
				"- Modelled tiles fill all 256 cell hashes by turning about Y.\n"
				"- Transformations are applied via bitwise-mapped rotation matrices.\n"
				"- Current UI tracks real-time performance and generation stats.\n\n"
				"- Stats Button: Toggle sidebar visibility",
				"OK"
		);
	}
}

// Refresh FPS, draw calls, tile / chunk counts and the hovered cell's hash.
void MCManager::update_ui() {
	if (!ui.stats_vbox || !ui.manager || !is_inside_tree())
		return;
	_update_history_buttons(); // a load / mode switch may have cleared the history
	_update_pour_label(); // a new pour, or a reset

	Performance *perf_singleton = Performance::get_singleton();

	if (perf.fps_label)
		perf.fps_label->set_text("FPS: " + String::num(perf_singleton->get_monitor(Performance::TIME_FPS), 1));
	if (perf.draw_calls_label)
		perf.draw_calls_label->set_text(
				"Draw Calls: " +
				String::num_int64((int64_t)perf_singleton->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME))
		);
	if (perf.meshes_label)
		perf.meshes_label->set_text(
				"Engine Objects: " +
				String::num_int64((int64_t)perf_singleton->get_monitor(Performance::RENDER_TOTAL_OBJECTS_IN_FRAME))
		);
	if (perf.collision_label)
		perf.collision_label->set_text(
				"Collision Pairs: " +
				String::num_int64((int64_t)perf_singleton->get_monitor(Performance::PHYSICS_3D_COLLISION_PAIRS))
		);
	if (perf.memory_label)
		perf.memory_label->set_text(
				"Static Memory: " +
				String::num(perf_singleton->get_monitor(Performance::MEMORY_STATIC) / 1024.0 / 1024.0, 2) + " MB"
		);

	MCGrid *grid_node = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid_node && grid_stats.stats_label) {
		grid_stats.stats_label->set_text(
				"MC Tiles: " + String::num_int64(grid_node->get_total_tiles_drawn()) +
				"\nChunk Meshes: " + String::num_int64(grid_node->get_total_chunk_meshes()) +
				"\nMC Cells: " + String::num_int64(grid_node->get_total_cells()) +
				"\nDebug Corners: " + String::num_int64(grid_node->get_total_debug_corners())
		);
	}

	if (ui.hash_label) {
		if (interaction_mode == MODE_TERRAIN) {
			uint8_t h = _get_cell_hash(locked_grid_pos);
			String bin = mc::hash_to_binary(h);
			Vector3 display_pos = Vector3(locked_grid_pos);
			String level = grid_node ? grid_node->get_cell_level(locked_grid_pos).to_upper() : String();
			ui.hash_label->set_text(
					"Hash: " + bin + " (" + String::num_int64(h) + ")" + (level.is_empty() ? "" : "  Level: " + level) +
					"\nPos: " + String(display_pos)
			);
		} else {
			ui.hash_label->set_text("Hash: N/A\nPos: N/A");
		}
	}

	// Update variant counts if side panel is visible
	if (ui.side_panel && ui.side_panel->is_visible() && ui.variant_stats_vbox) {
		MCTileLibrary *tile_library = Object::cast_to<MCTileLibrary>(get_node_or_null(tile_library_path));
		if (tile_library) {
			// Clear children of variant_stats_vbox
			TypedArray<Node> children = ui.variant_stats_vbox->get_children();
			for (int i = 0; i < children.size(); i++) {
				Node *child = Object::cast_to<Node>(children[i]);
				child->queue_free();
			}

			Dictionary counts = tile_library->get_hashes_per_tile();
			std::vector<uint8_t> base_order = tile_library->get_loaded_hashes();
			int total = 0;
			Array keys = counts.keys();
			for (int i = 0; i < keys.size(); i++) {
				total += (int)counts[keys[i]];
			}

			for (uint8_t base_h : base_order) {
				String base_name = mc::hash_to_binary(base_h);
				int count = counts.has(base_name) ? (int)counts[base_name] : 0;
				int bit_count = 0;
				for (int i = 0; i < 8; i++) {
					if ((base_h >> i) & 1)
						bit_count++;
				}
				ui.manager->add_label(
						ui.variant_stats_vbox,
						String::num_int64(bit_count) + " : " + base_name + " : " + String::num_int64(count)
				);
			}
			ui.manager->add_label(ui.variant_stats_vbox, "Cumulative: " + String::num_int64(total));
		}
	}
}

// Debug (--mcshot): save the viewport to docs/png/mc_pretty.png and quit.
void MCManager::_save_screenshot_and_quit() {
	Ref<Image> img = get_viewport()->get_texture()->get_image();
	img->save_png("res://../docs/png/mc_pretty.png");
	get_tree()->quit();
}

} //namespace godot
