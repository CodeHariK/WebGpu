#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "camera/camera.h"
#include "character/physics_character.h"
#include "cui/cui.h"
#include "debug_draw/debug_manager.h"
#include "debug_draw/debug_quad.h"
#include "game_manager/game_manager.h"
#include "game_manager/player_input.h"
#include "marching_cubes/mc.h"
#include "marching_cubes/mc_grid.h"
#include "marching_cubes/mc_manager.h"
#include "player/celeste_controller.h"
#include "terrain/minecraft.h"
#include "terrain/wfc.h"
#include "vehicle/arcade_vehicle.h"
#include "vehicle/config/vehicle_config.h"
#include "vehicle/config/wheel_config.h"

namespace godot {

void initialize_gdextension_types(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(MinecraftNode);
	GDREGISTER_CLASS(WFCGenerator3D);

	GDREGISTER_CLASS(MCNode);
	GDREGISTER_CLASS(MCManager);
	GDREGISTER_CLASS(MCGrid);

	GDREGISTER_CLASS(CUI);

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
