
#include "minecraft.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <godot_cpp/classes/surface_tool.hpp>

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>

using namespace godot;

MinecraftNode::MinecraftNode() {
	set_process_mode(Node::ProcessMode::PROCESS_MODE_ALWAYS);
}

MinecraftNode::~MinecraftNode() {
}

void MinecraftNode::_process(double delta) {
	if (terrain_dirty) {
		generate_terrain();
		terrain_dirty = false;
	}
}

void MinecraftNode::_ready() {
}

void MinecraftNode::generate_terrain() {
	UtilityFunctions::print("Generated noise terrain mesh!");
}
