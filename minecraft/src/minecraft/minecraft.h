#ifndef MinecraftNode_CLASS
#define MinecraftNode_CLASS

#include "godot_cpp/classes/array_mesh.hpp"
#include "godot_cpp/classes/global_constants.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/core/property_info.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/vector3.hpp"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/slider.hpp>

using namespace godot;

struct AccordionUI {
	Control *content = nullptr;
	Button *header = nullptr;
	Slider *slider = nullptr;

	bool is_valid() const {
		return content != nullptr && header != nullptr && slider != nullptr;
	}
};

class MinecraftNode : public Node3D {
	GDCLASS(MinecraftNode, Node3D);

private:
	int terrain_width = 64;
	int terrain_depth = 64;
	float terrain_scale = 0.05f;
	float terrain_height_scale = 12.0f;
	Ref<Material> terrain_material;

	Camera3D *camera;

	bool terrain_dirty = false;

	AccordionUI ui_accordion;

	PackedInt32Array heights;
	PackedInt32Array generate_terrain_heights(int width, int depth, float scale, float height_scale);
	inline int get_height(int x, int z) {
		return heights[x + z * terrain_width];
	}

	Ref<ArrayMesh> build_chunk_mesh(int size);
	void generate_terrain(Vector3 pos);
	void generate_voxel_terrain(Vector3 pos);
	void generate_voxel_terrain2(Vector3 pos);

	void blendTest();
	void minHeapTest();
	void jsonTest();

	void raycast();

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_terrain_width", "width"), &MinecraftNode::set_terrain_width);
		ClassDB::bind_method(D_METHOD("get_terrain_width"), &MinecraftNode::get_terrain_width);
		ADD_PROPERTY(PropertyInfo(Variant::INT, "terrain_width"), "set_terrain_width", "get_terrain_width");

		ClassDB::bind_method(D_METHOD("set_terrain_depth", "depth"), &MinecraftNode::set_terrain_depth);
		ClassDB::bind_method(D_METHOD("get_terrain_depth"), &MinecraftNode::get_terrain_depth);
		ADD_PROPERTY(PropertyInfo(Variant::INT, "terrain_depth"), "set_terrain_depth", "get_terrain_depth");

		ClassDB::bind_method(D_METHOD("set_terrain_scale", "scale"), &MinecraftNode::set_terrain_scale);
		ClassDB::bind_method(D_METHOD("get_terrain_scale"), &MinecraftNode::get_terrain_scale);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "terrain_scale"), "set_terrain_scale", "get_terrain_scale");

		ClassDB::bind_method(D_METHOD("set_terrain_height_scale", "height_scale"), &MinecraftNode::set_terrain_height_scale);
		ClassDB::bind_method(D_METHOD("get_terrain_height_scale"), &MinecraftNode::get_terrain_height_scale);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "terrain_height_scale"), "set_terrain_height_scale", "get_terrain_height_scale");

		ClassDB::bind_method(D_METHOD("set_terrain_material", "material"), &MinecraftNode::set_terrain_material);
		ClassDB::bind_method(D_METHOD("get_terrain_material"), &MinecraftNode::get_terrain_material);
		ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "terrain_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_terrain_material", "get_terrain_material");

		ClassDB::bind_method(D_METHOD("_on_accordion_header_pressed"), &MinecraftNode::_on_accordion_header_pressed);
		ClassDB::bind_method(D_METHOD("_on_slider_value_changed", "value"), &MinecraftNode::_on_slider_value_changed);
	}

public:
	MinecraftNode();
	~MinecraftNode();

	void _process(double delta) override;
	void _ready() override;

	void _on_accordion_header_pressed() {
		bool new_visible = !ui_accordion.content->is_visible();
		ui_accordion.content->set_visible(new_visible);
		ui_accordion.header->set_text(new_visible ? "Hide" : "Show");
	}

	void _on_slider_value_changed(double value) {
		terrain_width = (int)value;
		terrain_depth = (int)value;
		terrain_dirty = true;
	}

	void set_terrain_width(int w) {
		terrain_width = w;
		terrain_dirty = true;
	}
	int get_terrain_width() const { return terrain_width; }

	void set_terrain_depth(int d) {
		terrain_depth = d;
		terrain_dirty = true;
	}
	int get_terrain_depth() const { return terrain_depth; }

	void set_terrain_scale(float s) {
		terrain_scale = s;
		terrain_dirty = true;
	}
	float get_terrain_scale() const { return terrain_scale; }

	void set_terrain_height_scale(float hs) {
		terrain_height_scale = hs;
		terrain_dirty = true;
	}
	float get_terrain_height_scale() const { return terrain_height_scale; }

	void set_terrain_material(const Ref<Material> &mat) {
		terrain_material = mat;
		terrain_dirty = true;
	}
	Ref<Material> get_terrain_material() const { return terrain_material; }
};

#endif
