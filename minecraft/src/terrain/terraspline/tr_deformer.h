/*
 * Module Path: src/terrain/terraspline/tr_deformer.h
 * System Responsibility: Declares the classes and types for spline-based heightmap deformation.
 * Build Dependencies: godot-cpp, utils/spline3d/procedural_spline3d.h
 */

#ifndef TR_DEFORMER_H
#define TR_DEFORMER_H

#include "godot_cpp/classes/texture_rect.hpp"
#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/noise.hpp>
#include <godot_cpp/classes/path3d.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace godot {

class ProceduralSpline3D;
class TerrainSplineDeformer;
class TerrainHeightmap;

/*
 * Purpose: Sealed data envelope for parallel spline heightmap deformation.
 * Responsibilities:
 *   - Encapsulates the heightmap reference, raw data pointer, parent spline, offset, and active
 *     tile boundaries, along with pre-evaluated curves.
 *   - Isolates each chunk's deformation execution to make the deformation pipeline thread-safe.
 */
class DeformerJob : public RefCounted {
	GDCLASS(DeformerJob, RefCounted)
public:
	// Reference to the active heightmap object being modified.
	Ref<TerrainHeightmap> heightmap;
	// Pointer to the spline geometry component.
	ProceduralSpline3D *spline = nullptr;
	// Pointer to the parent deformer instance.
	TerrainSplineDeformer *deformer = nullptr;
	// The 2D chunk offset coordinates.
	Vector2 offset;
	// Pointer to the raw float array of height values.
	float *data_ptr = nullptr;

	// Lookup array containing the pre-sampled falloff curve points.
	std::vector<float> baked_curve;
	// Flag indicating if a custom falloff curve is configured.
	bool has_curve = false;
	// Rectangles containing the local boundaries of active deformation tiles.
	std::vector<Rect2i> active_tiles;
	// Segment indices of the spline that overlap with each active tile.
	std::vector<std::vector<int>> tile_segments;

	/*
	 * Purpose: Default constructor.
	 * Execution steps: Initializes a clean instance.
	 * Parameters: None.
	 * Behavioral bounds: Returns a valid DeformerJob.
	 */
	DeformerJob() {}

	/*
	 * Purpose: Default destructor.
	 * Execution steps: Cleans up the instance.
	 * Parameters: None.
	 * Behavioral bounds: Safely releases resources.
	 */
	~DeformerJob() {}

protected:
	/*
	 * Purpose: Bind methods to Godot's ClassDB.
	 * Execution steps: None for this job container.
	 * Parameters: None.
	 * Behavioral bounds: None.
	 */
	static void _bind_methods() {}
};

/*
 * Purpose: Modifies a TerrainHeightmap along a spline footprint.
 * Responsibilities:
 *   - Calculates bounding areas, splits them into culling tiles, and runs deformation algorithms.
 */
class TerrainSplineDeformer : public SplineComponent {
	GDCLASS(TerrainSplineDeformer, SplineComponent)

public:
	enum BlendMode {
		BLEND_ADD = 0,
		BLEND_SUBTRACT = 1,
		BLEND_MAX = 2,
		BLEND_MIN = 3,
		BLEND_REPLACE = 4
	};

private:
	// Target height offset added to the base spline height during deformation.
	float max_height = 0.0f;
	// Half-width of the core spline footprint where full deformation applies.
	float spline_width = 2.0f;
	// Transition width over which height values blend back to original terrain height.
	float falloff_distance = 5.0f;
	// Mathematical operator used to blend the spline height with heightmap elevation.
	BlendMode blend_mode = BLEND_ADD;
	// Optional custom interpolation curve defining falloff weight distribution.
	Ref<Curve> falloff_curve;

	// Controls whether tile-based culling of spline segments is active.
	bool use_tile_culling = true;
	// Dimension of square tiles (in pixels) used for segment overlap checks.
	int tile_size = 32;

	/*
	 * Purpose: Creates and instantiates a DeformerJob data container.
	 * Execution steps:
	 *   1. Instantiate a new Ref<DeformerJob>.
	 *   2. Assign heightmap, spline, and offset references.
	 *   3. If curve is valid, pre-bake curve values into job->baked_curve array.
	 * Parameters:
	 *   - p_heightmap: Heightmap to deform.
	 *   - p_spline: The ProceduralSpline3D to source spline coordinates.
	 *   - p_offset: Offset coordinates of the active chunk.
	 * Behavioral bounds:
	 *   - Returns a valid, populated Ref<DeformerJob>.
	 */
	Ref<DeformerJob> _create_deformer_job(const Ref<TerrainHeightmap> &p_heightmap, ProceduralSpline3D *p_spline, const Vector2 &p_offset);

	/*
	 * Purpose: Groups and filters spline segments to identify active heightmap tiles.
	 * Execution steps:
	 *   1. Compute thread boundaries.
	 *   2. Iterate over chunk in tile grid size.
	 *   3. Test each segment overlap to see if spline is near the tile center.
	 *   4. Populates active tiles list inside job.
	 * Parameters:
	 *   - p_job: The current DeformerJob.
	 *   - p_aabb: Spline padding bounding box.
	 *   - p_w: Heightmap width.
	 *   - p_h: Heightmap height.
	 * Behavioral bounds: Modifies p_job->active_tiles and p_job->tile_segments.
	 */
	void _compute_active_tiles_and_culling(Ref<DeformerJob> p_job, const Rect2 &p_aabb, int p_w, int p_h);

	/*
	 * Purpose: Enqueues the deformer job into the WorkerThreadPool or runs it synchronously.
	 * Execution steps:
	 *   1. If WorkerThreadPool singleton is available, dispatch task group.
	 *   2. Otherwise, loop through tasks synchronously on the main thread.
	 * Parameters:
	 *   - p_job: The populated DeformerJob to run.
	 * Behavioral bounds: None.
	 */
	void _dispatch_deformer_job(Ref<DeformerJob> p_job);

	/*
	 * Purpose: Prints spline control points and baked geometry metrics for debugging.
	 * Execution steps: Query curve and length, printing information to debug console.
	 * Parameters:
	 *   - p_spline: The active ProceduralSpline3D pointer.
	 *   - p_aabb: Spline bounding box.
	 * Behavioral bounds: Prints output.
	 */
	void _print_deformer_debug_info(ProceduralSpline3D *p_spline, const Rect2 &p_aabb);

protected:
	/*
	 * Purpose: Binds TerrainSplineDeformer methods and properties to Godot's ClassDB.
	 * Execution steps: ClassDB bindings for getters, setters, and properties.
	 * Parameters: None.
	 * Behavioral bounds: Exposes properties to the Godot Inspector.
	 */
	static void _bind_methods();

public:
	/*
	 * Purpose: Constructor to initialize default values.
	 * Execution steps: Sets initial values for heights, padding, and blend modes.
	 * Parameters: None.
	 * Behavioral bounds: Returns a valid TerrainSplineDeformer object.
	 */
	TerrainSplineDeformer();

	/*
	 * Purpose: Destructor to disconnect signal listeners.
	 * Execution steps: Disconnects curve signals.
	 * Parameters: None.
	 * Behavioral bounds: Safely destroys instance.
	 */
	~TerrainSplineDeformer();

	/*
	 * Purpose: Computes maximum bounding margin around the spline.
	 * Execution steps: Returns sum of width and falloff.
	 * Parameters: None.
	 * Behavioral bounds: Returns a positive float value.
	 */
	float get_spline_padding() const override {
		return spline_width + falloff_distance;
	}

	/*
	 * Purpose: Sets maximum spline elevation height.
	 * Execution steps: Assigns value and marks dirty.
	 * Parameters:
	 *   - p_height: Maximum elevation value.
	 * Behavioral bounds: Assigns any float value.
	 */
	void set_max_height(float p_height) {
		max_height = p_height;
		mark_dirty();
	}

	/*
	 * Purpose: Gets maximum spline elevation height.
	 * Execution steps: Returns max height field.
	 * Parameters: None.
	 * Behavioral bounds: Returns float value.
	 */
	float get_max_height() const {
		return max_height;
	}

	/*
	 * Purpose: Sets half-width of the spline footprint.
	 * Execution steps: Assigns clamped width value and marks dirty.
	 * Parameters:
	 *   - p_width: Desired footprint width.
	 * Behavioral bounds: Clamps input to positive floats.
	 */
	void set_spline_width(float p_width) {
		spline_width = MAX(0.0f, p_width);
		mark_dirty();
	}

	/*
	 * Purpose: Gets half-width of the spline footprint.
	 * Execution steps: Returns spline width.
	 * Parameters: None.
	 * Behavioral bounds: Returns positive float.
	 */
	float get_spline_width() const {
		return spline_width;
	}

	/*
	 * Purpose: Sets transition distance for height falloff.
	 * Execution steps: Assigns clamped falloff value and marks dirty.
	 * Parameters:
	 *   - p_dist: Falloff distance limit.
	 * Behavioral bounds: Clamps input to positive floats.
	 */
	void set_falloff_distance(float p_dist) {
		falloff_distance = MAX(0.0f, p_dist);
		mark_dirty();
	}

	/*
	 * Purpose: Gets transition distance for height falloff.
	 * Execution steps: Returns falloff distance.
	 * Parameters: None.
	 * Behavioral bounds: Returns positive float.
	 */
	float get_falloff_distance() const {
		return falloff_distance;
	}

	/*
	 * Purpose: Sets blending formula mode.
	 * Execution steps: Assigns enum mode and marks dirty.
	 * Parameters:
	 *   - p_mode: Preferred BlendMode option.
	 * Behavioral bounds: Accepts valid BlendMode enum.
	 */
	void set_blend_mode(BlendMode p_mode) {
		blend_mode = p_mode;
		mark_dirty();
	}

	/*
	 * Purpose: Gets blending formula mode.
	 * Execution steps: Returns the blend mode.
	 * Parameters: None.
	 * Behavioral bounds: Returns a BlendMode enum value.
	 */
	BlendMode get_blend_mode() const {
		return blend_mode;
	}

	/*
	 * Purpose: Assigns optional Curve resource for custom falloff shaping.
	 * Execution steps: Disconnects old listener, connects to new curve, and marks dirty.
	 * Parameters:
	 *   - p_curve: Ref-wrapped Curve resource.
	 * Behavioral bounds: Accepts null or valid Curve references.
	 */
	void set_falloff_curve(const Ref<Curve> &p_curve) {
		if (falloff_curve.is_valid() && falloff_curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
			falloff_curve->disconnect("changed", Callable(this, "_on_curve_changed"));
		}
		falloff_curve = p_curve;
		if (falloff_curve.is_valid()) {
			falloff_curve->connect("changed", Callable(this, "_on_curve_changed"));
		}
		mark_dirty();
	}

	/*
	 * Purpose: Retrieves custom Curve resource.
	 * Execution steps: Returns Curve reference.
	 * Parameters: None.
	 * Behavioral bounds: Returns Ref<Curve>.
	 */
	Ref<Curve> get_falloff_curve() const {
		return falloff_curve;
	}

	/*
	 * Purpose: Configures whether tile-based culling is executed.
	 * Execution steps: Sets culling state and marks dirty.
	 * Parameters:
	 *   - p_use: Boolean culling state.
	 * Behavioral bounds: None.
	 */
	void set_use_tile_culling(bool p_use) {
		use_tile_culling = p_use;
		mark_dirty();
	}

	/*
	 * Purpose: Queries if tile-based culling is active.
	 * Execution steps: Returns use_tile_culling field.
	 * Parameters: None.
	 * Behavioral bounds: None.
	 */
	bool get_use_tile_culling() const {
		return use_tile_culling;
	}

	/*
	 * Purpose: Sets the tile size for spatial segment culling.
	 * Execution steps: Assigns size clamped to 8 minimum, and marks dirty.
	 * Parameters:
	 *   - p_size: Tile grid dimension in pixels.
	 * Behavioral bounds: Ensures tile size is at least 8.
	 */
	void set_tile_size(int p_size) {
		tile_size = MAX(8, p_size);
		mark_dirty();
	}

	/*
	 * Purpose: Gets the tile size for spatial segment culling.
	 * Execution steps: Returns tile size.
	 * Parameters: None.
	 * Behavioral bounds: Returns positive integer.
	 */
	int get_tile_size() const {
		return tile_size;
	}

	/*
	 * Purpose: Modifies heightmap heights using spline height and blend configurations.
	 * Execution steps:
	 *   1. Check intersections with chunk AABB.
	 *   2. Instantiate and fill DeformerJob envelope.
	 *   3. Segment the active tiles and check culling.
	 *   4. Dispatch jobs to WorkerThreadPool.
	 * Parameters:
	 *   - p_heightmap: Elevation buffer array.
	 *   - p_spline: Target path nodes.
	 *   - p_offset: Positioning offset of the heightmap.
	 * Behavioral bounds: Returns early on null/no overlaps.
	 */
	void deform_heightmap(const Ref<TerrainHeightmap> &p_heightmap, ProceduralSpline3D *p_spline, const Vector2 &p_offset);

	/*
	 * Purpose: Worker thread method processing deformation calculations on a single tile.
	 * Execution steps:
	 *   1. Loop over tile dimensions.
	 *   2. Request spline coordinates and check distance bounds.
	 *   3. Calculate blend weight.
	 *   4. Apply blend equations directly to data_ptr memory.
	 * Parameters:
	 *   - p_task_idx: Index of current tile task.
	 *   - p_job: Shared DeformerJob state envelope.
	 * Behavioral bounds: Returns early on invalid references.
	 */
	void _deform_heightmap_task(int p_task_idx, Ref<DeformerJob> p_job);

	/*
	 * Purpose: Marks the procedural parent spline node dirty to enforce a rebuild.
	 * Execution steps: Casts parent node to ProceduralSpline3D and calls mark_dirty on it.
	 * Parameters: None.
	 * Behavioral bounds: Checks pointer cast validation.
	 */
	void mark_dirty();

	/*
	 * Purpose: Triggers parent rebuilding when curve parameters change.
	 * Execution steps: Invokes mark_dirty helper.
	 * Parameters: None.
	 * Behavioral bounds: None.
	 */
	void _on_curve_changed();
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TerrainSplineDeformer::BlendMode);

#endif // TR_DEFORMER_H
