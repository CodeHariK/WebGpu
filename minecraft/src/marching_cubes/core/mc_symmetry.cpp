// Corner permutations for the cell symmetries, and applying them to hash + transform.
#include "marching_cubes/core/mc_symmetry.h"
#include "marching_cubes/core/mc_corners.h"
#include <godot_cpp/core/math.hpp>

namespace godot::mc {

uint8_t rotate_y(uint8_t p_hash) {
	static const uint8_t swap_table[8] = { C1, C2, C3, C0, C5, C6, C7, C4 };
	uint8_t next = 0;
	for (int i = 7; i >= 0; i--) {
		if ((p_hash >> i) & 1) {
			next |= (1 << swap_table[i]);
		}
	}
	return next;
}

uint8_t rotate_x(uint8_t p_hash) {
	static const uint8_t swap_table[8] = { C3, C2, C6, C7, C0, C1, C5, C4 };
	uint8_t next = 0;
	for (int i = 7; i >= 0; i--) {
		if ((p_hash >> i) & 1) {
			next |= (1 << swap_table[i]);
		}
	}
	return next;
}

uint8_t rotate_z(uint8_t p_hash) {
	static const uint8_t swap_table[8] = { C1, C5, C6, C2, C0, C4, C7, C3 };
	uint8_t next = 0;
	for (int i = 7; i >= 0; i--) {
		if ((p_hash >> i) & 1) {
			next |= (1 << swap_table[i]);
		}
	}
	return next;
}

uint8_t mirror_x(uint8_t p_hash) {
	static const uint8_t swap_table[8] = { C1, C0, C3, C2, C5, C4, C7, C6 };
	uint8_t next = 0;
	for (int i = 7; i >= 0; i--) {
		if ((p_hash >> i) & 1) {
			next |= (1 << swap_table[i]);
		}
	}
	return next;
}

void apply_transform(
		uint8_t &p_hash,
		Transform3D &p_transform,
		MCTransform p_op
) {
	float p90 = Math::PI / 2.0;
	float m90 = -Math::PI / 2.0;

	switch (p_op) {
		case RX90:
			p_hash = rotate_x(p_hash);
			p_transform = p_transform.rotated(Vector3(1, 0, 0), p90);
			break;
		case RX180:
			p_hash = rotate_x(rotate_x(p_hash));
			p_transform = p_transform.rotated(Vector3(1, 0, 0), Math::PI);
			break;
		case RX270:
			p_hash = rotate_x(rotate_x(rotate_x(p_hash)));
			p_transform = p_transform.rotated(Vector3(1, 0, 0), m90);
			break;

		case RY90:
			p_hash = rotate_y(p_hash);
			p_transform = p_transform.rotated(Vector3(0, 1, 0), p90);
			break;
		case RY180:
			p_hash = rotate_y(rotate_y(p_hash));
			p_transform = p_transform.rotated(Vector3(0, 1, 0), Math::PI);
			break;
		case RY270:
			p_hash = rotate_y(rotate_y(rotate_y(p_hash)));
			p_transform = p_transform.rotated(Vector3(0, 1, 0), m90);
			break;

		case RZ90:
			p_hash = rotate_z(p_hash);
			p_transform = p_transform.rotated(Vector3(0, 0, 1), p90);
			break;
		case RZ180:
			p_hash = rotate_z(rotate_z(p_hash));
			p_transform = p_transform.rotated(Vector3(0, 0, 1), Math::PI);
			break;
		case RZ270:
			p_hash = rotate_z(rotate_z(rotate_z(p_hash)));
			p_transform = p_transform.rotated(Vector3(0, 0, 1), m90);
			break;

		case SX:
			p_hash = mirror_x(p_hash);
			p_transform = p_transform.scaled_local(Vector3(-1, 1, 1));
			break;
	}
}

} // namespace godot::mc
