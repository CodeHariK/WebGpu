#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include "player_input.h"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class MCManager;
class ArcadeVehicle;
class PhysicsCharacter3D;
class CelesteController;
class GameCamera;
class PlayerInput;

class GameManager : public Node {
	GDCLASS(GameManager, Node)

private:
	static GameManager *singleton;

	// Tracked managers
	MCManager *mc_manager = nullptr;

	GameCamera *main_camera = nullptr;
	PlayerInput *player_input = nullptr;

	ArcadeVehicle *vehicle = nullptr;
	PhysicsCharacter3D *character = nullptr;
	CelesteController *celeste_character = nullptr;
	Node *active_target = nullptr;

protected:
	static void _bind_methods();

public:
	GameManager();
	~GameManager();

	static GameManager *get_singleton();

	void _enter_tree() override;
	void _exit_tree() override;

	// Registry methods
	void register_mc_manager(MCManager *p_manager);
	MCManager *get_mc_manager() const;

	void register_vehicle(ArcadeVehicle *p_vehicle);
	ArcadeVehicle *get_vehicle() const;

	void register_character(PhysicsCharacter3D *p_character);
	PhysicsCharacter3D *get_character() const;

	void register_celeste_controller(Node *p_character);
	Node *get_celeste_controller() const;

	void register_camera(GameCamera *p_camera);
	GameCamera *get_camera() const;

	PlayerInput *get_player_input() const { return player_input; }

	void set_active_target(Node *p_target);
	Node *get_active_target() const;

	void _physics_process(double delta) override;
	void _input(const Ref<InputEvent> &p_event) override;

	// Global Persistence logic
	void save_game(const String &p_slot_name);
	void load_game(const String &p_slot_name);
};

} // namespace godot

#endif // GAME_MANAGER_H
