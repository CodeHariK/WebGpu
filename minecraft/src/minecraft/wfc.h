#ifndef WFC
#define WFC

#include "godot_cpp/classes/global_constants.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/core/memory.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/core/property_info.hpp"
#include "godot_cpp/templates/vector.hpp"
#include <cstdint>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3i.hpp>

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <random>
#include <stack>

using namespace godot;

// A simple struct to hold all data for a single tile prototype.
struct WFCTile {
	int id;
	godot::Ref<godot::Mesh> mesh;

	// Hashes for connectivity.
	// Indices correspond to directions: 0:+X, 1:-X, 2:+Y, 3:-Y, 4:+Z, 5:-Z
	int hashes[6];
};

// Represents a single cell in the WFC grid.
class WFCCell {
public:
	bool collapsed = false;
	godot::Vector<int> possible_tiles;

	WFCCell() = default;

	void initialize(int total_tile_count) {
		collapsed = false;
		possible_tiles.resize(total_tile_count);
		int *possible_tiles_ptr = possible_tiles.ptrw();
		for (int i = 0; i < total_tile_count; ++i) {
			possible_tiles_ptr[i] = i;
		}
	}
};

class WFCGenerator3D : public Node3D {
	GDCLASS(WFCGenerator3D, Node3D)

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("generate"), &WFCGenerator3D::generate);

		ClassDB::bind_method(D_METHOD("set_grid_size", "size"), &WFCGenerator3D::set_grid_size);
		ClassDB::bind_method(D_METHOD("get_grid_size"), &WFCGenerator3D::get_grid_size);
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "grid_size"), "set_grid_size", "get_grid_size");

		ClassDB::bind_method(D_METHOD("set_tile_definitions", "definitions"), &WFCGenerator3D::set_tile_definitions);
		ClassDB::bind_method(D_METHOD("get_tile_definitions"), &WFCGenerator3D::get_tile_definitions);
		ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "tile_definitions", PROPERTY_HINT_ARRAY_TYPE, "Dictionary"), "set_tile_definitions", "get_tile_definitions");
	}

private:
	Vector3i grid_size = Vector3i(10, 2, 10);
	Array tile_definitions;

	Vector<WFCTile> tile_prototypes;
	Vector<WFCCell> grid;
	int collapsed_count = 0;
	Dictionary compatibility_map;

	void clear_scene() {
		for (int i = get_child_count() - 1; i >= 0; --i) {
			Node *child = get_child(i);
			child->queue_free();
		}
	}

	bool parse_tile_definitions() {
		tile_prototypes.clear();
		compatibility_map.clear();
		ResourceLoader *rl = ResourceLoader::get_singleton();

		for (int i = 0; i < tile_definitions.size(); ++i) {
			Dictionary tile_dict = tile_definitions[i];
			if (!tile_dict.has("obj") || !tile_dict.has("hash_PX")) {
				UtilityFunctions::printerr("Tile definition at index ", i, " is missing required keys ('obj', 'hash_PX', etc).");
				return false;
			}

			WFCTile tile;
			tile.id = i;

			String mesh_path = tile_dict["obj"];
			tile.mesh = rl->load(mesh_path);
			if (tile.mesh.is_null()) {
				UtilityFunctions::printerr("Failed to load mesh at path: ", mesh_path);
				return false;
			}

			tile.hashes[0] = tile_dict["hash_PX"];
			tile.hashes[1] = tile_dict["hash_NX"];
			tile.hashes[2] = tile_dict["hash_PY"];
			tile.hashes[3] = tile_dict["hash_NY"];
			tile.hashes[4] = tile_dict["hash_PZ"];
			tile.hashes[5] = tile_dict["hash_NZ"];

			tile_prototypes.push_back(tile);
		}

		if (tile_prototypes.is_empty()) {
			return false;
		}

		// --- Pre-calculate the compatibility map ---
		// For each hash and each direction, find all tiles that have that hash on that face.
		for (int i = 0; i < tile_prototypes.size(); ++i) {
			const WFCTile &tile = tile_prototypes[i];
			for (int dir = 0; dir < 6; ++dir) {
				// Key: (hash, direction)
				int64_t key = (int64_t(tile.hashes[dir]) << 3) + dir;

				if (!compatibility_map.has(key)) {
					compatibility_map[key] = Array();
				}
				Array arr = compatibility_map[key];
				arr.push_back(i); // Add tile_id to the list for this hash/direction pair
			}
		}
		UtilityFunctions::print("Hash-to-tile compatibility map generated.");

		return !tile_prototypes.is_empty();
	}

	void initialize_grid() {
		int total_cells = grid_size.x * grid_size.y * grid_size.z;
		grid.resize(total_cells);
		WFCCell *grid_ptr = grid.ptrw();
		for (int i = 0; i < total_cells; ++i) {
			grid_ptr[i].initialize(tile_prototypes.size());
		}
		collapsed_count = 0;
	}

	int get_cell_index(const Vector3i &p_pos) const {
		return p_pos.x + p_pos.y * grid_size.x + p_pos.z * grid_size.x * grid_size.y;
	}

	bool is_in_bounds(const Vector3i &p_pos) const {
		return p_pos.x >= 0 && p_pos.x < grid_size.x &&
				p_pos.y >= 0 && p_pos.y < grid_size.y &&
				p_pos.z >= 0 && p_pos.z < grid_size.z;
	}

	int find_lowest_entropy_cell() {
		int min_entropy = tile_prototypes.size() + 1;
		Vector<int> lowest_entropy_cells;

		const WFCCell *grid_ptr = grid.ptr();
		for (int64_t i = 0; i < grid.size(); ++i) {
			if (!grid_ptr[i].collapsed) {
				int entropy = grid_ptr[i].possible_tiles.size();
				if (entropy > 0 && entropy < min_entropy) {
					min_entropy = entropy;
					lowest_entropy_cells.clear();
					lowest_entropy_cells.push_back(i);
				} else if (entropy == min_entropy) {
					lowest_entropy_cells.push_back(i);
				}
			}
		}

		if (lowest_entropy_cells.is_empty()) {
			return -1; // All cells collapsed or contradiction
		}

		// Pick one randomly from the cells with the lowest entropy
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> distrib(0, lowest_entropy_cells.size() - 1);
		return lowest_entropy_cells[distrib(gen)];
	}

	bool propagate(const Vector3i &p_initial_pos) {
		std::stack<Vector3i> propagation_stack;
		propagation_stack.push(p_initial_pos);

		const Vector3i directions[] = {
			Vector3i(1, 0, 0), Vector3i(-1, 0, 0),
			Vector3i(0, 1, 0), Vector3i(0, -1, 0),
			Vector3i(0, 0, 1), Vector3i(0, 0, -1)
		};
		// Opposite direction indices: 0->1, 1->0, 2->3, 3->2, etc.
		const int opposite_dir_idx[] = { 1, 0, 3, 2, 5, 4 };

		while (!propagation_stack.empty()) {
			Vector3i current_pos = propagation_stack.top();
			propagation_stack.pop();

			WFCCell *grid_ptrw = grid.ptrw();

			int current_idx = get_cell_index(current_pos);
			const Vector<int> &possible_current_tiles = grid_ptrw[current_idx].possible_tiles;

			// For each neighbor
			for (int i = 0; i < 6; ++i) {
				Vector3i neighbor_pos = current_pos + directions[i];
				if (!is_in_bounds(neighbor_pos))
					continue;

				int neighbor_idx = get_cell_index(neighbor_pos);
				if (grid_ptrw[neighbor_idx].collapsed)
					continue;

				Vector<int> &possible_neighbor_tiles = grid_ptrw[neighbor_idx].possible_tiles;
				bool changed = false;

				// --- OPTIMIZATION using pre-calculated map ---
				// 1. Build a set of all tile IDs that are valid neighbors for this direction,
				//    based on the current cell's possibilities.
				Dictionary valid_neighbor_ids;
				for (int k = 0; k < possible_current_tiles.size(); ++k) {
					int current_tile_id = possible_current_tiles[k];
					int hash_to_match = tile_prototypes[current_tile_id].hashes[i];

					// Find all tiles that have this hash on the opposite face.
					int64_t lookup_key = (int64_t(hash_to_match) << 3) + opposite_dir_idx[i];
					if (compatibility_map.has(lookup_key)) {
						Array compatible_tiles = compatibility_map[lookup_key];
						for (int l = 0; l < compatible_tiles.size(); ++l) {
							valid_neighbor_ids[compatible_tiles[l]] = true; // Add to the set of valid neighbors
						}
					}
				}

				// Check each of the neighbor's possible tiles
				for (int j = possible_neighbor_tiles.size() - 1; j >= 0; --j) {
					int neighbor_tile_id = possible_neighbor_tiles[j];

					// 2. A tile is compatible if its ID is in our set of valid IDs.
					bool is_compatible = valid_neighbor_ids.has(neighbor_tile_id);
					if (!is_compatible) {
						possible_neighbor_tiles.remove_at(j);
						changed = true;
					}
				}

				if (changed) {
					if (possible_neighbor_tiles.is_empty()) {
						UtilityFunctions::printerr("Contradiction found at ", neighbor_pos, ". Generation failed.");
						return false; // Contradiction
					}
					propagation_stack.push(neighbor_pos);
				}
			}
		}
		return true;
	}

public:
	void set_grid_size(const Vector3i &p_size) {
		grid_size = p_size;
	}
	Vector3i get_grid_size() const {
		return grid_size;
	}

	void set_tile_definitions(const Array &p_defs) {
		tile_definitions = p_defs;
	}
	Array get_tile_definitions() const {
		return tile_definitions;
	}

	void generate() {
		UtilityFunctions::print("Starting WFC generation...");
		clear_scene();

		if (!parse_tile_definitions()) {
			UtilityFunctions::printerr("Could not parse tile definitions. Aborting.");
			return;
		}

		initialize_grid();

		std::random_device rd;
		std::mt19937 gen(rd());

		int total_cells = grid_size.x * grid_size.y * grid_size.z;
		while (collapsed_count < total_cells) {
			// 1. Observation: Find cell with lowest entropy
			int cell_to_collapse_idx = find_lowest_entropy_cell();
			if (cell_to_collapse_idx == -1) {
				if (collapsed_count < total_cells) {
					UtilityFunctions::printerr("WFC failed: No cell to collapse, but not all cells are collapsed.");
				}
				break; // Should be finished or failed
			}

			// 2. Collapse
			WFCCell &cell = grid.ptrw()[cell_to_collapse_idx];
			std::uniform_int_distribution<> distrib(0, cell.possible_tiles.size() - 1);
			int chosen_tile_idx = cell.possible_tiles[distrib(gen)];
			cell.possible_tiles.clear();
			cell.possible_tiles.push_back(chosen_tile_idx);
			cell.collapsed = true;
			collapsed_count++;

			// 3. Propagate
			Vector3i cell_pos(
					cell_to_collapse_idx % grid_size.x,
					(cell_to_collapse_idx / grid_size.x) % grid_size.y,
					cell_to_collapse_idx / (grid_size.x * grid_size.y));

			if (!propagate(cell_pos)) {
				// Handle failure (e.g., clear and retry, or just stop)
				UtilityFunctions::printerr("Propagation failed. Halting generation.");
				return;
			}
		}

		// Generation complete, instantiate meshes
		UtilityFunctions::print("WFC generation complete. Instantiating meshes...");
		for (int z = 0; z < grid_size.z; ++z) {
			for (int y = 0; y < grid_size.y; ++y) {
				const WFCCell *grid_ptr = grid.ptr();
				for (int x = 0; x < grid_size.x; ++x) {
					Vector3i pos(x, y, z);
					int index = get_cell_index(pos);
					if (grid_ptr[index].collapsed && !grid_ptr[index].possible_tiles.is_empty()) {
						int tile_id = grid_ptr[index].possible_tiles[0];
						const WFCTile &tile_data = tile_prototypes[tile_id];

						MeshInstance3D *mi = memnew(MeshInstance3D);
						mi->set_mesh(tile_data.mesh);
						mi->set_position(Vector3(x, y, z)); // Assuming 1 unit per cell
						add_child(mi);
					}
				}
			}
		}
		UtilityFunctions::print("Scene populated.");
	}
};

#endif
