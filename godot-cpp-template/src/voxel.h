#ifndef VoxelNode_CLASS
#define VoxelNode_CLASS

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>

using namespace godot;

class VoxelNode : public Node3D {
	GDCLASS(VoxelNode, Node3D);

private:
	int terrain_width = 32;
	int terrain_depth = 32;
	float terrain_scale = 0.15f;
	float terrain_height_scale = 6.0f;
	Ref<Material> terrain_material;

	bool terrain_dirty = false;

	void blendTest();
	void make_triangle();
	void make_quad();
	void generate_terrain();

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_terrain_width", "width"), &VoxelNode::set_terrain_width);
		ClassDB::bind_method(D_METHOD("get_terrain_width"), &VoxelNode::get_terrain_width);
		ADD_PROPERTY(PropertyInfo(Variant::INT, "terrain_width"), "set_terrain_width", "get_terrain_width");

		ClassDB::bind_method(D_METHOD("set_terrain_depth", "depth"), &VoxelNode::set_terrain_depth);
		ClassDB::bind_method(D_METHOD("get_terrain_depth"), &VoxelNode::get_terrain_depth);
		ADD_PROPERTY(PropertyInfo(Variant::INT, "terrain_depth"), "set_terrain_depth", "get_terrain_depth");

		ClassDB::bind_method(D_METHOD("set_terrain_scale", "scale"), &VoxelNode::set_terrain_scale);
		ClassDB::bind_method(D_METHOD("get_terrain_scale"), &VoxelNode::get_terrain_scale);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "terrain_scale"), "set_terrain_scale", "get_terrain_scale");

		ClassDB::bind_method(D_METHOD("set_terrain_height_scale", "height_scale"), &VoxelNode::set_terrain_height_scale);
		ClassDB::bind_method(D_METHOD("get_terrain_height_scale"), &VoxelNode::get_terrain_height_scale);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "terrain_height_scale"), "set_terrain_height_scale", "get_terrain_height_scale");

		ClassDB::bind_method(D_METHOD("set_terrain_material", "material"), &VoxelNode::set_terrain_material);
		ClassDB::bind_method(D_METHOD("get_terrain_material"), &VoxelNode::get_terrain_material);
		ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "terrain_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_terrain_material", "get_terrain_material");
	}

public:
	VoxelNode();
	~VoxelNode();

	void _process(double delta) override;

	void _ready() override;

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
