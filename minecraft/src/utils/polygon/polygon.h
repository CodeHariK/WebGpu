/*
 * Module Path: src/utils/polygon/polygon.h
 * Explicit System Responsibility: Declares utility functions for polygon operations including point-in-polygon checks and convexity/winding verification.
 * Build Dependencies: godot_cpp/variant/packed_vector2_array.hpp, godot_cpp/variant/vector2.hpp.
 */

#include "godot_cpp/variant/packed_vector2_array.hpp"
#include "godot_cpp/variant/packed_vector3_array.hpp"
#include "godot_cpp/variant/vector2.hpp"

namespace godot {
class Polygon {
public:
	/**
	 * Purpose: General ray-casting point-in-polygon check.
	 * Execution steps:
	 *   1. Check if polygon vertex count is less than 3, return false.
	 *   2. Run standard ray-casting algorithm to count crossings.
	 *   3. Return true if odd number of crossings, false otherwise.
	 * Parameters:
	 *   - p: 2D coordinates of the point to check.
	 *   - polygon: Set of vertices representing the polygon.
	 * Behavioral bounds: Works for arbitrary non-self-intersecting polygons.
	 */
	static bool is_point_inside(const Vector2 &p, const PackedVector3Array &polygon);

	/**
	 * Purpose: Fast-path point-in-convex-polygon check using an early-exit half-plane test.
	 * Execution steps:
	 *   1. Iterate over all edges of the polygon.
	 *   2. Compute cross product of the edge and point vector.
	 *   3. If any cross product differs from the expected winding sign, early-exit and return false.
	 *   4. Return true if all tests pass.
	 * Parameters:
	 *   - p: Point to check.
	 *   - polygon: Vertices of the convex polygon.
	 *   - clockwise: True if polygon is wound clockwise, false otherwise.
	 * Behavioral bounds: Returns true if point is inside, false otherwise.
	 */
	static bool is_point_inside_convex(const Vector2 &p, const PackedVector3Array &polygon, bool clockwise);

	/**
	 * Purpose: Checks if a polygon is convex and determines if it is wound clockwise.
	 * Execution steps:
	 *   1. If vertices count is less than 3, return false.
	 *   2. Iterate over all vertices, computing the cross product of adjacent edge vectors.
	 *   3. Track the sign of non-zero cross products; if the sign changes, the polygon is non-convex.
	 *   4. If convex, winding is clockwise if the sign is negative.
	 * Parameters:
	 *   - polygon: Set of vertices representing the polygon.
	 *   - r_is_clockwise: Output reference parameter to store winding direction.
	 * Behavioral bounds: Returns true if the polygon is convex, false otherwise.
	 */
	static bool check_convexity(const PackedVector3Array &polygon, bool &r_is_clockwise);
};
} //namespace godot
