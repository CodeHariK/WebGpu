#include "minecraft.h"

#include "godot_cpp/classes/node.hpp"
#include <godot_cpp/classes/engine.hpp>

#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/viewport.hpp>

using namespace godot;

MinecraftNode::MinecraftNode() {
	set_process_mode(Node::ProcessMode::PROCESS_MODE_INHERIT);
}

MinecraftNode::~MinecraftNode() {
	if (Engine::get_singleton()->is_editor_hint()) {
		set_process_mode(Node::ProcessMode::PROCESS_MODE_DISABLED);
	}
}

void MinecraftNode::_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (terrain_dirty) {
		generate_smooth_terrain("HTerrain");
		// generate_cube_terrain("CTerrain", Vector3(100, 0, 0));
		// generate_voxel_terrain("KTerrain");

		// generate_terrain("SHTerrain", Vector3(0, 0, 100));
		// generate_cube_terrain("SCTerrain", Vector3(100, 0, 100));
		// generate_chunked_terrain("SKTerrain", Vector3(200, 0, 100));

		terrain_dirty = false;
	}
}

void MinecraftNode::_ready() {
	camera = get_viewport()->get_camera_3d();
	if (!camera) {
		UtilityFunctions::print("No active camera found!");
	}

	setup_ui();

	// minHeapTest();
}
