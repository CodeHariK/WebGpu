// MCTileLibrary::build_showcase(): every tile in a grid (one row per base mesh), labelled, for checking.
#include "marching_cubes/core/mc_hash.h"
#include "marching_cubes/library/mc_library.h"
#include <algorithm>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <vector>

namespace godot {

// Replace the children with one row per modelled tile, holding every hash it fills, each
// labelled with its corner count and hash.
void MCTileLibrary::build_showcase() {
	for (int i = get_child_count() - 1; i >= 0; i--) {
		Node *child = get_child(i);
		child->queue_free();
		remove_child(child);
	}

	float spacing = 2.0f; // 1 unit mesh + 1 unit gap

	// 1. Arrange Rows (Base meshes along Z based on master sorted order)
	for (int row = 0; row < (int)loaded_hashes.size(); row++) {
		uint8_t base_h = loaded_hashes[row];

		// 2. Find all variants for this row
		std::vector<uint8_t> variants;
		if (tiles[base_h].mesh.is_valid()) {
			variants.push_back(base_h);
		}

		for (int i = 0; i < 256; i++) {
			if (i == base_h) {
				continue;
			}
			if (tiles[i].mesh.is_valid() && tiles[i].source_hash == base_h) {
				variants.push_back((uint8_t)i);
			}
		}

		for (int col = 0; col < (int)variants.size(); col++) {
			uint8_t variant_h = variants[col];
			const MCTile &conf = tiles[variant_h];

			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(conf.mesh);
			String h_str = mc::hash_to_binary(variant_h);
			mi->set_name(h_str);

			Transform3D grid_t;
			grid_t.origin = Vector3(col * spacing, 0, row * spacing);
			mi->set_transform(grid_t * conf.turn);

			add_child(mi);
			if (Engine::get_singleton()->is_editor_hint()) {
				mi->set_owner(get_owner());
			}

			Label3D *label = memnew(Label3D);
			int bit_count = mc::corner_count(variant_h);
			label->set_text(String::num_int64(bit_count) + " : " + h_str);
			label->set_position(grid_t.origin + Vector3(0, 1.5f, -0.2f));
			label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
			label->set_font_size(24);
			add_child(label);
			if (Engine::get_singleton()->is_editor_hint()) {
				label->set_owner(get_owner());
			}
		}
	}
}

} // namespace godot
