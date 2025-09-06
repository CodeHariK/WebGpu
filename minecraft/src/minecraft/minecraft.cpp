#include "minecraft.h"

#include "godot_cpp/classes/node.hpp"
#include <godot_cpp/classes/engine.hpp>

#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/viewport.hpp>

#include "godot_cpp/classes/button.hpp"
#include "godot_cpp/classes/slider.hpp"

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

		heights = generate_terrain_heights(terrain_width, terrain_depth, terrain_scale, terrain_height_scale);

		generate_terrain(Vector3(0, -.1, 0));
		generate_voxel_terrain(Vector3(0, 0, 0));
		generate_voxel_terrain2(Vector3(0, .1, 0));
		terrain_dirty = false;
	}
}

void MinecraftNode::_ready() {
	camera = get_viewport()->get_camera_3d();
	if (!camera) {
		UtilityFunctions::print("No active camera found!");
	}

	String ui_root = "/root/World/UiMinecraft/";
	ui_accordion.header = Object::cast_to<Button>(get_node_or_null(ui_root + "TerrainProperties/TerrainHeaderButton"));
	ui_accordion.slider = Object::cast_to<Slider>(get_node_or_null(ui_root + "TerrainProperties/TerrainContent/TerrainSizeSlider"));
	ui_accordion.content = Object::cast_to<Control>(get_node_or_null(ui_root + "TerrainProperties/TerrainContent"));
	if (ui_accordion.is_valid()) {
		ui_accordion.header->connect("pressed", Callable(this, "_on_accordion_header_pressed"));
		ui_accordion.slider->connect("value_changed", Callable(this, "_on_slider_value_changed"));
	}
}
