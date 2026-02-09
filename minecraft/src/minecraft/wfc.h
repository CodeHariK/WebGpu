#ifndef WFC
#define WFC

#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/label3d.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/core/memory.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/core/property_info.hpp"
#include "godot_cpp/templates/vector.hpp"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3i.hpp>

#include <cstddef>
#include <cstdint>
#include <random>
#include <stack>
#include <string>

#include "../include/help.hpp"
#include "../include/json.hpp"
#include "../include/wfc_blender_hash_parse.hpp"

using namespace godot;
using njson = nlohmann::json;

const Vector3i DIRECTIONS[] = {
	Vector3i(1, 0, 0), Vector3i(-1, 0, 0),
	Vector3i(0, 1, 0), Vector3i(0, -1, 0),
	Vector3i(0, 0, 1), Vector3i(0, 0, -1)
};
// Opposite direction indices: 0->1, 1->0, 2->3, 3->2, etc.
const int OPPOSITE_DIR_IDX[] = { 1, 0, 3, 2, 5, 4 };
const inline char *DIR_NAMES[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };

// A simple struct to hold all data for a single tile prototype.
struct WFCTile {
	int id;
	godot::Ref<godot::Mesh> mesh;

	// Hashes for connectivity.
	// Indices correspond to directions: 0:+X, 1:-X, 2:+Y, 3:-Y, 4:+Z, 5:-Z
	int16_t hashes[6];
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

private:
	enum GenerationState {
		IDLE,
		GENERATING,
		DONE,
		FAILED
	};

	GenerationState generation_state = IDLE;
	bool step_by_step = true;
	bool generate_on_ready = true;
	bool step_forward = false;

	Vector3i grid_size = Vector3i(2, 1, 2);

	Vector<WFCTile> tile_prototypes;
	Vector<WFCCell> grid;
	int collapsed_count = 0;
	Dictionary compatibility_map;
	std::mt19937 random_generator;

	bool m_show_entropy_labels = true;
	Node3D *m_debug_entropy_labels_node = nullptr;
	bool m_show_compatibility_table = false;
	Node3D *m_debug_compatibility_table_node = nullptr;
	bool m_show_tile_prototypes = false;
	Node3D *m_debug_tile_prototypes_node = nullptr;
	bool m_show_all_possibilities = false;
	Node3D *m_debug_possibilities_node = nullptr;

	Wfc_blender_hash::Data loadWfc_blender_hashJson(String path) {
		Ref<FileAccess> fileHandle = FileAccess::open(path, FileAccess::READ);
		if (fileHandle.is_null()) {
			UtilityFunctions::print("Could not open " + path);
			return {};
		}
		String fileContent = fileHandle->get_as_text();
		njson libraryJson = njson::parse(std::string(fileContent.utf8().get_data()), nullptr, false);
		if (libraryJson.is_discarded()) {
			UtilityFunctions::print("Parse error: invalid JSON in " + path);
			return {};
		}
		Wfc_blender_hash::Data data = libraryJson.get<Wfc_blender_hash::Data>();
		return data;
	}

	inline int get_cell_index(const Vector3i &pos) const {
		return pos.x + pos.y * grid_size.x + pos.z * grid_size.x * grid_size.y;
	}

	inline Vector3i get_cell_pos(int p_index) const {
		return Vector3i(
				p_index % grid_size.x,
				(p_index / grid_size.x) % grid_size.y,
				p_index / (grid_size.x * grid_size.y));
	}

	bool is_in_bounds(const Vector3i &pos) const {
		return pos.x >= 0 && pos.x < grid_size.x &&
				pos.y >= 0 && pos.y < grid_size.y &&
				pos.z >= 0 && pos.z < grid_size.z;
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

		UtilityFunctions::print("find_lowest_entropy_cell random select in ", lowest_entropy_cells.size());
		// Pick one randomly from the cells with the lowest entropy
		std::uniform_int_distribution<> distrib(0, lowest_entropy_cells.size() - 1);
		return lowest_entropy_cells[distrib(random_generator)];
	}

	// This is the new primary way to provide tile data to the generator.
	// It populates the internal data structures from your C++ structs.
	void parse_tile_data() {
		Wfc_blender_hash::Data Wfc_blender_hashData = loadWfc_blender_hashJson("res://assets/library.json");

		tile_prototypes.clear();
		compatibility_map.clear();

		godot::Dictionary meshes = Help::loadMeshFile("res://assets/Voxel.glb");

		for (size_t i = 0; i < Wfc_blender_hashData.items.size(); ++i) {
			const Wfc_blender_hash::LibraryItem &item = Wfc_blender_hashData.items[i];

			WFCTile tile;
			tile.id = i;

			String mesh_path = String(item.obj.c_str());
			tile.mesh = meshes[mesh_path];
			if (tile.mesh.is_null()) {
				UtilityFunctions::printerr("Failed to load mesh at path: ", mesh_path);
				tile_prototypes.clear(); // Invalidate data on failure
				return;
			}

			tile.hashes[0] = item.hashPx;
			tile.hashes[1] = item.hashNx;
			tile.hashes[2] = item.hashPy;
			tile.hashes[3] = item.hashNy;
			tile.hashes[4] = item.hashPz;
			tile.hashes[5] = item.hashNz;

			// UtilityFunctions::print(
			// 		"Id: ", tile.id,
			// 		" Mesh: ", mesh_path,
			// 		" hashes: ", tile.hashes[0], ",", tile.hashes[1], ",", tile.hashes[2],
			// 		",", tile.hashes[3], ",", tile.hashes[4], ",", tile.hashes[5]);

			tile_prototypes.push_back(tile);
		}

		// Add a special "air" tile. It has a null mesh and all-zero hashes.
		WFCTile air_tile;
		air_tile.id = tile_prototypes.size();
		air_tile.mesh.unref(); // No mesh for air
		for (int i = 0; i < 6; ++i) {
			air_tile.hashes[i] = 0;
		}
		tile_prototypes.push_back(air_tile);

		// --- Pre-calculate the compatibility map ---
		// Use a temporary map with Dictionaries as sets to avoid duplicate tile IDs.
		Dictionary temp_compatibility_sets;
		int air_tile_id = tile_prototypes.size() - 1;
		for (int i = 0; i < tile_prototypes.size(); ++i) {
			const WFCTile &tile = tile_prototypes[i];
			for (int dir = 0; dir < 6; ++dir) {
				int hash = tile.hashes[dir];
				int64_t key = (int64_t(hash) << 3) + dir;

				if (!temp_compatibility_sets.has(key)) {
					temp_compatibility_sets[key] = Dictionary();
				}

				Dictionary set = temp_compatibility_sets[key];
				set[i] = true; // Add tile `i` to the set for its own hash.

				if (hash != 0) {
					set[air_tile_id] = true; // Add air tile compatibility for non-air faces.
				}
			}
		}
		// Convert the sets (Dictionaries) to Arrays for the final map.
		Array keys = temp_compatibility_sets.keys();
		for (int i = 0; i < keys.size(); ++i) {
			Variant key = keys[i];
			Dictionary set = temp_compatibility_sets[key];
			compatibility_map[key] = set.keys();
		}
	}

	void reset() {
		UtilityFunctions::print("Reset");

		parse_tile_data();

		for (int i = get_child_count() - 1; i >= 0; --i) {
			Node *child = get_child(i);
			child->queue_free();
		}

		int total_cells = grid_size.x * grid_size.y * grid_size.z;
		grid.resize(total_cells);
		WFCCell *grid_ptr = grid.ptrw();
		for (int i = 0; i < total_cells; ++i) {
			grid_ptr[i].initialize(tile_prototypes.size());
		}
		collapsed_count = 0;
		generation_state = IDLE;

		//-------
		m_debug_entropy_labels_node = memnew(Node3D);
		m_debug_entropy_labels_node->set_name("DebugLabels");
		add_child(m_debug_entropy_labels_node);

		m_debug_possibilities_node = memnew(Node3D);
		m_debug_possibilities_node->set_name("DebugPossibilities");
		add_child(m_debug_possibilities_node);

		// Create or clear debug labels
		if (m_show_entropy_labels) {
			int total_cells = grid_size.x * grid_size.y * grid_size.z;
			// Ensure we have enough labels
			for (int i = m_debug_entropy_labels_node->get_child_count(); i < total_cells; ++i) {
				Label3D *label = memnew(Label3D);
				label->set_position(get_cell_pos(i) + Vector3(0.5, 0.5, 0.5));
				label->set_rotation(Vector3(-1.25, 0, 0));
				label->set_scale(Vector3(.5, .5, .5));
				m_debug_entropy_labels_node->add_child(label);
			}
		}

		if (m_show_compatibility_table) {
			debug_compatibility_table();
		}

		if (m_show_tile_prototypes) {
			debug_tile_prototypes();
		}
	}

	void step() {
		UtilityFunctions::print("\nStep---------\n");

		if (generation_state != GENERATING) {
			return;
		}

		int total_cells = grid_size.x * grid_size.y * grid_size.z;
		if (collapsed_count >= total_cells) {
			generation_state = DONE;
			UtilityFunctions::print("WFC generation complete. Instantiating meshes...");
			return;
		}

		// 1. Observation: Find cell with lowest entropy
		int cell_to_collapse_idx = find_lowest_entropy_cell();
		if (cell_to_collapse_idx == -1) {
			UtilityFunctions::printerr("WFC failed: No cell to collapse, but not all cells are collapsed.");
			generation_state = FAILED;
			return;
		}

		// 2. Collapse
		WFCCell &cell = grid.ptrw()[cell_to_collapse_idx];
		int chosen_tile_idx;

		// Exclude air tile from random selection unless it's the only option
		int air_tile_id = tile_prototypes.size() - 1;
		if (cell.possible_tiles.size() > 1 && cell.possible_tiles.has(air_tile_id)) {
			Vector<int> non_air_options;
			for (int i = 0; i < cell.possible_tiles.size(); ++i) {
				if (cell.possible_tiles[i] != air_tile_id) {
					non_air_options.push_back(cell.possible_tiles[i]);
				}
			}
			std::uniform_int_distribution<> distrib(0, non_air_options.size() - 1);
			chosen_tile_idx = non_air_options[distrib(random_generator)];
		} else {
			// Either only one option left, or only air is an option.
			std::uniform_int_distribution<> distrib(0, cell.possible_tiles.size() - 1);
			chosen_tile_idx = cell.possible_tiles[distrib(random_generator)];
		}

		cell.possible_tiles.clear();
		cell.possible_tiles.push_back(chosen_tile_idx);
		cell.collapsed = true;
		collapsed_count++;

		// Instantiate mesh for the collapsed cell
		Vector3i cell_pos = get_cell_pos(cell_to_collapse_idx);
		const WFCTile &tile_data = tile_prototypes[chosen_tile_idx];
		if (tile_data.mesh.is_valid()) { // Only instantiate if there is a mesh (air tile has no mesh)
			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(tile_data.mesh);
			mi->set_position(cell_pos);
			add_child(mi);
		}

		UtilityFunctions::print("Collapsed ", cell_pos, " -> ", chosen_tile_idx, " Possibilities ", cell.possible_tiles.size());

		// 3. Propagate
		if (!propagate(cell_pos)) {
			UtilityFunctions::printerr("Propagation failed. Halting generation.");
			generation_state = FAILED;
			return;
		}
	}

	bool propagate(const Vector3i &p_initial_pos) {
		std::stack<Vector3i> propagation_stack;
		propagation_stack.push(p_initial_pos);

		UtilityFunctions::print("propagate ", p_initial_pos);

		WFCCell *grid_ptrw = grid.ptrw();
		while (!propagation_stack.empty()) {
			Vector3i current_pos = propagation_stack.top();
			propagation_stack.pop();

			// UtilityFunctions::print("propagation_stack size : ", String::num(propagation_stack.size()));

			int currentIdx = get_cell_index(current_pos);
			const Vector<int> &possibleCurrentTiles = grid_ptrw[currentIdx].possible_tiles;

			// For each neighbor
			for (int dirIdx = 0; dirIdx < 6; ++dirIdx) {
				Vector3i neighbor_pos = current_pos + DIRECTIONS[dirIdx];
				if (!is_in_bounds(neighbor_pos))
					continue;

				int neighborIdx = get_cell_index(neighbor_pos);
				if (grid_ptrw[neighborIdx].collapsed)
					continue;

				Vector<int> &possible_neighbor_tiles = grid_ptrw[neighborIdx].possible_tiles;
				// UtilityFunctions::print(neighbor_pos, " ", Help::vector_to_string(possible_neighbor_tiles));

				bool changed = false;

				// --- OPTIMIZATION using pre-calculated map ---
				// 1. Build a set of all tile IDs that are valid neighbors for the neighbor cell,
				//    based on the current cell's possibilities.
				Dictionary valid_neighbor_ids;
				int opposite_dir = OPPOSITE_DIR_IDX[dirIdx];

				for (int possibleIdx = 0; possibleIdx < possibleCurrentTiles.size(); ++possibleIdx) {
					int current_tile_id = possibleCurrentTiles[possibleIdx];
					int current_face_hash = tile_prototypes[current_tile_id].hashes[dirIdx];

					// Find all tiles that have a compatible hash on their opposite face.
					int64_t lookup_key = (int64_t(current_face_hash) << 3) + opposite_dir;
					if (compatibility_map.has(lookup_key)) {
						Array compatible_tiles = compatibility_map[lookup_key];
						for (int l = 0; l < compatible_tiles.size(); ++l) {
							valid_neighbor_ids[compatible_tiles[l]] = true; // Add to the set of valid neighbors
						}
					}
				}

				// If there are no specific compatible tiles, it means we must connect to air.
				// This happens at the edge of the map where no other tile can be placed.
				if (valid_neighbor_ids.is_empty()) {
					valid_neighbor_ids[tile_prototypes.size() - 1] = true; // Add air tile ID
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
						UtilityFunctions::printerr("Contradiction found at ", neighbor_pos, ". No valid tiles left. Generation failed.");
						return false; // Contradiction
					}
					propagation_stack.push(neighbor_pos);
					UtilityFunctions::print(DIR_NAMES[dirIdx], " ", neighbor_pos, " changed: ", changed, " tiles: ", Help::vector_to_string(possible_neighbor_tiles));
				}
			}
		}
		return true;
	}

	void debug_entropy_labels() {
		for (int i = 0; i < grid.size(); ++i) {
			Label3D *label = Object::cast_to<Label3D>(m_debug_entropy_labels_node->get_child(i));
			if (label) {
				const WFCCell &cell = grid[i];
				if (cell.collapsed) {
					label->set_visible(false);
				} else {
					label->set_visible(true);
					Vector3i pos = get_cell_pos(i);
					label->set_text(String(pos) + "\n" +
							String::num(cell.possible_tiles.size()));
				}
			}
		}
	}

	void debug_all_possibilities_display() {
		// Clear previous meshes
		for (int i = 0; i < m_debug_possibilities_node->get_child_count(); ++i) {
			m_debug_possibilities_node->get_child(i)->queue_free();
		}

		const float vertical_offset_start = grid_size.y + 1.0f;
		const float vertical_spacing = 1.1f;

		for (int i = 0; i < grid.size(); ++i) {
			const WFCCell &wfc_cell = grid[i];
			if (wfc_cell.collapsed || wfc_cell.possible_tiles.size() == tile_prototypes.size()) {
				continue;
			}

			Vector3 cell_base_pos = get_cell_pos(i);

			for (int j = 0; j < wfc_cell.possible_tiles.size(); ++j) {
				int tile_id = wfc_cell.possible_tiles[j];
				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(tile_prototypes[tile_id].mesh);
				mi->set_position(cell_base_pos + Vector3(0, vertical_offset_start + j * vertical_spacing, 0));
				mi->set_scale(Vector3(.2, .2, .2));
				m_debug_possibilities_node->add_child(mi);
			}
		}
	}

	void debug_tile_prototypes() {
		m_debug_tile_prototypes_node = memnew(Node3D);
		m_debug_tile_prototypes_node->set_name("TilePrototypesDebug");
		add_child(m_debug_tile_prototypes_node);

		const Vector3 display_origin = Vector3(grid_size.x + 5.0f, 0, 0);
		const float tile_spacing = 2.0f;
		const float label_offset = 0.75f;

		for (int i = 0; i < tile_prototypes.size(); ++i) {
			const WFCTile &tile = tile_prototypes[i];
			Vector3 tile_pos = display_origin + Vector3(i * tile_spacing, 0, 0);

			if (tile.mesh != nullptr) {
				// Display the tile mesh
				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(tile.mesh);
				mi->set_position(tile_pos);
				m_debug_tile_prototypes_node->add_child(mi);
			}

			String tileName = tile.mesh != nullptr ? tile.mesh->get_name() : "air";

			Label3D *main_label = memnew(Label3D);
			main_label->set_text(
					tileName + "\n" +
					String::num_uint64(tile.hashes[0]) + "," + String::num_uint64(tile.hashes[1]) + "," + String::num_uint64(tile.hashes[2]) +
					"," + String::num_uint64(tile.hashes[3]) + "," + String::num_uint64(tile.hashes[4]) + "," + String::num_uint64(tile.hashes[5]));
			main_label->set_position(tile_pos + Vector3(0, 1.6, 0) * label_offset);
			m_debug_tile_prototypes_node->add_child(main_label);

			// Display hash labels for each face
			for (int dir = 0; dir < 6; ++dir) {
				Label3D *hash_label = memnew(Label3D);
				hash_label->set_text(String::num(tile.hashes[dir]));
				hash_label->set_position(tile_pos + Vector3(DIRECTIONS[dir]) * label_offset);
				m_debug_tile_prototypes_node->add_child(hash_label);
			}
		}
	}

	void debug_compatibility_table() {
		m_debug_compatibility_table_node = memnew(Node3D);
		m_debug_compatibility_table_node->set_name("CompatibilityTable");
		add_child(m_debug_compatibility_table_node);

		// Render the table directly from the compatibility_map
		const float row_height = 0.5f;
		const Vector3 table_origin = Vector3(0, 0, -5.0f);
		const Vector3 tile_scale = Vector3(0.2, 0.2, 0.2);
		const float tile_spacing = 0.25f;

		Array keys = compatibility_map.keys();
		keys.sort(); // Sort for a consistent layout

		for (int i = 0; i < keys.size(); ++i) {
			int64_t key = keys[i];
			Vector3 row_pos = table_origin + Vector3(0, i * -row_height, 0);

			Array compatible_tiles = compatibility_map[key];

			// Add key label for the row
			Label3D *key_label = memnew(Label3D);
			int64_t hash = key >> 3;
			int dir = key & 0x7; // Mask for the lower 3 bits
			key_label->set_text("Hash: " + String::num_int64(hash) + ", Dir: " + DIR_NAMES[dir] + " : " + String::num_uint64(compatible_tiles.size()));
			key_label->set_position(row_pos + Vector3(-1, 0, 0));
			m_debug_compatibility_table_node->add_child(key_label);

			for (int k = 0; k < compatible_tiles.size(); ++k) {
				int tile_id = compatible_tiles[k];
				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(tile_prototypes[tile_id].mesh);
				mi->set_scale(tile_scale);
				mi->set_position(row_pos + Vector3(k * tile_spacing, 0, 0));
				m_debug_compatibility_table_node->add_child(mi);
			}
		}
	}

public:
	WFCGenerator3D() {
		random_generator.seed(std::random_device()());
		set_process_mode(Node::ProcessMode::PROCESS_MODE_INHERIT);
	}

	~WFCGenerator3D() {
		if (Engine::get_singleton()->is_editor_hint()) {
			set_process_mode(Node::ProcessMode::PROCESS_MODE_DISABLED);
		}
	}

	void _ready() override {
	}

	void _process(double delta) override {
		//		if (Engine::get_singleton()->is_editor_hint()) {
		//			return;
		//		}

		// If generate_on_ready is true, run generation once and then disable it.
		if (generate_on_ready) {
			reset();

			UtilityFunctions::print("Starting WFC generation...");
			generation_state = GENERATING;

			if (!step_by_step) {
				while (generation_state == GENERATING) {
					step();
				}
			}

			generate_on_ready = false;
		}

		if (generation_state == GENERATING && step_by_step) {
			if (step_forward) {
				step();
				step_forward = false;
			} else if (!Engine::get_singleton()->is_editor_hint() && Input::get_singleton()->is_action_just_pressed("wfc_step")) {
				step();
			}

			if (m_show_entropy_labels) {
				debug_entropy_labels();
			}
			if (m_show_all_possibilities) {
				debug_all_possibilities_display();
			}
		}
	}

	void set_generate_on_ready(bool p_enable) { generate_on_ready = p_enable; }
	bool get_generate_on_ready() const { return generate_on_ready; }

	void set_step_by_step(bool p_enable) { step_by_step = p_enable; }
	bool is_step_by_step() const { return step_by_step; }

	void set_show_entropy(bool p_enable) { m_show_entropy_labels = p_enable; }
	bool is_showing_entropy() const { return m_show_entropy_labels; }

	void set_step_forward(bool p_enable) { step_forward = p_enable; }
	bool get_step_forward() const { return step_forward; }

	void set_show_all_possibilities(bool p_enable) { m_show_all_possibilities = p_enable; }
	bool is_showing_all_possibilities() const { return m_show_all_possibilities; }

	void set_show_compatibility_table(bool p_enable) { m_show_compatibility_table = p_enable; }
	bool is_showing_compatibility_table() const { return m_show_compatibility_table; }

	void set_show_tile_prototypes(bool p_enable) { m_show_tile_prototypes = p_enable; }
	bool is_showing_tile_prototypes() const { return m_show_tile_prototypes; }

	void set_grid_size(const Vector3i &p_size) { grid_size = p_size; }
	Vector3i get_grid_size() const { return grid_size; }

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("reset"), &WFCGenerator3D::reset);
		ClassDB::bind_method(D_METHOD("step"), &WFCGenerator3D::step);

		ClassDB::bind_method(D_METHOD("set_grid_size", "size"), &WFCGenerator3D::set_grid_size);
		ClassDB::bind_method(D_METHOD("get_grid_size"), &WFCGenerator3D::get_grid_size);
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "grid_size"), "set_grid_size", "get_grid_size");

		ClassDB::bind_method(D_METHOD("set_step_by_step", "enable"), &WFCGenerator3D::set_step_by_step);
		ClassDB::bind_method(D_METHOD("is_step_by_step"), &WFCGenerator3D::is_step_by_step);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "step_by_step"), "set_step_by_step", "is_step_by_step");

		ClassDB::bind_method(D_METHOD("set_generate_on_ready", "enable"), &WFCGenerator3D::set_generate_on_ready);
		ClassDB::bind_method(D_METHOD("get_generate_on_ready"), &WFCGenerator3D::get_generate_on_ready);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_on_ready"), "set_generate_on_ready", "get_generate_on_ready");

		ClassDB::bind_method(D_METHOD("set_step_forward", "enable"), &WFCGenerator3D::set_step_forward);
		ClassDB::bind_method(D_METHOD("get_step_forward"), &WFCGenerator3D::get_step_forward);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "step_forward"), "set_step_forward", "get_step_forward");

		ClassDB::bind_method(D_METHOD("set_show_entropy", "enable"), &WFCGenerator3D::set_show_entropy);
		ClassDB::bind_method(D_METHOD("is_showing_entropy"), &WFCGenerator3D::is_showing_entropy);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_entropy"), "set_show_entropy", "is_showing_entropy");

		ClassDB::bind_method(D_METHOD("set_show_all_possibilities", "enable"), &WFCGenerator3D::set_show_all_possibilities);
		ClassDB::bind_method(D_METHOD("is_showing_all_possibilities"), &WFCGenerator3D::is_showing_all_possibilities);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_all_possibilities"), "set_show_all_possibilities", "is_showing_all_possibilities");

		ClassDB::bind_method(D_METHOD("set_show_compatibility_table", "enable"), &WFCGenerator3D::set_show_compatibility_table);
		ClassDB::bind_method(D_METHOD("is_showing_compatibility_table"), &WFCGenerator3D::is_showing_compatibility_table);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_compatibility_table"), "set_show_compatibility_table", "is_showing_compatibility_table");

		ClassDB::bind_method(D_METHOD("set_show_tile_prototypes", "enable"), &WFCGenerator3D::set_show_tile_prototypes);
		ClassDB::bind_method(D_METHOD("is_showing_tile_prototypes"), &WFCGenerator3D::is_showing_tile_prototypes);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_tile_prototypes"), "set_show_tile_prototypes", "is_showing_tile_prototypes");
	}
};

#endif
