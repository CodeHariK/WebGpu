#include "game_manager.h"
#include "../camera/camera.h"
#include "../character/physics_character.h"
#include "../marching_cubes/mc_manager.h"
#include "../player/celeste_controller.h"
#include "../debug_draw/debug_manager.h"
#include "../vehicle/arcade_vehicle.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

GameManager *GameManager::singleton = nullptr;

void GameManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_mc_manager", "p_manager"), &GameManager::register_mc_manager);
	ClassDB::bind_method(D_METHOD("get_mc_manager"), &GameManager::get_mc_manager);

	ClassDB::bind_method(D_METHOD("save_game", "p_slot_name"), &GameManager::save_game);
	ClassDB::bind_method(D_METHOD("load_game", "p_slot_name"), &GameManager::load_game);

	ClassDB::bind_method(D_METHOD("set_active_target", "p_target"), &GameManager::set_active_target);
	ClassDB::bind_method(D_METHOD("get_active_target"), &GameManager::get_active_target);
	ClassDB::bind_method(D_METHOD("register_vehicle", "p_vehicle"), &GameManager::register_vehicle);
	ClassDB::bind_method(D_METHOD("register_character", "p_character"), &GameManager::register_character);
	ClassDB::bind_method(D_METHOD("register_celeste_controller", "p_character"), &GameManager::register_celeste_controller);
	ClassDB::bind_method(D_METHOD("register_camera", "p_camera"), &GameManager::register_camera);
	ClassDB::bind_method(D_METHOD("get_debug_manager"), &GameManager::get_debug_manager);
}

GameManager::GameManager() {
	// Pre-initialization: don't overwrite if we already have one
	if (singleton == nullptr) {
		singleton = this;
	}
	player_input = memnew(PlayerInput);
	debug_manager = memnew(DebugManager);
	debug_manager->set_name("DebugManager");
}

GameManager::~GameManager() {
	if (player_input) {
		memdelete(player_input);
		player_input = nullptr;
	}
	if (singleton == this) {
		singleton = nullptr;
	}
}

GameManager *GameManager::get_singleton() {
	return singleton;
}

void GameManager::_enter_tree() {
	if (singleton != nullptr && singleton != this) {
		UtilityFunctions::printerr("GameManager Error: Multiple instances of GameManager detected! Only one singleton is allowed. Removing extra instance.");
		queue_free();
		return;
	}

	singleton = this;
	if (debug_manager && !debug_manager->is_inside_tree()) {
		add_child(debug_manager);
	}
	UtilityFunctions::print("GameManager: Singleton initialized and ready.");
}

void GameManager::_exit_tree() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void GameManager::register_mc_manager(MCManager *p_manager) {
	mc_manager = p_manager;
	UtilityFunctions::print("GameManager: Registered MCManager (Marching Cubes System).");
}

MCManager *GameManager::get_mc_manager() const {
	return mc_manager;
}

void GameManager::register_vehicle(ArcadeVehicle *p_vehicle) {
	vehicle = p_vehicle;
	if (vehicle) {
		vehicle->set_game_manager(this);
		if (player_input) {
			vehicle->set_player_input(player_input);
		}
	}
	UtilityFunctions::print("GameManager: Registered ArcadeVehicle.");
}

ArcadeVehicle *GameManager::get_vehicle() const {
	return vehicle;
}

void GameManager::register_character(PhysicsCharacter3D *p_character) {
	character = p_character;
	if (character) {
		character->set_game_manager(this);
		if (player_input) {
			character->set_player_input(player_input);
		}
	}
	UtilityFunctions::print("GameManager: Registered PhysicsCharacter3D.");
	if (active_target == nullptr) {
		set_active_target(character);
	}
}

PhysicsCharacter3D *GameManager::get_character() const {
	return character;
}

void GameManager::register_celeste_controller(Node *p_character) {
	celeste_character = Object::cast_to<CelesteController>(p_character);
	if (celeste_character) {
		// Note: CelesteController doesn't currently use GameManager/PlayerInput setters
		// but we might want them later for consistency.
		UtilityFunctions::print("GameManager: Registered CelesteController.");
		if (active_target == nullptr) {
			set_active_target(celeste_character);
		}
	}
}

Node *GameManager::get_celeste_controller() const {
	return celeste_character;
}

void GameManager::register_camera(GameCamera *p_camera) {
	main_camera = p_camera;
	if (main_camera && player_input) {
		main_camera->set_player_input(player_input);
	}
	UtilityFunctions::print("GameManager: Registered GameCamera.");
}

GameCamera *GameManager::get_camera() const {
	return main_camera;
}

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
				} else if (Object::cast_to<PhysicsCharacter3D>(active_target) || Object::cast_to<CelesteController>(active_target)) {
					// Only auto-switch to TPS if we aren't already in a character-friendly mode like FIXED
					if (main_camera->get_camera_mode() != GameCamera::MODE_FIXED) {
						main_camera->set_camera_mode(GameCamera::MODE_TPS);
					}
				}
			}
		}
	}
}

Node *GameManager::get_active_target() const {
	return active_target;
}

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
			if (character)
				valid_targets.push_back(character);
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
	if (player_input) {
		player_input->handle_input(p_event);
	}

	// Recapture mouse on left click (Exclude Fly mode which needs visible cursor for raycasting)
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_LEFT) {
		Input *input = Input::get_singleton();
		if (main_camera && main_camera->get_camera_mode() != GameCamera::MODE_FLY && main_camera->get_camera_mode() != GameCamera::MODE_FIXED) {
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
