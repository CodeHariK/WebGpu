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
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/slider.hpp>

#include <godot_cpp/classes/tween.hpp>

using namespace godot;

struct MinecraftUI {
	ProgressBar *health_bar = nullptr;
	Control *content = nullptr;
	Button *header_button = nullptr;
	Slider *terrain_slider = nullptr;

	bool is_valid() const {
		return health_bar != nullptr &&
				content != nullptr &&
				header_button != nullptr &&
				terrain_slider != nullptr;
	}
};

class MinecraftNode : public Node3D {
	GDCLASS(MinecraftNode, Node3D);

private:
	int terrain_len_x = 64;
	int terrain_len_z = 64;
	float terrain_freq = 0.05f;
	float terrain_height = 12.0f;
	Ref<Material> terrain_material;
	Ref<Curve> height_curve;
	Ref<Tween> tween;

	Camera3D *camera;

	bool terrain_dirty = false;

	MinecraftUI ui;

	PackedInt32Array heights;
	PackedInt32Array generate_terrain_heights(bool height_curve_sampling);
	inline int get_height(int x, int z) {
		return heights[x + z * terrain_len_x];
	}

	Ref<ArrayMesh> build_chunk_mesh(int size);
	void generate_terrain(String name, Vector3 pos);
	void generate_cube_terrain(String name, Vector3 pos);
	void generate_chunked_terrain(String name, Vector3 pos);

	void loadBlendFile(String path);
	void minHeapTest();
	void jsonTest(String path);

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

		ClassDB::bind_method(D_METHOD("set_height_curve", "curve"), &MinecraftNode::set_height_curve);
		ClassDB::bind_method(D_METHOD("get_height_curve"), &MinecraftNode::get_height_curve);
		ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "height_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_height_curve", "get_height_curve");

		ClassDB::bind_method(D_METHOD("ui_on_header_button_pressed"), &MinecraftNode::ui_on_header_button_pressed);
		ClassDB::bind_method(D_METHOD("ui_on_terrain_slider_change", "value"), &MinecraftNode::ui_on_terrain_slider_change);
		ClassDB::bind_method(D_METHOD("ui_on_header_mouse_entered"), &MinecraftNode::ui_on_header_mouse_entered);
		ClassDB::bind_method(D_METHOD("ui_on_header_mouse_exited"), &MinecraftNode::ui_on_header_mouse_exited);

		ClassDB::bind_method(D_METHOD("delaunator_test"), &MinecraftNode::DelaunatorTest);
		ClassDB::bind_method(D_METHOD("voronoi_test"), &MinecraftNode::VoronoiTest);
	}

public:
	MinecraftNode();
	~MinecraftNode();

	void _process(double delta) override;
	void _ready() override;

	void setup_ui();
	void ui_on_header_button_pressed();
	void ui_on_header_mouse_entered();
	void ui_on_header_mouse_exited();
	void ui_on_terrain_slider_change(double value);

	godot::Dictionary DelaunatorTest();
	godot::Array VoronoiTest();

	void set_terrain_width(int w) {
		terrain_len_x = w;
		terrain_dirty = true;
	}
	int get_terrain_width() const { return terrain_len_x; }

	void set_terrain_depth(int d) {
		terrain_len_z = d;
		terrain_dirty = true;
	}
	int get_terrain_depth() const { return terrain_len_z; }

	void set_terrain_scale(float s) {
		terrain_freq = s;
		terrain_dirty = true;
	}
	float get_terrain_scale() const { return terrain_freq; }

	void set_terrain_height_scale(float hs) {
		terrain_height = hs;
		terrain_dirty = true;
	}
	float get_terrain_height_scale() const { return terrain_height; }

	void set_terrain_material(const Ref<Material> &mat) {
		terrain_material = mat;
		terrain_dirty = true;
	}
	Ref<Material> get_terrain_material() const { return terrain_material; }

	void set_height_curve(const Ref<Curve> &c) {
		height_curve = c;
		terrain_dirty = true;
	}
	Ref<Curve> get_height_curve() const { return height_curve; }
};

#endif
