/*
 * Module Path: src/utils/spline3d/procedural_spline3d.h
 * Explicit System Responsibility: Defines the ProceduralSpline3D and SplineComponent classes
 * which manage procedural spline caching, tessellation, projection, and containment/distance queries.
 * Build Dependencies: Godot C++ bindings (Curve3D, Path3D, Node3D, Vector2, Vector3, Rect2, Transform3D), std::vector.
 */

#ifndef PROCEDURAL_SPLINE_3D
#define PROCEDURAL_SPLINE_3D

#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/path3d.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <vector>

namespace godot {

class ProceduralSpline3D;

class SplineComponent : public Node3D {
	GDCLASS(SplineComponent, Node3D)

protected:
	static void _bind_methods() {}

public:
	/**
	 * Purpose: Get the spline padding for this component.
	 * Parameters: None.
	 * Behavioral bounds: Returns 0.0f by default, overridden by child classes.
	 */
	virtual float get_spline_padding() const { return 0.0f; }
};

class ProceduralSpline3D : public Path3D {
	GDCLASS(ProceduralSpline3D, Path3D)

public:
	enum InterpolationMode {
		INTERP_NEAREST = 0,
		INTERP_IDW_LINE = 1,
		INTERP_IDW_VERTEX = 2,
		INTERP_PEAK_RIDGE = 3
	};

	struct SplineEval {
		// Distance from query point to the closest point on the spline.
		float distance;
		// Interpolated height (Y-coordinate) of the spline at the closest point.
		float spline_y;
		// True if the query point is inside the closed spline area.
		bool is_inside;
	};

	struct BakedSegment {
		// Starting point of the segment on XZ plane.
		Vector2 a;
		// Ending point of the segment on XZ plane.
		Vector2 b;
		// Vector from start to end (b - a).
		Vector2 ab;
		// Squared length of the segment vector.
		float l2;
		// Height at the starting point of the segment.
		float y_start;
		// Difference in height from start to end.
		float y_diff;
		// Bounding box minimum X coordinate.
		float min_x;
		// Bounding box maximum X coordinate.
		float max_x;
		// Bounding box minimum Z coordinate.
		float min_z;
		// Bounding box maximum Z coordinate.
		float max_z;
	};

private:
	// Selected height interpolation mode.
	InterpolationMode interpolation_mode = INTERP_IDW_LINE;
	// Target distance between baked curve vertices.
	float bake_interval = 2.0f;
	// Steepness scale factor for peak/ridge interpolation.
	float ridge_steepness = 0.35f;

	struct CurveState {
		// Position of the control point.
		Vector3 pos;
		// In-tangent handle vector.
		Vector3 in;
		// Out-tangent handle vector.
		Vector3 out;
	};
	// Cached control point data from the curve resource.
	std::vector<CurveState> cached_curve_state;
	// Global AABB from previous frame including padding.
	Rect2 last_padded_aabb;
	// Accumulated bounding box of dirty areas.
	Rect2 accumulated_dirty_rect;
	// True if the baked cache is out of date.
	bool is_dirty = true;
	// True if a full rebuild is requested instead of incremental.
	bool is_full_rebuild = true;

	// Maximum padding from child SplineComponent nodes.
	float cached_max_padding = 0.0f;
	
	/**
	 * Purpose: Updates the maximum padding value by checking all child SplineComponents.
	 * Execution steps:
	 *   1. Iterate over all children of this node.
	 *   2. Cast each child to SplineComponent.
	 *   3. If the cast succeeds, compare its padding with the current maximum.
	 *   4. Update cached_max_padding with the largest found value.
	 * Parameters: None.
	 * Behavioral bounds: Should be called on the main thread when children or node setup changes.
	 */
	void _update_max_padding();

	// Active Curve3D resource reference.
	Ref<Curve3D> connected_curve;
	
	/**
	 * Purpose: Monitored connection state verification between the spline node and its Curve3D resource.
	 * Execution steps:
	 *   1. Retrieve current curve from Path3D.
	 *   2. If it is different from connected_curve, disconnect the old one and connect the new one to the "changed" signal.
	 *   3. Set connected_curve to the new curve and mark the spline dirty.
	 * Parameters: None.
	 * Behavioral bounds: Safely manages connection signals without double connecting or leaking connections.
	 */
	void _check_curve_connection();
	
	/**
	 * Purpose: Interpolate Y-coordinate using Inverse Distance Weighting (IDW) on vertices.
	 * Execution steps:
	 *   1. Define a search radius based on max padding and bake interval.
	 *   2. Iterate over all baked 3D points.
	 *   3. If point is within the search bounds, compute distance weight.
	 *   4. Blended Y using the sum of weights.
	 * Parameters:
	 *   - p: Query coordinates on XZ plane.
	 *   - closest_y: The Y value of the closest point.
	 * Behavioral bounds: Returns closest_y if no points fall within the search radius.
	 */
	float _interpolate_idw_vertex(const Vector2 &p, float closest_y) const;

	/**
	 * Purpose: Interpolate Y-coordinate using distance-based mountain peak/ridge algorithm.
	 * Execution steps:
	 *   1. Check if the query point is inside the closed spline area.
	 *   2. If inside, add scaled distance to closest_y.
	 *   3. Otherwise, return closest_y.
	 * Parameters:
	 *   - closest_y: The Y value of the closest point.
	 *   - distance: Distance to the closest point.
	 *   - is_inside: True if query point is inside the spline shape.
	 * Behavioral bounds: Returns closest_y when is_inside is false.
	 */
	float _interpolate_peak_ridge(float closest_y, float distance, bool is_inside) const;

protected:
	/**
	 * Purpose: Bind class methods, properties, and signals to Godot's ClassDB for scripting access.
	 * Execution steps:
	 *   1. Bind getter/setter methods.
	 *   2. Define properties like interpolation_mode and bake_interval.
	 *   3. Define and register the "spline_changed" signal.
	 *   4. Bind enum constants for InterpolationMode.
	 * Parameters: None.
	 * Behavioral bounds: Called internally by Godot during class registration.
	 */
	static void _bind_methods();
	
	/**
	 * Purpose: Handle Godot engine lifecycle notifications.
	 * Execution steps:
	 *   1. Check if the notification is NOTIFICATION_TRANSFORM_CHANGED and mark dirty if so.
	 *   2. Handle NOTIFICATION_READY by verifying curve connection, updating padding, caching states, and setting up process.
	 *   3. Handle NOTIFICATION_PROCESS to check curve connection.
	 *   4. Handle NOTIFICATION_CHILD_ORDER_CHANGED by re-updating padding and marking dirty.
	 * Parameters:
	 *   - p_what: The notification identifier.
	 * Behavioral bounds: Executes logic specific to engine lifecycle phases.
	 */
	void _notification(int p_what);

public:
	/**
	 * Purpose: Construct the ProceduralSpline3D object and initialize default values.
	 * Execution steps:
	 *   1. Enable transform notifications.
	 *   2. Set default interpolation mode to INTERP_IDW_LINE.
	 *   3. Set default bake interval to 2.0.
	 * Parameters: None.
	 * Behavioral bounds: Initializes object to a valid starting state.
	 */
	ProceduralSpline3D();
	
	/**
	 * Purpose: Destruct the ProceduralSpline3D object and disconnect signal handlers.
	 * Execution steps:
	 *   1. Retrieve the current Curve3D.
	 *   2. Disconnect the "changed" signal from this object if it is still connected.
	 * Parameters: None.
	 * Behavioral bounds: Prevents dangling signal references and ensures clean resource release.
	 */
	~ProceduralSpline3D();

	/**
	 * Purpose: Mark the spline cache as dirty and accumulate the modified area.
	 * Execution steps:
	 *   1. Reset baked and transform cache flags.
	 *   2. Merge the last padded AABB into the accumulated dirty rect.
	 *   3. Set is_dirty and is_full_rebuild to true.
	 *   4. Emit the "spline_changed" signal.
	 * Parameters: None.
	 * Behavioral bounds: Used to invalidate cache on modification.
	 */
	void mark_dirty();

	/**
	 * Purpose: Get whether the curve forms a closed loop.
	 * Parameters: None.
	 * Behavioral bounds: Returns true if the curve is valid and closed, false otherwise.
	 */
	bool get_is_closed() const {
		Ref<Curve3D> curve = get_curve();
		return curve.is_valid() && curve->is_closed();
	}

	/**
	 * Purpose: Set the interpolation mode and mark the spline dirty.
	 * Parameters:
	 *   - p_mode: The new InterpolationMode to set.
	 * Behavioral bounds: Triggers a dirtiness notification and cache invalidation.
	 */
	void set_interpolation_mode(InterpolationMode p_mode) {
		interpolation_mode = p_mode;
		mark_dirty();
	}

	/**
	 * Purpose: Get the current interpolation mode.
	 * Parameters: None.
	 * Behavioral bounds: Returns the active InterpolationMode.
	 */
	InterpolationMode get_interpolation_mode() const {
		return interpolation_mode;
	}

	/**
	 * Purpose: Set the bake interval and mark the spline dirty.
	 * Parameters:
	 *   - p_interval: The new bake interval. Clamped to a minimum of 0.1f.
	 * Behavioral bounds: Triggers a dirtiness notification and cache invalidation.
	 */
	void set_bake_interval(float p_interval) {
		bake_interval = MAX(0.1f, p_interval);
		mark_dirty();
	}

	/**
	 * Purpose: Get the current bake interval.
	 * Parameters: None.
	 * Behavioral bounds: Returns the bake interval value.
	 */
	float get_bake_interval() const {
		return bake_interval;
	}

	/**
	 * Purpose: Set the ridge steepness scale factor and mark dirty.
	 * Parameters:
	 *   - p_steepness: The new steepness scale factor.
	 * Behavioral bounds: Marks the spline as dirty to force rebuild.
	 */
	void set_ridge_steepness(float p_steepness) {
		ridge_steepness = p_steepness;
		mark_dirty();
	}

	/**
	 * Purpose: Get the current ridge steepness scale factor.
	 * Parameters: None.
	 * Behavioral bounds: Returns the ridge_steepness value.
	 */
	float get_ridge_steepness() const {
		return ridge_steepness;
	}

	/**
	 * Purpose: Get whether the spline is currently marked dirty.
	 * Parameters: None.
	 * Behavioral bounds: Returns true if dirty, false otherwise.
	 */
	bool get_is_dirty() const {
		return is_dirty;
	}

	/**
	 * Purpose: Set the Curve3D resource and connect change listener signals.
	 * Execution steps:
	 *   1. Disconnect change listener from the existing curve if valid.
	 *   2. Set the new curve using the parent class method.
	 *   3. Connect the new curve's change listener to this node's callback.
	 *   4. Mark the spline dirty to trigger a rebuild.
	 * Parameters:
	 *   - p_curve: Ref pointer to the new Curve3D resource.
	 * Behavioral bounds: Ensures only one signal connection exists per curve.
	 */
	void set_curve(const Ref<Curve3D> &p_curve);

	/**
	 * Purpose: Retrieve the baked 3D points of the spline transformed to global space.
	 * Execution steps:
	 *   1. Call CurveBaker::bake_curve with the curve, bake interval, and global transform.
	 * Parameters: None.
	 * Behavioral bounds: Returns a PackedVector3Array. If the curve is null, returns empty array.
	 */
	PackedVector3Array get_baked_points_3d() const;

	/**
	 * Purpose: Retrieve the XZ-plane projection of the baked 3D spline points.
	 * Execution steps:
	 *   1. Retrieve baked 3D points.
	 *   2. Project each point onto the XZ plane by creating Vector2(x, z).
	 * Parameters: None.
	 * Behavioral bounds: Returns a PackedVector2Array of same size as the 3D points array.
	 */
	PackedVector2Array get_baked_points_2d() const;

	/**
	 * Purpose: Calculate the XZ-plane bounding box of the baked spline points, expanded by child padding.
	 * Execution steps:
	 *   1. Retrieve baked 3D points.
	 *   2. If empty, return empty Rect2.
	 *   3. Compute the min and max XZ coordinates of the points.
	 *   4. Grow the resulting Rect2 by cached_max_padding.
	 * Parameters: None.
	 * Behavioral bounds: Returns a padded Rect2 in global space.
	 */
	Rect2 get_padded_aabb() const;

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
	bool is_point_inside(const Vector2 &p, const PackedVector2Array &polygon) const;

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
	bool is_point_inside_convex(const Vector2 &p, const PackedVector2Array &polygon, bool clockwise) const;

	/**
	 * Purpose: Signal callback handler for Curve3D resource change events.
	 * Execution steps:
	 *   1. If curve is null, mark fully dirty.
	 *   2. If curve control point count changed or full rebuild flagged, mark fully dirty.
	 *   3. Find which control points changed.
	 *   4. Build local dirty Rect2 covering old and new control point scopes plus padding.
	 *   5. Merge into accumulated dirty rect, flag dirty, and emit spline_changed.
	 * Parameters: None.
	 * Behavioral bounds: Optimizes dirtiness to localized areas when only one point changes.
	 */
	void _on_curve_changed();

	/**
	 * Purpose: Record the current control points, handles, and tangents of the Curve3D resource.
	 * Execution steps:
	 *   1. Get the curve. Return if null.
	 *   2. Resize cached_curve_state to match point count.
	 *   3. Save position, in-handle, and out-handle for each point.
	 * Parameters: None.
	 * Behavioral bounds: Ensures subsequent change detection has a baseline comparison.
	 */
	void _cache_curve_state();

	/**
	 * Purpose: Retrieve and reset the accumulated dirty rectangle, updating internal states.
	 * Execution steps:
	 *   1. Get current padded AABB.
	 *   2. Compute the result dirty Rect2 by merging accumulated dirty rect with current bounds if full rebuild.
	 *   3. Reset dirty flags, accumulated rect, and cache the current curve state.
	 *   4. Return the calculated dirty Rect2.
	 * Parameters: None.
	 * Behavioral bounds: Clears internal dirty states upon return.
	 */
	Rect2 consume_dirty_rect();

	/**
	 * Purpose: Re-bake the spline points, segments, and determine polygon convexity/winding if closed.
	 * Execution steps:
	 *   1. Return early if has_baked_cache is true.
	 *   2. Retrieve 3D and XZ-plane 2D baked points.
	 *   3. Generate BakedSegment entries for each segment, calculating limits and AABBs.
	 *   4. If closed and has >= 3 vertices, calculate cross products to determine convexity and clockwise winding.
	 *   5. Set has_baked_cache to true.
	 * Parameters: None.
	 * Behavioral bounds: Updates is_convex and is_clockwise flags accordingly.
	 */
	void ensure_baked_cache();

	/**
	 * Purpose: Evaluate the height and distance from a query point to the spline.
	 * Execution steps:
	 *   1. Ensure baked cache is up to date.
	 *   2. If closed, run point-in-polygon check (using convex fast-path if applicable).
	 *   3. Project point onto query segments to find the nearest segment.
	 *   4. Perform height interpolation based on selected InterpolationMode (Nearest, IDW Line, IDW Vertex).
	 * Parameters:
	 *   - p: Query coordinates on XZ plane.
	 *   - p_segment_indices: List of baked segment indices to query.
	 * Behavioral bounds: Safely returns SplineEval struct containing height, distance, and inside status.
	 */
	SplineEval evaluate_spline_point_segmented(const Vector2 &p, const std::vector<int> &p_segment_indices) const;

	// Flag indicating if the closed spline forms a convex polygon.
	bool is_convex = false;
	// Flag indicating if the vertices of the closed spline are wound clockwise.
	bool is_clockwise = false;

	// True if the baked points and segments are cached.
	bool has_baked_cache = false;
	// Cached 3D points baked from the curve.
	PackedVector3Array baked_poly3d;
	// Cached XZ-plane 2D points baked from the curve.
	PackedVector2Array baked_poly2d;
	// Cached list of segment descriptors for fast distance checks.
	std::vector<BakedSegment> baked_segments;

	// True if the baked transforms are cached.
	bool has_transform_cache = false;
	// Cached 3D transform matrices along the curve.
	std::vector<Transform3D> baked_transforms;
	
	/**
	 * Purpose: Re-bake and cache transform matrices along the spline.
	 * Execution steps:
	 *   1. Return early if has_transform_cache is true.
	 *   2. Bake transform matrices using CurveBaker::bake_transforms.
	 *   3. Set has_transform_cache to true.
	 * Parameters: None.
	 * Behavioral bounds: Generates transforms matching the bake interval.
	 */
	void ensure_transform_cache();
};

} // namespace godot

VARIANT_ENUM_CAST(godot::ProceduralSpline3D::InterpolationMode);

#endif // PROCEDURAL_SPLINE_3D
