#include "game_manager.h"
#include "../character/physics_character.h"
#include "../marching_cubes/mc_manager.h"
#include "../vehicle/arcade_vehicle.h"
#include "../camera/camera.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
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
	ClassDB::bind_method(D_METHOD("register_camera", "p_camera"), &GameManager::register_camera);
}

GameManager::GameManager() {
	// Pre-initialization: don't overwrite if we already have one
	if (singleton == nullptr) {
		singleton = this;
	}
}

GameManager::~GameManager() {
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
	UtilityFunctions::print("GameManager: Registered ArcadeVehicle.");
}

ArcadeVehicle *GameManager::get_vehicle() const {
	return vehicle;
}

void GameManager::register_character(PhysicsCharacter3D *p_character) {
	character = p_character;
	UtilityFunctions::print("GameManager: Registered PhysicsCharacter3D.");
	if (active_target == nullptr) {
		set_active_target(character);
	}
}

PhysicsCharacter3D *GameManager::get_character() const {
	return character;
}

void GameManager::register_camera(MCCamera *p_camera) {
	main_camera = p_camera;
	UtilityFunctions::print("GameManager: Registered MCCamera.");
}

MCCamera *GameManager::get_camera() const {
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
					main_camera->set_mode(MCCamera::MODE_CAR);
				} else if (Object::cast_to<PhysicsCharacter3D>(active_target)) {
					main_camera->set_mode(MCCamera::MODE_CHARACTER);
				}
			}
		}
	}
}

Node *GameManager::get_active_target() const {
	return active_target;
}

void GameManager::_input(const Ref<InputEvent> &p_event) {
	// Simple TAB toggle for testing
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
		if (key->get_keycode() == KEY_TAB) {
			if (active_target == character && vehicle != nullptr) {
				set_active_target(vehicle);
			} else if (active_target == vehicle && character != nullptr) {
				set_active_target(character);
			} else if (active_target == nullptr) {
				// Fallback
				if (character)
					set_active_target(character);
				else if (vehicle)
					set_active_target(vehicle);
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
