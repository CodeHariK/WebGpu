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

namespace {

// How one base tile of the 21-mesh set is spread over the cube's symmetries
// (each maps to one of MCNode's apply_* helpers).
enum class Spread : uint8_t {
	ROT4_Y, // the 3 other quarter turns about Y
	ROT6, // 6 orientations (tiles with a 2-fold symmetry)
	ROT8, // 8 orientations
	ROT12, // 12 orientations
	ROT12_MIRROR, // the mirror image, in 12 orientations
	ROT24, // all 24 orientations of the cube
	RX90_ONLY, // just one extra quarter turn about X (the tetrahedron)
};

struct BaseTile {
	uint8_t hash;
	Spread spread;
	const char *name;
};

// The 21-mesh set, grouped by how many corners are solid. Order matters: when two rotations
// land on the same hash, the first one wins (later ones are logged as "Rejected").
const BaseTile BASE_TILES_21[] = {
	{ 0b00000001, Spread::ROT8, "1Corner" }, // 1 corner
	{ 0b00001001, Spread::ROT12, "Edge" }, // 2 corners
	{ 0b00000101, Spread::ROT12, "Diag" },
	{ 0b01000001, Spread::ROT4_Y, "Opp" },
	{ 0b00000111, Spread::ROT24, "V" }, // 3 corners
	{ 0b01000101, Spread::ROT24, "L" },
	{ 0b10000101, Spread::ROT8, "Tri" },
	{ 0b00001111, Spread::ROT6, "Slab" }, // 4 corners
	{ 0b10000111, Spread::ROT24, "E1" },
	{ 0b10100101, Spread::RX90_ONLY, "Tetra Rx90" },
	{ 0b00100111, Spread::ROT8, "E4" },
	{ 0b00010111, Spread::ROT12, "E8" },
	{ 0b00010111, Spread::ROT12_MIRROR, "E8 Sx" },
	{ 0b01010101, Spread::ROT6, "OppEdges" },
	{ 0b01001111, Spread::ROT24, "InvV" }, // 5 corners
	{ 0b01010111, Spread::ROT24, "InvV2" },
	{ 0b01011011, Spread::ROT8, "InvTri" },
	{ 0b01101111, Spread::ROT12, "InvEdge" }, // 6 corners
	{ 0b01011111, Spread::ROT12, "InvDiag" },
	{ 0b11101011, Spread::ROT4_Y, "InvOpp" },
	{ 0b01111111, Spread::ROT8, "7Corner Top" }, // 7 corners
};

// The 68-mesh set only ever turns tiles about Y (so grass stays on top): each row is a family
// of base tiles that share a name and whether the unrotated base itself is (re)registered.
struct YRotationRow {
	const char *name;
	bool include_base;
	std::vector<uint8_t> hashes;
};

const YRotationRow BASE_TILES_68[] = {
	{ "1Corner", true, { 0b00000001, 0b00010000 } },
	{ "Corner2Edge", true, { 0b00001001, 0b10010000, 0b00010001 } },
	{ "Corner2Diag", true, { 0b00000101, 0b01010000, 0b00011000, 0b10000001 } },
	{ "Corner2Opp", false, { 0b01000001 } },
	{ "Corner3Face", false, { 0b00001101, 0b11010000, 0b10001001, 0b00011001, 0b11000100, 0b00110010 } },
	{ "Corner3L", false, { 0b00010110, 0b01010100, 0b01100001, 0b01101000, 0b10000110, 0b01000101 } },
	{ "Corner3Tri", false, { 0b10000101, 0b01010010 } },
	{ "4CornerFace", false, { 0b00001111, 0b11110000, 0b10011001 } },
	{ "4CornerFace", false, { 0b10000111, 0b00111010, 0b01011001, 0b01011100, 0b01101010, 0b11100001 } },
	{ "Corner4Opp", false, { 0b10100101 } },
	{ "Corner4Tri", false, { 0b00100111, 0b11100100 } },
	{ "Corner4OppEdges", false, { 0b00010111, 0b00101011, 0b00110110, 0b00111001, 0b01110001, 0b10110010 } },
	{ "Corner4OppEdges", false, { 0b01010101, 0b01101001 } },
	{ "Corner5Top", true, { 0b01001111, 0b11110001, 0b01110110, 0b11100110, 0b01100111, 0b01101110 } },
	{ "Corner5Top", true, { 0b01010111, 0b11101010, 0b11100011, 0b00111110, 0b11000111, 0b01111100 } },
	{ "Corner5Tri", true, { 0b01011011, 0b10110101 } },
	{ "Corner6Edge", true, { 0b11001111, 0b11110110, 0b11101110 } },
	{ "Corner6Diag", true, { 0b01011111, 0b11111010, 0b11100111, 0b01111110 } },
	{ "InvOpp", false, { 0b11101011 } },
	{ "7Corner", true, { 0b11101111, 0b11111110 } },
};

} // namespace

void MCNode::generate_variants_by_21_basemesh() {
	for (const BaseTile &tile : BASE_TILES_21) {
		if (!mesh_library[tile.hash].mesh.is_valid()) {
			continue; // this base tile is missing from the .glb
		}
		uint8_t h = tile.hash;
		Transform3D t = mesh_library[h].transform;
		String name = tile.name;
		switch (tile.spread) {
			case Spread::ROT4_Y:
				apply_4_axis_rotations(h, h, t, Vector3::AXIS_Y, name, false);
				break;
			case Spread::ROT6:
				apply_6_rotations(h, t, name);
				break;
			case Spread::ROT8:
				apply_8_rotations(h, t, name);
				break;
			case Spread::ROT12:
				apply_12_rotations(h, h, t, name);
				break;
			case Spread::ROT12_MIRROR:
				apply_12_mirror_rotations(h, t, name);
				break;
			case Spread::ROT24:
				apply_24_rotations(h, t, name);
				break;
			case Spread::RX90_ONLY:
				apply_transform_sequence(h, h, t, { RX90 }, name);
				break;
		}
	}
}

void MCNode::generate_variants_by_4Y_rotation() {
	for (const YRotationRow &row : BASE_TILES_68) {
		for (uint8_t h : row.hashes) {
			if (mesh_library[h].mesh.is_valid()) {
				apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, row.name, row.include_base);
			}
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
