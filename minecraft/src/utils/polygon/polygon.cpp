/*
 * Module Path: src/utils/polygon/polygon.cpp
 * Explicit System Responsibility: Implements utility functions for polygon operations including point-in-polygon checks and convexity/winding verification.
 * Build Dependencies: polygon.h, godot_cpp/variant/packed_vector2_array.hpp, godot_cpp/variant/vector2.hpp, godot_cpp/variant/utility_functions.hpp.
 */

#include "polygon.h"
#include "godot_cpp/variant/packed_vector2_array.hpp"
#include "godot_cpp/variant/vector2.hpp"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

bool Polygon::is_point_inside(const Vector2 &p, const PackedVector3Array &polygon) {
	int num_pts = polygon.size();
	if (num_pts < 3)
		return false;
	bool inside = false;
	for (int i = 0, j = num_pts - 1; i < num_pts; j = i++) {
		Vector2 vi = Vector2(polygon[i].x, polygon[i].z);
		Vector2 vj = Vector2(polygon[j].x, polygon[j].z);
		if (((vi.y > p.y) != (vj.y > p.y)) && (p.x < (vj.x - vi.x) * (p.y - vi.y) / (vj.y - vi.y) + vi.x)) {
			inside = !inside;
		}
	}
	return inside;
}

bool Polygon::is_point_inside_convex(const Vector2 &p, const PackedVector3Array &polygon, bool clockwise) {
	int num_pts = polygon.size();
	for (int i = 0; i < num_pts; ++i) {
		Vector2 a = Vector2(polygon[i].x, polygon[i].z);
		Vector2 b = Vector2(polygon[(i + 1) % num_pts].x, polygon[(i + 1) % num_pts].z);
		float cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
		if (clockwise) {
			if (cross > 0.0f) {
				return false;
			}
		} else {
			if (cross < 0.0f) {
				return false;
			}
		}
	}
	return true;
}

bool Polygon::check_convexity(const PackedVector3Array &polygon, bool &r_is_clockwise) {
	int num_pts = polygon.size();
	if (num_pts < 3) {
		return false;
	}

	bool sign_set = false;
	bool positive_sign = false;
	bool possible_convex = true;

	for (int i = 0; i < num_pts; ++i) {
		Vector2 a = Vector2(polygon[i].x, polygon[i].z);
		Vector2 b = Vector2(polygon[(i + 1) % num_pts].x, polygon[(i + 1) % num_pts].z);
		Vector2 c = Vector2(polygon[(i + 2) % num_pts].x, polygon[(i + 2) % num_pts].z);

		float cross = (b - a).cross(c - b);
		if (Math::abs(cross) < 0.0001f) {
			continue;
		}

		bool current_positive = cross > 0.0f;
		if (!sign_set) {
			positive_sign = current_positive;
			sign_set = true;
		} else if (positive_sign != current_positive) {
			possible_convex = false;
			break;
		}
	}

	if (possible_convex && sign_set) {
		r_is_clockwise = !positive_sign;
		return true;
	}

	return false;
}

} //namespace godot