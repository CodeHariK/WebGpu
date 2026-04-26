#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "camera/camera.h"

#include "character/physics_character.h"

#include "cui/cui.h"
#include "cui/cui_line_graph.h"

#include "debug_draw/debug_manager.h"
#include "debug_draw/debug_quad.h"

#include "game_manager/game_manager.h"
#include "game_manager/player_input.h"

#include "marching_cubes/mc.h"
#include "marching_cubes/mc_grid.h"
#include "marching_cubes/mc_manager.h"

#include "player/celeste_controller.h"
#include "player/celeste_ui.h"

#include "terrain/minecraft.h"

#include "vehicle/arcade_vehicle.h"
#include "vehicle/config/vehicle_config.h"
#include "vehicle/config/wheel_config.h"

#include "enemy/enemy_base.h"
#include "enemy/enemy_manager.h"
#include "enemy/ground/turret/turret_enemy.h"
#include "enemy/ground/patrol_melee/patrol_melee_enemy.h"

#include "ai/blackboard.h"
#include "ai/bt_composites.h"
#include "ai/bt_leaves.h"
#include "ai/bt_task.h"
#include "ai/bt_decorators.h"
#include "ai/bt_player.h"

namespace godot {

void initialize_gdextension_types(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(MinecraftNode);

	GDREGISTER_CLASS(MCNode);
	GDREGISTER_CLASS(MCManager);
	GDREGISTER_CLASS(MCGrid);

	GDREGISTER_CLASS(CUI);
	GDREGISTER_CLASS(CUILineGraph);

	GDREGISTER_CLASS(GameCamera);

	GDREGISTER_CLASS(WheelConfig);
	GDREGISTER_CLASS(VehicleConfig);
	GDREGISTER_CLASS(ArcadeVehicle);

	GDREGISTER_CLASS(DebugLineQuad);
	GDREGISTER_CLASS(DebugManager);

	GDREGISTER_CLASS(GameManager);
	GDREGISTER_CLASS(PlayerInput);

	GDREGISTER_CLASS(PhysicsCharacter3D);
	GDREGISTER_CLASS(CelesteController);

	GDREGISTER_CLASS(EnemyManager);
	GDREGISTER_CLASS(EnemyBase);
	GDREGISTER_CLASS(TurretEnemy);
	GDREGISTER_CLASS(PatrolMeleeEnemy);

	GDREGISTER_CLASS(Blackboard);
	GDREGISTER_CLASS(BTTask);
	GDREGISTER_CLASS(BTComposite);
	GDREGISTER_CLASS(BTSequence);
	GDREGISTER_CLASS(BTSelector);
	GDREGISTER_CLASS(BTRandomSelector);
	GDREGISTER_CLASS(BTRandomSequence);
	GDREGISTER_CLASS(BTReactiveSelector);
	GDREGISTER_CLASS(BTReactiveSequence);
	GDREGISTER_CLASS(BTWait);
	GDREGISTER_CLASS(BTIsInRange);
	GDREGISTER_CLASS(BTActionShoot);
	GDREGISTER_CLASS(BTActionApproach);
	GDREGISTER_CLASS(BTPatrol);
	GDREGISTER_CLASS(BTActionMelee);
	GDREGISTER_CLASS(BTDecorator);
	GDREGISTER_CLASS(BTInverter);
	GDREGISTER_CLASS(BTForceSuccess);
	GDREGISTER_CLASS(BTForceFailure);
	GDREGISTER_CLASS(BTProbability);
	GDREGISTER_CLASS(BTRepeat);
	GDREGISTER_CLASS(BTRepeatUntilSuccess);
	GDREGISTER_CLASS(BTRepeatUntilFailure);
	GDREGISTER_CLASS(BTDelay);
	GDREGISTER_CLASS(BTCooldown);
	GDREGISTER_CLASS(BTRunLimit);
	GDREGISTER_CLASS(BTTimeLimit);
	GDREGISTER_CLASS(BTPlayer);
}

void uninitialize_gdextension_types(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {
// Initialization
GDExtensionBool GDE_EXPORT minecraft_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
	init_obj.register_initializer(initialize_gdextension_types);
	init_obj.register_terminator(uninitialize_gdextension_types);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
} //namespace godot
