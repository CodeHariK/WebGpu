#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "acnh/acnh.h"
#include "cui/cui.h"
#include "marching_cubes/mc.h"
#include "marching_cubes/mc_grid.h"
#include "marching_cubes/mc_manager.h"
#include "minecraft/minecraft.h"
#include "minecraft/wfc.h"

namespace godot {

void initialize_gdextension_types(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(MinecraftNode);
	GDREGISTER_CLASS(WFCGenerator3D);
	GDREGISTER_CLASS(ACNHNode);
	GDREGISTER_CLASS(MCNode);
	GDREGISTER_CLASS(MCManager);
	GDREGISTER_CLASS(MCGrid);
	GDREGISTER_CLASS(CUI);
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
