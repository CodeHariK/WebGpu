// MCNode: build all 256 tiles from the base meshes by rotating/mirroring (hash + transform together).
#include "marching_cubes/core/mc_hash.h"
#include "marching_cubes/core/mc_symmetry.h"
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

void MCNode::generate_variants_by_21_basemesh() {
	// 1 Corner - 8 total
	uint8_t h0 = 0b00000001; // C0 (Bottom)
	if (mesh_library[h0].mesh.is_valid()) {
		apply_8_rotations(h0, mesh_library[h0].transform, "1Corner");
	}

	// 2 Corners (Edge) - 12 total
	uint8_t h_edge_bot = 0b00001001; // Bottom Edge
	if (mesh_library[h_edge_bot].mesh.is_valid()) {
		apply_12_rotations(h_edge_bot, h_edge_bot, mesh_library[h_edge_bot].transform, "Edge");
	}
	uint8_t base_h_diag = 0b00000101;
	if (mesh_library[base_h_diag].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_diag];
		Transform3D base_t = base_conf.transform;

		apply_12_rotations(base_h_diag, base_h_diag, base_t, "Diag");
	}
	uint8_t base_h_opp = 0b01000001;
	if (mesh_library[base_h_opp].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_opp];
		Transform3D base_t = base_conf.transform;

		apply_4_axis_rotations(base_h_opp, base_h_opp, base_t, Vector3::AXIS_Y, "Opp", false);
	}

	// 3 Corners
	uint8_t base_h_v = 0b00000111;
	if (mesh_library[base_h_v].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_v];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_v, base_t, "V");
	}
	uint8_t base_h_l = 0b01000101;
	if (mesh_library[base_h_l].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_l];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_l, base_t, "L");
	}
	uint8_t base_h_tri = 0b10000101;
	if (mesh_library[base_h_tri].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_tri];
		Transform3D base_t = base_conf.transform;

		apply_8_rotations(base_h_tri, base_t, "Tri");
	}

	// 4 Corners
	uint8_t base_h_slab = 0b00001111;
	if (mesh_library[base_h_slab].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_slab];
		Transform3D base_t = base_conf.transform;

		apply_6_rotations(base_h_slab, base_t, "Slab");
	}
	uint8_t base_h_e1 = 0b10000111;
	if (mesh_library[base_h_e1].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_e1];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_e1, base_t, "E1");
	}
	uint8_t base_h_tetra = 0b10100101;
	if (mesh_library[base_h_tetra].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_tetra];
		Transform3D base_t = base_conf.transform;

		apply_transform_sequence(base_h_tetra, base_h_tetra, base_t, { RX90 }, "Tetra Rx90");
	}
	uint8_t base_h_e4 = 0b00100111;
	if (mesh_library[base_h_e4].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_e4];
		Transform3D base_t = base_conf.transform;

		apply_8_rotations(base_h_e4, base_t, "E4");
	}
	uint8_t base_h_e8 = 0b00010111;
	if (mesh_library[base_h_e8].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_e8];
		Transform3D base_t = base_conf.transform;

		apply_12_rotations(base_h_e8, base_h_e8, base_t, "E8");

		// Add mirror variants
		apply_12_mirror_rotations(base_h_e8, base_t, "E8 Sx");
	}
	uint8_t base_h_opp_edges = 0b01010101;
	if (mesh_library[base_h_opp_edges].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_opp_edges];
		Transform3D base_t = base_conf.transform;

		apply_6_rotations(base_h_opp_edges, base_t, "OppEdges");
	}

	// 5 Corners
	uint8_t base_h_v_inv = 0b01001111;
	if (mesh_library[base_h_v_inv].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_v_inv];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_v_inv, base_t, "InvV");
	}
	uint8_t base_h_ea = 0b01010111;
	if (mesh_library[base_h_ea].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_ea];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_ea, base_t, "InvV2");
	}
	uint8_t base_h_da = 0b01011011;
	if (mesh_library[base_h_da].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_da];
		Transform3D base_t = base_conf.transform;

		apply_8_rotations(base_h_da, base_t, "InvTri");
	}

	// 6 Corners
	uint8_t base_h_edge_inv = 0b01101111;
	if (mesh_library[base_h_edge_inv].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_edge_inv];
		Transform3D base_t = base_conf.transform;

		apply_12_rotations(base_h_edge_inv, base_h_edge_inv, base_t, "InvEdge");
	}
	uint8_t base_h_inv_diag = 0b01011111;
	if (mesh_library[base_h_inv_diag].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_inv_diag];
		Transform3D base_t = base_conf.transform;

		apply_12_rotations(base_h_inv_diag, base_h_inv_diag, base_t, "InvDiag");
	}
	uint8_t base_h_inv_opp = 0b11101011;
	if (mesh_library[base_h_inv_opp].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_inv_opp];
		Transform3D base_t = base_conf.transform;

		apply_4_axis_rotations(base_h_inv_opp, base_h_inv_opp, base_t, Vector3::AXIS_Y, "InvOpp", false);
	}

	// 7 Corners - 8 total
	uint8_t h_inv_top = 0b01111111; // Inv C4 (Missing Top)
	if (mesh_library[h_inv_top].mesh.is_valid()) {
		apply_8_rotations(h_inv_top, mesh_library[h_inv_top].transform, "7Corner Top");
	}
}

void MCNode::generate_variants_by_4Y_rotation() {
	// 1 Corner - 8 total (2 base meshes x 4 Y-rotations)
	for (uint8_t h : { 0b00000001, 0b00010000 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "1Corner", true);
		}
	}

	// 2 Corners (Edge) - 12 total (3 base meshes x 4 Y-rotations)
	for (uint8_t h : { 0b00001001, 0b10010000, 0b00010001 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner2Edge", true);
		}
	}
	for (uint8_t h : { 0b00000101, 0b01010000, 0b00011000, 0b10000001 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner2Diag", true);
		}
	}
	for (uint8_t h : { 0b01000001 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner2Opp", false);
		}
	}

	// 3 Corners
	for (uint8_t h : { 0b00001101, 0b11010000, 0b10001001, 0b00011001, 0b11000100, 0b00110010 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner3Face", false);
		}
	}
	for (uint8_t h : { 0b00010110, 0b01010100, 0b01100001, 0b01101000, 0b10000110, 0b01000101 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner3L", false);
		}
	}
	for (uint8_t h : { 0b10000101, 0b01010010 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner3Tri", false);
		}
	}

	// 4 Corners
	for (uint8_t h : { 0b00001111, 0b11110000, 0b10011001 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "4CornerFace", false);
		}
	}
	for (uint8_t h : { 0b10000111, 0b00111010, 0b01011001, 0b01011100, 0b01101010, 0b11100001 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "4CornerFace", false);
		}
	}
	for (uint8_t h : { 0b10100101 }) { // 2 Rotations
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner4Opp", false);
		}
	}
	for (uint8_t h : { 0b00100111, 0b11100100 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner4Tri", false);
		}
	}
	for (uint8_t h : { 0b00010111, 0b00101011, 0b00110110, 0b00111001, 0b01110001, 0b10110010 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner4OppEdges", false);
		}
	}
	for (uint8_t h : { 0b01010101, 0b01101001 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner4OppEdges", false);
		}
	}

	// 5 Corners
	for (uint8_t h : { 0b01001111, 0b11110001, 0b01110110, 0b11100110, 0b01100111, 0b01101110 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner5Top", true);
		}
	}
	for (uint8_t h : { 0b01010111, 0b11101010, 0b11100011, 0b00111110, 0b11000111, 0b01111100 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner5Top", true);
		}
	}
	for (uint8_t h : { 0b01011011, 0b10110101 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner5Tri", true);
		}
	}

	// 6 Corners
	for (uint8_t h : { 0b11001111, 0b11110110, 0b11101110 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner6Edge", true);
		}
	}
	for (uint8_t h : { 0b01011111, 0b11111010, 0b11100111, 0b01111110 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner6Diag", true);
		}
	}
	for (uint8_t h : { 0b11101011 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "InvOpp", false);
		}
	}

	// 7 Corners - 8 total
	for (uint8_t h : { 0b11101111, 0b11111110 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "7Corner", true);
		}
	}
}

void MCNode::validate_full_library() {
	UtilityFunctions::print("MCNode: Validating full 256-mesh library...");
	int missing_count = 0;
	for (int i = 0; i < 256; i++) {
		if (!mesh_library[i].mesh.is_valid()) {
			UtilityFunctions::print("MCNode: Missing mesh for hash: ", mc::hash_to_binary((uint8_t)i));
			missing_count++;
		}
	}

	if (missing_count == 0) {
		UtilityFunctions::print("MCNode: Full library validation successful (all 256 meshes present).");
	} else {
		UtilityFunctions::print("MCNode: Full library validation failed. Missing ", missing_count, " meshes.");
	}
}

void MCNode::apply_4_axis_rotations(
		uint8_t p_base_hash,
		uint8_t p_variant_hash,
		const Transform3D &p_base_t,
		Vector3::Axis p_axis,
		const String &p_prefix,
		bool p_include_base
) {
	if (p_include_base) {
		apply_transform_sequence(p_base_hash, p_variant_hash, p_base_t, {}, p_prefix);
	}

	MCTransform r90, r180, r270;
	String axis_name;

	switch (p_axis) {
		case Vector3::AXIS_X:
			r90 = RX90;
			r180 = RX180;
			r270 = RX270;
			axis_name = " Rx";
			break;
		case Vector3::AXIS_Y:
			r90 = RY90;
			r180 = RY180;
			r270 = RY270;
			axis_name = " Ry";
			break;
		case Vector3::AXIS_Z:
			r90 = RZ90;
			r180 = RZ180;
			r270 = RZ270;
			axis_name = " Rz";
			break;
	}

	apply_transform_sequence(p_base_hash, p_variant_hash, p_base_t, { r90 }, p_prefix + axis_name + "90");
	apply_transform_sequence(p_base_hash, p_variant_hash, p_base_t, { r180 }, p_prefix + axis_name + "180");
	apply_transform_sequence(p_base_hash, p_variant_hash, p_base_t, { r270 }, p_prefix + axis_name + "270");
}

void MCNode::apply_6_rotations(
		uint8_t p_base_hash,
		const Transform3D &p_base_t,
		const String &p_prefix
) {
	apply_4_axis_rotations(p_base_hash, p_base_hash, p_base_t, Vector3::AXIS_X, p_prefix, false);

	uint8_t h90 = p_base_hash;
	Transform3D t90 = p_base_t;
	mc::apply_transform(h90, t90, RX90);
	apply_transform_sequence(p_base_hash, h90, t90, { RY90 }, p_prefix + String(" Rx90 Ry90"));
	apply_transform_sequence(p_base_hash, h90, t90, { RY270 }, p_prefix + String(" Rx90 Ry270"));
}

void MCNode::apply_8_rotations(
		uint8_t p_base_hash,
		const Transform3D &p_base_t,
		const String &p_prefix
) {
	apply_4_axis_rotations(p_base_hash, p_base_hash, p_base_t, Vector3::AXIS_Y, p_prefix, false);

	for (MCTransform base_rot : { RX270, RX180 }) {
		uint8_t h = p_base_hash;
		Transform3D t = p_base_t;
		mc::apply_transform(h, t, base_rot);
		String name = (base_rot == RX270) ? " Rx270" : " Rx180";
		apply_4_axis_rotations(p_base_hash, h, t, Vector3::AXIS_Y, p_prefix + name, true);
	}
}

void MCNode::apply_12_rotations(
		uint8_t p_base_hash,
		uint8_t p_variant_hash,
		const Transform3D &p_base_t,
		const String &p_prefix
) {
	apply_4_axis_rotations(p_base_hash, p_variant_hash, p_base_t, Vector3::AXIS_Y, p_prefix, false);

	for (MCTransform axis_rot : { RX180, RX90, RZ90 }) {
		uint8_t h = p_variant_hash;
		Transform3D t = p_base_t;
		mc::apply_transform(h, t, axis_rot);
		String name = (axis_rot == RX180) ? " Rx180" : (axis_rot == RX90 ? " Rx90" : " Rz90");
		apply_4_axis_rotations(p_base_hash, h, t, Vector3::AXIS_Y, p_prefix + name, true);
	}
}

void MCNode::apply_12_mirror_rotations(
		uint8_t p_base_hash,
		const Transform3D &p_base_t,
		const String &p_prefix
) {
	uint8_t h = p_base_hash;
	Transform3D t = p_base_t;
	mc::apply_transform(h, t, SX);
	apply_transform_sequence(p_base_hash, h, t, {}, p_prefix);
	apply_12_rotations(p_base_hash, h, t, p_prefix);
}

void MCNode::apply_24_rotations(
		uint8_t p_base_hash,
		const Transform3D &p_base_t,
		const String &p_prefix
) {
	apply_4_axis_rotations(p_base_hash, p_base_hash, p_base_t, Vector3::AXIS_Y, p_prefix, false);

	// Rx180 -> Ry rotations
	{
		uint8_t h = p_base_hash;
		Transform3D t = p_base_t;
		mc::apply_transform(h, t, RX180);
		apply_4_axis_rotations(p_base_hash, h, t, Vector3::AXIS_Y, p_prefix + String(" Rx180"), true);
	}

	// Rx90/270 -> Rz rotations
	for (MCTransform rx : { RX90, RX270 }) {
		uint8_t h = p_base_hash;
		Transform3D t = p_base_t;
		mc::apply_transform(h, t, rx);
		apply_4_axis_rotations(
				p_base_hash, h, t, Vector3::AXIS_Z, p_prefix + String(rx == RX90 ? " Rx90" : " Rx270"), true
		);
	}

	// Rz90/270 -> Rx rotations
	for (MCTransform rz : { RZ90, RZ270 }) {
		uint8_t h = p_base_hash;
		Transform3D t = p_base_t;
		mc::apply_transform(h, t, rz);
		apply_4_axis_rotations(
				p_base_hash, h, t, Vector3::AXIS_X, p_prefix + String(rz == RZ90 ? " Rz90" : " Rz270"), true
		);
	}
}

void MCNode::apply_transform_sequence(
		uint8_t p_base_hash,
		uint8_t p_variant_hash,
		const Transform3D &p_base_t,
		const std::vector<MCTransform> &p_sequence,
		const String &p_name
) {
	uint8_t h = p_variant_hash;
	Transform3D t = p_base_t;

	for (MCTransform op : p_sequence) {
		mc::apply_transform(h, t, op);
	}

	String h_str = mc::hash_to_binary(h);

	if (!mesh_library[h].mesh.is_valid()) {
		MeshConfig config;
		config.mesh = mesh_library[p_base_hash].mesh;
		config.transform = t;
		config.source_mesh = p_base_hash;
		mesh_library[h] = config;
		UtilityFunctions::print("Generated Variant ", p_name, ": ", h_str);
	} else {
		UtilityFunctions::print("Rejected Variant ", p_name, ": ", h_str);
	}
}

} // namespace godot
