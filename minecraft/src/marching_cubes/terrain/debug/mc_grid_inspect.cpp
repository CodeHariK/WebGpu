// MCGrid: inspect mode — look at one cell's tile and its corners, without touching the terrain.
//
// inspect_cell(cell) marks the cell in the world (an orange wire cube, faintly visible through the
// ground) and the editor shows it in a side view (MCManager's inspect panel, a SubViewport with its
// own world). make_cell_preview(cell) builds what that view shows, centred on the origin:
//   - the tile mesh as the chunk draws it (same turn), with a wireframe overlay
//   - the cell's cube: thin dark edges
//   - its 8 corners as vertex dots: active = filled orange (like a selected vertex in Blender),
//     inactive = small black; each numbered with its hash bit (0..7), so the hash reads off the
//     dots: the name's character 7 - i is corner i (bits 0..3 bottom, 4..7 top; bit 0 at x, z+1;
//     1 at x+1, z+1; 2 at x+1, z; 3 at x, z)
// get_cell_info(cell) is the text beside it: cell, hash, the modelled tile it was turned from and
// the turn, group, level, variant. Nothing here changes the terrain, saves or undoes.
#include "marching_cubes/terrain/mc_grid.h"
#include "marching_cubes/library/mc_library.h"
#include "marching_cubes/util/mc_wire.h"
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <cctype>
#include <cmath>

namespace godot {

namespace {
const Color CUBE_EDGE(0.08f, 0.08f, 0.1f, 0.9f);
const Color HIGHLIGHT(1.0f, 0.62f, 0.1f, 1.0f);
const Color CORNER_ON(1.0f, 0.55f, 0.1f, 1.0f);
const Color CORNER_OFF(0.05f, 0.05f, 0.05f, 1.0f);

// Corner i of a cell, from the cell's centre (the hash bit order).
Vector3 corner_offset(int p_bit) {
	const Vector3 bottom[4] = { Vector3(-0.5f, -0.5f, 0.5f), Vector3(0.5f, -0.5f, 0.5f), Vector3(0.5f, -0.5f, -0.5f),
		Vector3(-0.5f, -0.5f, -0.5f) };
	Vector3 p = bottom[p_bit & 3];
	if (p_bit >= 4) {
		p.y = 0.5f;
	}
	return p;
}

String hash_bits(uint8_t p_hash) {
	String bits;
	for (int i = 7; i >= 0; i--) {
		bits += (p_hash >> i) & 1 ? "1" : "0";
	}
	return bits;
}

Ref<StandardMaterial3D> dot_material(const Color &p_color) {
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_albedo(p_color);
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true); // corners show through the tile
	material->set_render_priority(2);
	return material;
}
} // namespace

void MCGrid::set_inspect_mode(bool p_on) {
	inspect_mode = p_on;
	if (!p_on) {
		clear_inspect();
	}
}

bool MCGrid::inspect_cell(const Vector3i &p_cell) {
	Vector3i cells = _dims().max_corner(); // cells run 0 .. max_corner - 1 on each axis
	if (p_cell.x < 0 || p_cell.y < 0 || p_cell.z < 0 || p_cell.x >= cells.x || p_cell.y >= cells.y || p_cell.z >= cells.z) {
		return false;
	}
	if (!inspect_root) {
		inspect_root = memnew(Node3D);
		inspect_root->set_name("Inspect");
		add_child(inspect_root);
		inspect_highlight = memnew(MeshInstance3D);
		inspect_highlight->set_mesh(mc_wire_box(Vector3(1.02f, 1.02f, 1.02f)));
		inspect_highlight->set_material_override(mc_wire_material(HIGHLIGHT));
		inspect_root->add_child(inspect_highlight);
	}
	inspect_cell_pos = p_cell;
	inspect_has_cell = true;
	inspect_highlight->set_position(Vector3(p_cell) + Vector3(0.5f, 0.5f, 0.5f));
	inspect_root->show();
	return get_cell_hash(p_cell) != 0;
}

void MCGrid::clear_inspect() {
	inspect_has_cell = false;
	if (inspect_root) {
		inspect_root->hide();
	}
}

Vector3i MCGrid::get_inspect_cell() const { return inspect_has_cell ? inspect_cell_pos : Vector3i(-1, -1, -1); }

String MCGrid::get_cell_info(const Vector3i &p_cell) const {
	uint8_t hash = get_cell_hash(p_cell);
	String text = vformat("cell (%d, %d, %d)\nhash %s", p_cell.x, p_cell.y, p_cell.z, hash_bits(hash));
	if (hash == 0) {
		return text + "\nempty: no tile";
	}
	char level = static_cast<char>(std::toupper(static_cast<unsigned char>(mc::LEVEL_LETTERS[_level_at(p_cell)])));
	if (tile_library) {
		const MCTile &tile = tile_library->get_group_tile(_group_at(p_cell), mc::LEVEL_LETTERS[_level_at(p_cell)], hash);
		if (tile.mesh.is_null()) {
			text += "\nno tile in the library for this hash";
		} else {
			int degrees = static_cast<int>(std::lround(Math::rad_to_deg(tile.turn.basis.get_euler().y)));
			text += vformat(String::utf8("\nfrom %s, turned %d°"), hash_bits(tile.source_hash), (degrees + 360) % 360);
			if (tile.variant_meshes.size() > 1) {
				text += vformat("\nvariant %d of %d", static_cast<int>(tile.variant_for_cell(p_cell)),
						static_cast<int>(tile.variant_meshes.size()));
			}
		}
	}
	text += vformat(String::utf8("\ngroup %s · level %s"), String::chr(static_cast<char32_t>(_group_at(p_cell))),
			String::chr(static_cast<char32_t>(level)));
	return text;
}

Node3D *MCGrid::make_cell_preview(const Vector3i &p_cell) {
	if (_inspect_cube_material.is_null()) {
		_inspect_cube_material = mc_wire_material(CUBE_EDGE, false);
		_inspect_on_material = dot_material(CORNER_ON);
		_inspect_off_material = dot_material(CORNER_OFF);
		Ref<SphereMesh> dot;
		dot.instantiate();
		dot->set_radius(0.05f);
		dot->set_height(0.1f);
		dot->set_radial_segments(12);
		dot->set_rings(6);
		_inspect_dot_mesh = dot;
	}
	if (_wireframe_material.is_null()) {
		set_debug_wireframe(debug_wireframe); // makes the wireframe material
	}
	uint8_t hash = get_cell_hash(p_cell);
	Node3D *root = memnew(Node3D);
	root->set_name(vformat("Cell_%d_%d_%d", p_cell.x, p_cell.y, p_cell.z));

	// the tile, as the chunk draws it (relative to the cell centre), with the wireframe on top
	if (tile_library && hash != 0) {
		const MCTile &tile = tile_library->get_group_tile(_group_at(p_cell), mc::LEVEL_LETTERS[_level_at(p_cell)], hash);
		Ref<Mesh> mesh = tile.mesh_for_cell(p_cell);
		if (mesh.is_valid()) {
			MeshInstance3D *tile_node = memnew(MeshInstance3D);
			tile_node->set_name("Tile");
			tile_node->set_mesh(mesh);
			tile_node->set_transform(tile.turn);
			tile_node->set_material_overlay(_wireframe_material);
			root->add_child(tile_node);
		}
	}
	// the cube and its corners, numbered by hash bit
	MeshInstance3D *cube = memnew(MeshInstance3D);
	cube->set_mesh(mc_wire_box(Vector3(1, 1, 1)));
	cube->set_material_override(_inspect_cube_material);
	root->add_child(cube);
	for (int bit = 0; bit < 8; bit++) {
		bool on = (hash >> bit) & 1;
		MeshInstance3D *dot = memnew(MeshInstance3D);
		dot->set_mesh(_inspect_dot_mesh);
		dot->set_material_override(on ? _inspect_on_material : _inspect_off_material);
		dot->set_position(corner_offset(bit));
		dot->set_scale(on ? Vector3(1, 1, 1) : Vector3(0.7f, 0.7f, 0.7f));
		root->add_child(dot);
		Label3D *number = memnew(Label3D);
		number->set_text(String::num_int64(bit));
		number->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		number->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
		number->set_pixel_size(0.003f);
		number->set_font_size(32);
		number->set_outline_size(8);
		number->set_modulate(on ? CORNER_ON : Color(0.9f, 0.9f, 0.9f));
		number->set_position(corner_offset(bit) * 1.2f);
		root->add_child(number);
	}
	return root;
}

} // namespace godot
