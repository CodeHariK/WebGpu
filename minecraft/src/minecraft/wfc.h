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

#include "../include/celebi_parse.hpp"
#include "../include/help.hpp"
#include "../include/json.hpp"

using namespace godot;
using njson = nlohmann::json;

const Vector3i DIRECTIONS[] = {
	Vector3i(1, 0, 0), Vector3i(-1, 0, 0),
	Vector3i(0, 1, 0), Vector3i(0, -1, 0),
	Vector3i(0, 0, 1), Vector3i(0, 0, -1)
};
// Opposite direction indices: 0->1, 1->0, 2->3, 3->2, etc.
const int OPPOSITE_DIR_IDX[] = { 1, 0, 3, 2, 5, 4 };
const char *DIR_NAMES[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };

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

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("reset"), &WFCGenerator3D::reset);
		ClassDB::bind_method(D_METHOD("generate"), &WFCGenerator3D::generate);
		ClassDB::bind_method(D_METHOD("step"), &WFCGenerator3D::step);

		ClassDB::bind_method(D_METHOD("set_grid_size", "size"), &WFCGenerator3D::set_grid_size);
		ClassDB::bind_method(D_METHOD("get_grid_size"), &WFCGenerator3D::get_grid_size);
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "grid_size"), "set_grid_size", "get_grid_size");

		ClassDB::bind_method(D_METHOD("set_step_by_step", "enable"), &WFCGenerator3D::set_step_by_step);
		ClassDB::bind_method(D_METHOD("is_step_by_step"), &WFCGenerator3D::is_step_by_step);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "step_by_step"), "set_step_by_step", "is_step_by_step");

		ClassDB::bind_method(D_METHOD("set_show_entropy", "enable"), &WFCGenerator3D::set_show_entropy);
		ClassDB::bind_method(D_METHOD("is_showing_entropy"), &WFCGenerator3D::is_showing_entropy);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_entropy"), "set_show_entropy", "is_showing_entropy");

		ClassDB::bind_method(D_METHOD("set_generate_on_ready", "enable"), &WFCGenerator3D::set_generate_on_ready);
		ClassDB::bind_method(D_METHOD("get_generate_on_ready"), &WFCGenerator3D::get_generate_on_ready);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_on_ready"), "set_generate_on_ready", "get_generate_on_ready");

		ClassDB::bind_method(D_METHOD("set_step_forward", "enable"), &WFCGenerator3D::set_step_forward);
		ClassDB::bind_method(D_METHOD("get_step_forward"), &WFCGenerator3D::get_step_forward);
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "step_forward"), "set_step_forward", "get_step_forward");

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

private:
	enum GenerationState {
		IDLE,
		GENERATING,
		DONE,
		FAILED
	};

	GenerationState generation_state = IDLE;
	bool step_by_step = true;
	bool m_show_entropy = true;
	bool generate_on_ready = true;
	bool step_forward = false;
	bool m_show_all_possibilities = false;
	bool m_show_compatibility_table = false;
	bool m_show_tile_prototypes = false;

	Vector3i grid_size = Vector3i(2, 1, 2);

	Vector<WFCTile> tile_prototypes;
	Vector<WFCCell> grid;
	int collapsed_count = 0;
	Dictionary compatibility_map;
	Node3D *m_debug_labels_node = nullptr;
	Node3D *m_compatibility_table_node = nullptr;
	Node3D *m_tile_prototypes_node = nullptr;
	Node3D *m_debug_possibilities_node = nullptr;
	std::mt19937 random_generator;

	Celebi::Data loadCelebiJson(String path) {
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
		Celebi::Data data = libraryJson.get<Celebi::Data>();
		return data;
	}

	int get_cell_index(const Vector3i &pos) const {
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

		// Pick one randomly from the cells with the lowest entropy
		std::uniform_int_distribution<> distrib(0, lowest_entropy_cells.size() - 1);
		return lowest_entropy_cells[distrib(random_generator)];
	}

	bool propagate(const Vector3i &p_initial_pos) {
		std::stack<Vector3i> propagation_stack;
		propagation_stack.push(p_initial_pos);

		WFCCell *grid_ptrw = grid.ptrw();
		while (!propagation_stack.empty()) {
			Vector3i current_pos = propagation_stack.top();
			propagation_stack.pop();

			UtilityFunctions::print("propagation_stack size : ", String::num(propagation_stack.size()));

			int current_idx = get_cell_index(current_pos);
			const Vector<int> &possible_current_tiles = grid_ptrw[current_idx].possible_tiles;

			// For each neighbor
			for (int i = 0; i < 6; ++i) {
				Vector3i neighbor_pos = current_pos + DIRECTIONS[i];
				if (!is_in_bounds(neighbor_pos))
					continue;

				int neighbor_idx = get_cell_index(neighbor_pos);
				if (grid_ptrw[neighbor_idx].collapsed)
					continue;

				Vector<int> &possible_neighbor_tiles = grid_ptrw[neighbor_idx].possible_tiles;
				UtilityFunctions::print(neighbor_pos, " ", Help::vector_to_string(possible_neighbor_tiles));

				bool changed = false;

				// --- OPTIMIZATION using pre-calculated map ---
				// 1. Build a set of all tile IDs that are valid neighbors for this direction,
				//    based on the current cell's possibilities.
				Dictionary valid_neighbor_ids;
				for (int k = 0; k < possible_current_tiles.size(); ++k) {
					int current_tile_id = possible_current_tiles[k];
					int hash_to_match = tile_prototypes[current_tile_id].hashes[i];

					// Find all tiles that have this hash on the opposite face.
					int64_t lookup_key = (int64_t(hash_to_match) << 3) + OPPOSITE_DIR_IDX[i];
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

				UtilityFunctions::print(neighbor_pos, " changed: ", changed, " tiles: ", Help::vector_to_string(possible_neighbor_tiles));
			}
		}
		return true;
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
			// Perform one-time setup when the node enters the scene.
			if (!m_debug_labels_node) {
				m_debug_labels_node = memnew(Node3D);
				m_debug_labels_node->set_name("DebugLabels");
				add_child(m_debug_labels_node);
			}

			parse_tile_data();

			generate();

			if (m_show_compatibility_table) {
				show_compatibility_table();
			}

			if (m_show_tile_prototypes) {
				show_tile_prototypes_debug();
			}

			generate_on_ready = false;
		}

		if (generation_state == GENERATING && step_by_step) {
			if (m_show_entropy) {
				update_entropy_labels();
			}
			if (step_forward) {
				step();
				step_forward = false;
			} else if (!Engine::get_singleton()->is_editor_hint() && Input::get_singleton()->is_action_just_pressed("wfc_step")) {
				step();
			}

			if (m_show_all_possibilities) {
				update_all_possibilities_display();
			}
		}
	}

	void set_generate_on_ready(bool p_enable) {
		generate_on_ready = p_enable;
	}

	void set_step_by_step(bool p_enable) {
		step_by_step = p_enable;
	}
	bool is_step_by_step() const {
		return step_by_step;
	}

	void set_show_entropy(bool p_enable) {
		m_show_entropy = p_enable;
	}
	bool is_showing_entropy() const {
		return m_show_entropy;
	}

	bool get_generate_on_ready() const {
		return generate_on_ready;
	}

	void set_step_forward(bool p_enable) {
		step_forward = p_enable;
	}
	bool get_step_forward() const {
		return step_forward;
	}

	void set_show_all_possibilities(bool p_enable) {
		m_show_all_possibilities = p_enable;
	}
	bool is_showing_all_possibilities() const {
		return m_show_all_possibilities;
	}

	void set_show_compatibility_table(bool p_enable) {
		m_show_compatibility_table = p_enable;
	}
	bool is_showing_compatibility_table() const {
		return m_show_compatibility_table;
	}

	void set_show_tile_prototypes(bool p_enable) {
		m_show_tile_prototypes = p_enable;
	}
	bool is_showing_tile_prototypes() const {
		return m_show_tile_prototypes;
	}

	void set_grid_size(const Vector3i &p_size) {
		grid_size = p_size;
	}
	Vector3i get_grid_size() const {
		return grid_size;
	}

	// This is the new primary way to provide tile data to the generator.
	// It populates the internal data structures from your C++ structs.
	void parse_tile_data() {
		Celebi::Data celebiData = loadCelebiJson("res://assets/library.json");

		tile_prototypes.clear();
		compatibility_map.clear();

		godot::Dictionary meshes = Help::loadMeshFile("res://assets/Voxel.glb");
		godot::Array keys = meshes.keys();
		for (int i = 0; i < keys.size(); i++) {
			UtilityFunctions::print(keys[i], meshes[keys[i]]);
		}

		for (size_t i = 0; i < celebiData.items.size(); ++i) {
			const Celebi::LibraryItem &item = celebiData.items[i];

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

			UtilityFunctions::print(
					"Id: ", tile.id,
					" Mesh: ", mesh_path,
					" hashes: ", tile.hashes[0], ",", tile.hashes[1], ",", tile.hashes[2],
					",", tile.hashes[3], ",", tile.hashes[4], ",", tile.hashes[5]);

			tile_prototypes.push_back(tile);
		}

		if (tile_prototypes.is_empty()) {
			return;
		}

		// --- Pre-calculate the compatibility map ---
		for (int i = 0; i < tile_prototypes.size(); ++i) {
			const WFCTile &tile = tile_prototypes[i];

			for (int dir = 0; dir < 6; ++dir) {
				int64_t key = (int64_t(tile.hashes[dir]) << 3) + dir;
				if (!compatibility_map.has(key)) {
					compatibility_map[key] = Array();
				}
				Array arr = compatibility_map[key];
				arr.push_back(i);
			}
		}
	}

	void reset() {
		UtilityFunctions::print("Resetting WFC Generator.");

		for (int i = get_child_count() - 1; i >= 0; --i) {
			Node *child = get_child(i);
			if (child != m_debug_labels_node) {
				child->queue_free();
			}
		}

		if (tile_prototypes.is_empty()) {
			UtilityFunctions::printerr("Cannot initialize grid, tile data is empty.");
			return;
		}

		int total_cells = grid_size.x * grid_size.y * grid_size.z;
		grid.resize(total_cells);
		WFCCell *grid_ptr = grid.ptrw();
		for (int i = 0; i < total_cells; ++i) {
			grid_ptr[i].initialize(tile_prototypes.size());
		}
		collapsed_count = 0;
		generation_state = IDLE;

		// Create or clear debug labels
		if (m_show_entropy) {
			if (!m_debug_labels_node) {
				m_debug_labels_node = memnew(Node3D);
				m_debug_labels_node->set_name("DebugLabels");
				add_child(m_debug_labels_node);
			}
			// Ensure we have enough labels
			for (int i = m_debug_labels_node->get_child_count(); i < total_cells; ++i) {
				Label3D *label = memnew(Label3D);
				label->set_position(get_cell_pos(i) + Vector3(0.5, 0.5, 0.5));
				m_debug_labels_node->add_child(label);
			}
		}

		// Clear the possibilities display
		if (m_debug_possibilities_node) {
			m_debug_possibilities_node->queue_free();
			m_debug_possibilities_node = nullptr;
		}
	}

	void update_entropy_labels() {
		if (!m_debug_labels_node)
			return;

		for (int i = 0; i < grid.size(); ++i) {
			if (i >= m_debug_labels_node->get_child_count())
				continue;

			Label3D *label = Object::cast_to<Label3D>(m_debug_labels_node->get_child(i));
			if (label) {
				const WFCCell &cell = grid[i];
				if (cell.collapsed) {
					label->set_visible(false);
				} else {
					label->set_visible(true);
					label->set_text(String::num(cell.possible_tiles.size()));
				}
			}
		}
	}

	void update_all_possibilities_display() {
		if (!m_show_all_possibilities) {
			if (m_debug_possibilities_node) {
				m_debug_possibilities_node->queue_free();
				m_debug_possibilities_node = nullptr;
			}
			return;
		}

		if (!m_debug_possibilities_node) {
			m_debug_possibilities_node = memnew(Node3D);
			m_debug_possibilities_node->set_name("DebugPossibilities");
			add_child(m_debug_possibilities_node);
		} else {
			// Clear previous meshes
			for (int i = 0; i < m_debug_possibilities_node->get_child_count(); ++i) {
				m_debug_possibilities_node->get_child(i)->queue_free();
			}
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
				m_debug_possibilities_node->add_child(mi);
			}
		}
	}

	void show_tile_prototypes_debug() {
		if (!m_show_tile_prototypes) {
			if (m_tile_prototypes_node) {
				m_tile_prototypes_node->queue_free();
				m_tile_prototypes_node = nullptr;
			}
			return;
		}

		if (m_tile_prototypes_node) {
			m_tile_prototypes_node->queue_free();
		}
		m_tile_prototypes_node = memnew(Node3D);
		m_tile_prototypes_node->set_name("TilePrototypesDebug");
		add_child(m_tile_prototypes_node);

		const Vector3 display_origin = Vector3(grid_size.x + 5.0f, 0, 0);
		const float tile_spacing = 2.0f;
		const float label_offset = 0.75f;

		for (int i = 0; i < tile_prototypes.size(); ++i) {
			const WFCTile &tile = tile_prototypes[i];
			Vector3 tile_pos = display_origin + Vector3(i * tile_spacing, 0, 0);

			// Display the tile mesh
			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(tile.mesh);
			mi->set_position(tile_pos);
			m_tile_prototypes_node->add_child(mi);

			Label3D *hash_label = memnew(Label3D);
			hash_label->set_text(tile.mesh->get_name());
			hash_label->set_position(tile_pos + Vector3(0, 1.4, 0) * label_offset);
			m_tile_prototypes_node->add_child(hash_label);

			// Display hash labels for each face
			for (int dir = 0; dir < 6; ++dir) {
				Label3D *hash_label = memnew(Label3D);
				hash_label->set_text(String::num(tile.hashes[dir]));
				hash_label->set_position(tile_pos + Vector3(DIRECTIONS[dir]) * label_offset);
				m_tile_prototypes_node->add_child(hash_label);
			}
		}
	}

	void show_compatibility_table() { // Renamed from show_compatibility_table for consistency
		if (!m_show_compatibility_table) {
			if (m_compatibility_table_node) {
				m_compatibility_table_node->queue_free();
				m_compatibility_table_node = nullptr;
			}
			return;
		}

		m_compatibility_table_node = memnew(Node3D);
		m_compatibility_table_node->set_name("CompatibilityTable");
		add_child(m_compatibility_table_node);

		// Render the table directly from the compatibility_map
		const float row_height = 1.0f;
		const Vector3 table_origin = Vector3(0, 0, -5.0f);
		const Vector3 tile_scale = Vector3(0.2, 0.2, 0.2);
		const float tile_spacing = 0.25f;

		Array keys = compatibility_map.keys();
		keys.sort(); // Sort for a consistent layout

		for (int i = 0; i < keys.size(); ++i) {
			int64_t key = keys[i];
			Vector3 row_pos = table_origin + Vector3(0, i * -row_height, 0);

			// Add key label for the row
			Label3D *key_label = memnew(Label3D);
			int64_t hash = key >> 3;
			int dir = key & 0x7; // Mask for the lower 3 bits
			key_label->set_text("Hash: " + String::num_int64(hash) + ", Dir: " + DIR_NAMES[dir]);

			key_label->set_position(row_pos + Vector3(-4, 0, 0));
			m_compatibility_table_node->add_child(key_label);

			Array compatible_tiles = compatibility_map[key];

			for (int k = 0; k < compatible_tiles.size(); ++k) {
				int tile_id = compatible_tiles[k];
				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(tile_prototypes[tile_id].mesh);
				mi->set_scale(tile_scale);
				mi->set_position(row_pos + Vector3((k % 16) * tile_spacing, (float(k) / 16) * -tile_spacing, 0));
				m_compatibility_table_node->add_child(mi);
			}
		}
	}

	void step() {
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
		std::uniform_int_distribution<> distrib(0, cell.possible_tiles.size() - 1);
		int chosen_tile_idx = cell.possible_tiles[distrib(random_generator)];
		cell.possible_tiles.clear();
		cell.possible_tiles.push_back(chosen_tile_idx);
		cell.collapsed = true;
		collapsed_count++;

		// Instantiate mesh for the collapsed cell
		Vector3i cell_pos = get_cell_pos(cell_to_collapse_idx);
		const WFCTile &tile_data = tile_prototypes[chosen_tile_idx];
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(tile_data.mesh);
		mi->set_position(cell_pos);
		add_child(mi);

		UtilityFunctions::print("Collapsed ", cell_pos, " -> ", chosen_tile_idx);

		// 3. Propagate
		if (!propagate(cell_pos)) {
			UtilityFunctions::printerr("Propagation failed. Halting generation.");
			generation_state = FAILED;
			return;
		}
	}

	void generate() {
		reset();

		if (tile_prototypes.is_empty()) {
			UtilityFunctions::printerr("No tile data has been set. Call parse_tile_data() before generating. Aborting.");
			generation_state = FAILED;
			return;
		}

		UtilityFunctions::print("Starting WFC generation...");
		generation_state = GENERATING;

		if (!step_by_step) {
			while (generation_state == GENERATING) {
				step();
			}
		}
	}
};

#endif
