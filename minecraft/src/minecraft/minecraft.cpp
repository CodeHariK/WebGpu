#include "minecraft.h"

#include "godot_cpp/classes/node.hpp"
#include <godot_cpp/classes/engine.hpp>

#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/viewport.hpp>

#include <chrono>

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
		String children = String::num_int64(get_child_count());
		for (int i = 0; i < get_child_count(); i++) {
			Node *c = get_child(i);
			children += ", " + c->get_name();
		}
		UtilityFunctions::print(children);

		heights = generate_terrain_heights(false);
		generate_terrain("HTerrain", Vector3(0, 0, 0));
		generate_cube_terrain("CTerrain", Vector3(100, 0, 0));
		generate_chunked_terrain("KTerrain", Vector3(200, 0, 0));

		heights = generate_terrain_heights(true);
		generate_terrain("SHTerrain", Vector3(0, 0, 100));
		generate_cube_terrain("SCTerrain", Vector3(100, 0, 100));
		generate_chunked_terrain("SKTerrain", Vector3(200, 0, 100));

		terrain_dirty = false;
	}
}

void MinecraftNode::_ready() {
	camera = get_viewport()->get_camera_3d();
	if (!camera) {
		UtilityFunctions::print("No active camera found!");
	}

	using namespace std::chrono;

	auto start = high_resolution_clock::now();

	DelaunatorTest();

	auto end = high_resolution_clock::now();
	auto duration = duration_cast<milliseconds>(end - start).count();

	UtilityFunctions::print("DelaunatorTest took: ", duration, " ms");

	start = high_resolution_clock::now();

	VoronoiTest();

	end = high_resolution_clock::now();
	duration = duration_cast<milliseconds>(end - start).count();

	UtilityFunctions::print("VoronoiTest took: ", duration, " ms");

	setup_ui();
}
