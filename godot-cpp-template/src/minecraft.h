#ifndef MinecraftNode_CLASS
#define MinecraftNode_CLASS

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>

#include <unordered_map>
#include <vector>

using namespace godot;

struct Vector3iHash {
	std::size_t operator()(const godot::Vector3i &v) const noexcept {
		// Simple but effective hash combine
		std::size_t h1 = std::hash<int>()(v.x);
		std::size_t h2 = std::hash<int>()(v.y);
		std::size_t h3 = std::hash<int>()(v.z);
		return h1 ^ (h2 << 1) ^ (h3 << 2);
	}
};

struct Vector3iEqual {
	bool operator()(const godot::Vector3i &a, const godot::Vector3i &b) const noexcept {
		return a.x == b.x && a.y == b.y && a.z == b.z;
	}
};

enum VoxelType : uint8_t {
	AIR = 0,
	SOIL = 1,
	GRASS = 2,
	STONE = 3,
	WATER = 4,
};

class Chunk {
public:
	static const int WIDTH = 16;
	static const int HEIGHT = 4;

private:
	// Use a flat 3D array for block storage
	std::vector<uint8_t> blocks; // Using uint8_t to store block types efficiently
	Vector3i chunk_position; // Chunk position in world coordinates

public:
	Chunk(const Vector3i &pos) :
			chunk_position(pos) {
		blocks.resize(WIDTH * WIDTH * HEIGHT, 0);
	}

	uint8_t get_block(int x, int y, int z) const {
		return blocks[x + z * WIDTH + y * WIDTH * WIDTH];
	}

	void set_block(int x, int y, int z, uint8_t type) {
		blocks[x + z * WIDTH + y * WIDTH * WIDTH] = type;
	}
};

class MinecraftNode : public Node3D {
	GDCLASS(MinecraftNode, Node3D);

private:
	int terrain_width = 128;
	int terrain_depth = 128;
	float terrain_scale = 0.15f;
	float terrain_height_scale = 6.0f;
	Ref<Material> terrain_material;

	bool terrain_dirty = false;

	void generate_terrain(Vector3 pos);
	void generate_voxel_terrain(Vector3 pos);

	///////
	std::unordered_map<Vector3i, Chunk *, Vector3iHash, Vector3iEqual> chunks;
	int render_distance = 8; // Number of chunks to render in each direction

	// Helper function to get chunk coordinates from world coordinates
	Vector3i world_to_chunk_coords(const Vector3 &position) {
		return Vector3i(
				floor(position.x / Chunk::WIDTH),
				floor(position.y / Chunk::HEIGHT),
				floor(position.z / Chunk::WIDTH));
	}

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
		Node *content_node = get_node_or_null("/root/World/CanvasLayer/AccordionRoot/AccordionContent");
		Node *header_node = get_node_or_null("/root/World/CanvasLayer/AccordionRoot/AccordionHeader");

		if (content_node && header_node) {
			Control *content = Object::cast_to<Control>(content_node);
			Button *header = Object::cast_to<Button>(header_node);

			if (content && header) {
				bool new_visible = !content->is_visible();
				content->set_visible(new_visible);

				header->set_text(new_visible ? "Hide" : "Show");
			}
		}
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
