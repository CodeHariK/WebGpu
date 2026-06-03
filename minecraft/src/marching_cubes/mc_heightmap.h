/**
 * @file mc_heightmap.h
 * @brief Node for generating 2.5D Marching Squares terrain using Marching Cubes meshes.
 * Module Path: src/marching_cubes/mc_heightmap.h
 * Build Dependencies: godot-cpp, mc.h
 */

#ifndef MC_HEIGHTMAP_H
#define MC_HEIGHTMAP_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/noise.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace godot {

class MCNode;

class MCHeightmap : public Node3D {
	GDCLASS(MCHeightmap, Node3D)

private:
	// Raw heightmap data containing grid_size.x * grid_size.y byte height values.
	PackedByteArray heightmap_data;

	// Reference to the MCNode mesh library provider.
	MCNode *mc_node = nullptr;

	// Size of each voxel grid unit horizontally and vertically.
	float cell_spacing = 1.0f;

	// Scale factor applied to height values.
	float height_scale = 1.0f;

	// Dimensions of the 2D heightmap.
	Vector2i grid_size = Vector2i(16, 16);

	// Toggles visibility of tiny debug spheres at heightmap corner elevations.
	bool show_debug_spheres = false;

	// Node container holding all debug sphere instances.
	Node3D *debug_spheres_container = nullptr;

	// Noise resource used to generate procedural height elevations.
	Ref<Noise> noise_source;

	// Internal helper to determine if a corner is active at a given coordinates.
	bool _is_corner_active(int x, int y, int z, const uint8_t *data_ptr) const;

	// Internal helper to calculate the marching cubes case hash for a cell.
	uint8_t _get_cell_hash(int x, int y, int z, const uint8_t *data_ptr) const;

	// Internal helper to clear all dynamically spawned mesh instances.
	void _clear_terrain_meshes();

	// Internal helper to spawn a single marching cubes cell mesh at the specified location.
	void _spawn_cell_mesh(int x, int y, int z, uint8_t hash);

	// Spawns debug spheres at heightmap corner elevations.
	void _spawn_debug_spheres(const uint8_t *data_ptr);

protected:
	// Exposes variables and methods to Godot's class system.
	static void _bind_methods();

public:
	MCHeightmap();
	~MCHeightmap() override;

	// Getter for heightmap data property.
	PackedByteArray get_heightmap_data() const { return heightmap_data; }

	// Setter for heightmap data property.
	void set_heightmap_data(const PackedByteArray &p_data) { heightmap_data = p_data; }

	// Getter for mc_node property.
	MCNode *get_mc_node() const { return mc_node; }

	// Setter for mc_node property.
	void set_mc_node(MCNode *p_node) { mc_node = p_node; }

	// Getter for cell_spacing property.
	float get_cell_spacing() const { return cell_spacing; }

	// Setter for cell_spacing property.
	void set_cell_spacing(float p_spacing) { cell_spacing = p_spacing; }

	// Getter for height_scale property.
	float get_height_scale() const { return height_scale; }

	// Setter for height_scale property.
	void set_height_scale(float p_scale) { height_scale = p_scale; }

	// Getter for grid_size property.
	Vector2i get_grid_size() const { return grid_size; }

	// Setter for grid_size property.
	void set_grid_size(const Vector2i &p_size) { grid_size = p_size; }

	// Getter for show_debug_spheres property.
	bool get_show_debug_spheres() const { return show_debug_spheres; }

	// Setter for show_debug_spheres property.
	void set_show_debug_spheres(bool p_show) { show_debug_spheres = p_show; }

	// Getter for noise_source property.
	Ref<Noise> get_noise_source() const { return noise_source; }

	// Setter for noise_source property.
	void set_noise_source(const Ref<Noise> &p_noise);

	// Regenerates noise and updates the terrain mesh.
	void initialize_random_noise_and_generate();

	// Generates the 2.5D terrain meshes based on the heightmap data.
	void generate_terrain();

	// Clears generated terrain meshes.
	void clear_terrain();

	// Initializes the heightmap with random Perlin-like noise.
	void initialize_random_noise();

	// Called when the node enters the scene tree.
	void _ready() override;
};

} // namespace godot

#endif // MC_HEIGHTMAP_H
