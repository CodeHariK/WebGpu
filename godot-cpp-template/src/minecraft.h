#ifndef MinecraftNode_CLASS
#define MinecraftNode_CLASS

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>

#include <unordered_map>
#include <vector>

using namespace godot;

enum BlockType : uint8_t {
	AIR = 0,
	DIRT = 1,
	GRASS = 2,
	STONE = 3,
	WOOD = 4,
	LEAVES = 5,
};

class Chunk {
public:
	static const int CHUNK_SIZE = 16; // 16x16x16 blocks per chunk
	static const int CHUNK_HEIGHT = 256;

private:
	// Use a flat 3D array for block storage
	std::vector<uint8_t> blocks; // Using uint8_t to store block types efficiently
	Vector3i chunk_position; // Chunk position in world coordinates
	bool is_dirty; // Flag for mesh regeneration

public:
	Chunk(const Vector3i &pos) :
			chunk_position(pos) {
		blocks.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_HEIGHT, 0);
	}

	uint8_t get_block(int x, int y, int z) const {
		return blocks[x + z * CHUNK_SIZE + y * CHUNK_SIZE * CHUNK_SIZE];
	}

	void set_block(int x, int y, int z, uint8_t type) {
		blocks[x + z * CHUNK_SIZE + y * CHUNK_SIZE * CHUNK_SIZE] = type;
		is_dirty = true;
	}
};

class MinecraftNode : public Node3D {
	GDCLASS(MinecraftNode, Node3D);

private:
	int terrain_width = 32;
	int terrain_depth = 32;
	float terrain_scale = 0.15f;
	float terrain_height_scale = 6.0f;
	Ref<Material> terrain_material;

	bool terrain_dirty = false;

	void generate_terrain();

	///////
	std::unordered_map<Vector3i, Chunk *> chunks;
	int render_distance = 8; // Number of chunks to render in each direction

	// Helper function to get chunk coordinates from world coordinates
	Vector3i world_to_chunk_coords(const Vector3 &position) {
		return Vector3i(
				floor(position.x / Chunk::CHUNK_SIZE),
				floor(position.y / Chunk::CHUNK_HEIGHT),
				floor(position.z / Chunk::CHUNK_SIZE));
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
	}

public:
	MinecraftNode();
	~MinecraftNode();

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
