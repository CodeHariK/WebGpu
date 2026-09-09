#include "game_manager.h"
#include "../camera/camera.h"
#include "../cui/cui.h"
#include "../debug_draw/debug_manager.h"
#include "../enemy/enemy_manager.h"
#include "../marching_cubes/mc_manager.h"
#include "../minigames/tennis/tennis_manager.h"
#include "../player/celeste_controller.h"
#include "../terrain/marching_prism/mp_manager.h"
#include "../vehicle/arcade_vehicle.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "../minigames/overcooked/oc_manager.h"
#include <vector>

namespace godot {

GameManager *GameManager::singleton = nullptr;

void GameManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_mc_manager", "p_manager"), &GameManager::register_mc_manager);
	ClassDB::bind_method(D_METHOD("get_mc_manager"), &GameManager::get_mc_manager);

	ClassDB::bind_method(D_METHOD("register_mp_manager", "p_manager"), &GameManager::register_mp_manager);
	ClassDB::bind_method(D_METHOD("get_mp_manager"), &GameManager::get_mp_manager);

	ClassDB::bind_method(D_METHOD("save_game", "p_slot_name"), &GameManager::save_game);
	ClassDB::bind_method(D_METHOD("load_game", "p_slot_name"), &GameManager::load_game);

	ClassDB::bind_method(D_METHOD("set_active_target", "p_target"), &GameManager::set_active_target);
	ClassDB::bind_method(D_METHOD("get_active_target"), &GameManager::get_active_target);
	ClassDB::bind_method(D_METHOD("register_vehicle", "p_vehicle"), &GameManager::register_vehicle);
	ClassDB::bind_method(D_METHOD("register_tennis_manager", "p_manager"), &GameManager::register_tennis_manager);
	ClassDB::bind_method(D_METHOD("get_tennis_manager"), &GameManager::get_tennis_manager);
	ClassDB::bind_method(D_METHOD("register_camera", "p_camera"), &GameManager::register_camera);
	ClassDB::bind_method(D_METHOD("get_debug_manager"), &GameManager::get_debug_manager);
}

GameManager::GameManager() {
	// Don't initialize subsystems here; wait until _enter_tree to ensure we are the singleton.
}

GameManager::~GameManager() {
	if (player_input) {
		memdelete(player_input);
		player_input = nullptr;
	}
	if (enemy_manager) {
		memdelete(enemy_manager);
		enemy_manager = nullptr;
	}
	if (overcooked_manager) {
		memdelete(overcooked_manager);
		overcooked_manager = nullptr;
	}
	if (singleton == this) {
		singleton = nullptr;
	}
}

GameManager *GameManager::get_singleton() { return singleton; }

void GameManager::_enter_tree() {
	if (singleton != nullptr && singleton != this) {
		UtilityFunctions::printerr("GameManager Error: Multiple instances detected! Removing extra instance.");
		queue_free();
		return;
	}

	singleton = this;

	// Lazy Initializion of subsystems only for the true singleton
	if (!player_input) {
		player_input = memnew(PlayerInput);
	}

	if (!debug_manager) {
		debug_manager = memnew(DebugManager);
		debug_manager->set_name("DebugManager");
	}

	if (!enemy_manager) {
		enemy_manager = memnew(EnemyManager);
	}

	if (debug_manager && debug_manager->get_parent() == nullptr) {
		add_child(debug_manager);
	}

	if (!overcooked_manager) {
		overcooked_manager = memnew(OvercookedManager);
	}

	UtilityFunctions::print("GameManager: Singleton initialized and ready.");
}

void GameManager::_exit_tree() {
	if (singleton == this) {
		singleton = nullptr;
		UtilityFunctions::print("GameManager: Singleton cleared on exit tree.");
	}
}

void GameManager::register_mc_manager(MCManager *p_manager) {
	mc_manager = p_manager;
	UtilityFunctions::print("GameManager: Registered MCManager (Marching Cubes System).");
}

MCManager *GameManager::get_mc_manager() const { return mc_manager; }

void GameManager::register_mp_manager(MPManager *p_manager) {
	mp_manager = p_manager;
	UtilityFunctions::print("GameManager: Registered MPManager (Marching Prism System).");
}

MPManager *GameManager::get_mp_manager() const { return mp_manager; }

void GameManager::register_tennis_manager(TennisManager *p_manager) {
	tennis_manager = p_manager;
	UtilityFunctions::print("GameManager: Registered TennisManager.");
}

TennisManager *GameManager::get_tennis_manager() const { return tennis_manager; }

void GameManager::register_vehicle(ArcadeVehicle *p_vehicle) {
	vehicle = p_vehicle;
	if (vehicle) {
		vehicle->set_game_manager(this);
		if (player_input) {
			vehicle->set_player_input(player_input);
		}
		if (active_target == nullptr) {
			set_active_target(vehicle);
		}
	}
	UtilityFunctions::print("GameManager: Registered ArcadeVehicle.");
}

ArcadeVehicle *GameManager::get_vehicle() const { return vehicle; }

void GameManager::register_celeste_controller(Node *p_character) {
	if (p_character == nullptr) {
		if (active_target == celeste_character) {
			active_target = nullptr;
		}
		celeste_character = nullptr;
		return;
	}
	celeste_character = Object::cast_to<CelesteController>(p_character);
	if (celeste_character) {
		UtilityFunctions::print("GameManager: Registered CelesteController.");
		if (active_target == nullptr) {
			set_active_target(celeste_character);
		}
	}
}

Node *GameManager::get_celeste_controller() const { return celeste_character; }

void GameManager::register_camera(GameCamera *p_camera) {
	if (p_camera == nullptr) {
		main_camera = nullptr;
		return;
	}
	main_camera = p_camera;
	if (main_camera && player_input) {
		main_camera->set_player_input(player_input);
	}
	UtilityFunctions::print("GameManager: Registered GameCamera.");
}

GameCamera *GameManager::get_camera() const { return main_camera; }

void GameManager::set_active_target(Node *p_target) {
	active_target = p_target;
	if (active_target) {
		UtilityFunctions::print("GameManager: Active target set to: ", active_target->get_name());

		if (main_camera) {
			Node3D *target_3d = Object::cast_to<Node3D>(active_target);
			if (target_3d) {
				main_camera->set_follow_target_node(target_3d);

				// Auto-switch camera mode
				if (Object::cast_to<ArcadeVehicle>(active_target)) {
					main_camera->set_camera_mode(GameCamera::MODE_CAR);
				} else if (Object::cast_to<CelesteController>(active_target)) {
					// Only auto-switch to TPS if we aren't already in a character-friendly mode like FIXED
					if (main_camera->get_camera_mode() != GameCamera::MODE_FIXED) {
						main_camera->set_camera_mode(GameCamera::MODE_TPS);
					}
				}
			}
		}
	}
}

Node *GameManager::get_active_target() const { return active_target; }

void GameManager::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint())
		return;

	if (player_input) {
		player_input->update();

		// Cursor management (Escape to show/hide)
		Input *input = Input::get_singleton();
		if (input->is_action_just_pressed("ui_cancel")) {
			if (input->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED) {
				input->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
			} else {
				input->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
			}
		}

		// Handle target switching (TAB) via Action State
		const ActionState &state = player_input->get_state();
		if (state.system.swap_target_just_pressed) {
			// Create a list of available targets to cycle through
			std::vector<Node *> valid_targets;
			if (vehicle)
				valid_targets.push_back(vehicle);
			if (celeste_character)
				valid_targets.push_back(celeste_character);

			if (valid_targets.empty())
				return;

			// Find current index
			int current_index = -1;
			for (int i = 0; i < valid_targets.size(); ++i) {
				if (valid_targets[i] == active_target) {
					current_index = i;
					break;
				}
			}

			// Cycle to next
			int next_index = (current_index + 1) % valid_targets.size();
			set_active_target(valid_targets[next_index]);
		}

		// Reset deltas at the end of the physics frame
		// Wait, better to let the consumers (like camera) handle it if needed,
		// but since multiple systems might read it, we reset it at the end of GameManager's process.
		// NOTE: This assumes GameManager runs before other systems or we reset at the start of next update.
	}
}

void GameManager::_input(const Ref<InputEvent> &p_event) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && !k->is_echo()) {
		// F3 cycles the debug views: the viewport's built-in ones (unshaded, overdraw, wireframe,
		// normals), then TerraSpline's shader views through the `ts_debug_view` shader global
		// (1 UV, 2 UV2, 3 vertex colour, 4 road marking mask, 5 painted control map), then off.
		if (k->get_keycode() == KEY_F3) {
			Viewport *viewport = get_viewport();
			if (viewport) {
				static const char *TS_VIEW_NAMES[] = { "off",			"UV",			"UV2",
													   "vertex colour", "marking mask", "control map" };
				const int TS_VIEWS = 5;
				Viewport::DebugDraw current_mode = viewport->get_debug_draw();
				Viewport::DebugDraw next_mode = Viewport::DEBUG_DRAW_DISABLED;
				int ts_view = _ts_debug_view;

				if (current_mode == Viewport::DEBUG_DRAW_DISABLED && ts_view == 0) {
					next_mode = Viewport::DEBUG_DRAW_UNSHADED;
				} else if (current_mode == Viewport::DEBUG_DRAW_UNSHADED) {
					next_mode = Viewport::DEBUG_DRAW_OVERDRAW;
				} else if (current_mode == Viewport::DEBUG_DRAW_OVERDRAW) {
					next_mode = Viewport::DEBUG_DRAW_WIREFRAME;
				} else if (current_mode == Viewport::DEBUG_DRAW_WIREFRAME) {
					next_mode = Viewport::DEBUG_DRAW_NORMAL_BUFFER;
				} else if (current_mode == Viewport::DEBUG_DRAW_NORMAL_BUFFER) {
					next_mode = Viewport::DEBUG_DRAW_DISABLED;
					ts_view = 1;
				} else {
					ts_view = ts_view < TS_VIEWS ? ts_view + 1 : 0;
				}

				viewport->set_debug_draw(next_mode);
				_ts_debug_view = ts_view;
				RenderingServer::get_singleton()->global_shader_parameter_set("ts_debug_view", ts_view);
				UtilityFunctions::print(
						"GameManager: Debug Draw mode ", next_mode, " | TerraSpline view ", TS_VIEW_NAMES[ts_view]
				);
				_update_debug_banner();
			}
		}

		// Toggle collision shape visibility on F4
		if (k->get_keycode() == KEY_F4) {
			SceneTree *tree = get_tree();
			if (tree) {
				bool current = tree->is_debugging_collisions_hint();
				tree->set_debug_collisions_hint(!current);
				UtilityFunctions::print("GameManager: Collision Debug toggled to ", !current);
				_update_debug_banner();
			}
		}
	}

	if (player_input) {
		player_input->handle_input(p_event);
	}
}

/**
 * @brief Top-centre CUI banner listing the active debug views (F3 viewport mode, F3 TerraSpline shader
 * view, F4 collision shapes). Built on first use on its own CanvasLayer; hidden when everything is off.
 */
void GameManager::_update_debug_banner() {
	static const char *VIEWPORT_NAMES[] = { "", "Unshaded", "Lighting", "Overdraw", "Wireframe", "Normal buffer" };
	static const char *TS_NAMES[] = {
		"",
		"UV  (R = around profile, G = along track)",
		"UV2  (R = across deck, G = on deck)",
		"Vertex colour  (raw, unlit)",
		"Marking mask  (roads)",
		"Control map  (hue = paint id, brightness = weight)",
	};
	Viewport *viewport = get_viewport();
	SceneTree *tree = get_tree();
	if (!viewport || !tree) {
		return;
	}
	PackedStringArray parts;
	const int vp = (int)viewport->get_debug_draw();
	if (vp > 0 && vp <= 5) {
		parts.push_back(String("F3  ") + VIEWPORT_NAMES[vp]);
	}
	if (_ts_debug_view > 0 && _ts_debug_view <= 5) {
		parts.push_back(String("F3  ") + TS_NAMES[_ts_debug_view]);
	}
	if (tree->is_debugging_collisions_hint()) {
		parts.push_back("F4  Collision shapes");
	}

	if (!_debug_ui) {
		if (parts.is_empty()) {
			return;
		}
		_debug_ui = CUI::create_on_new_layer(this);
		PanelContainer *panel = _debug_ui->add_panel_container(nullptr, "debug_banner", Control::PRESET_CENTER_TOP);
		panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		panel->set_offset(SIDE_TOP, 12.0f); // Clear of the window edge; the container keeps its own height
		panel->set_offset(SIDE_BOTTOM, 12.0f);
		Label *label = _debug_ui->add_label(panel, "", "debug_banner_text");
		label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		label->set_modulate(Color(1.0f, 0.9f, 0.35f));
	}
	Control *panel = _debug_ui->get_element("debug_banner");
	if (panel) {
		panel->set_visible(!parts.is_empty());
	}
	_debug_ui->set_text("debug_banner_text", String("DEBUG   ") + String("   |   ").join(parts));
}

void GameManager::_unhandled_input(const Ref<InputEvent> &p_event) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	// Recapture mouse on left click (Exclude Fly mode which needs visible cursor for raycasting)
	// Moved to _unhandled_input so UI clicks don't trigger capture
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_LEFT) {
		Input *input = Input::get_singleton();
		if (main_camera && main_camera->get_camera_mode() != GameCamera::MODE_FLY &&
			main_camera->get_camera_mode() != GameCamera::MODE_FIXED) {
			if (input->get_mouse_mode() == Input::MOUSE_MODE_VISIBLE) {
				input->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
			}
		}
	}
}

void GameManager::save_game(const String &p_slot_name) {
	UtilityFunctions::print("GameManager: Starting global save for slot: ", p_slot_name);

	// Create a unified file path or directory structure for this slot
	// For now, we use a simple naming convention based on the slot name
	String save_dir = "user://saves/";
	// Future: Ensure directory exists

	if (mc_manager) {
		UtilityFunctions::print("GameManager: Persisting Environment...");
		mc_manager->save_terrain(save_dir + p_slot_name + "_terrain.mct");
	}

	// Future: Save Vehicle position
	// Future: Save Player stats

	UtilityFunctions::print("GameManager: Save Complete.");
}

void GameManager::load_game(const String &p_slot_name) {
	UtilityFunctions::print("GameManager: Starting global load for slot: ", p_slot_name);
	String save_dir = "user://saves/";

	if (mc_manager) {
		UtilityFunctions::print("GameManager: Restoring Environment...");
		mc_manager->load_terrain(save_dir + p_slot_name + "_terrain.mct");
	}

	UtilityFunctions::print("GameManager: Load Complete.");
}

} // namespace godot
