/**
 * @file tr_deformer.h
 * @brief TerrainSplineDeformer: raises or lowers a chunk heightmap along a parent spline.
 */
#ifndef TR_DEFORMER_H
#define TR_DEFORMER_H

#include "tr_deformer_job.h"
#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace godot {

/**
 * @class TerrainSplineDeformer
 * @brief SplineComponent that deforms terrain heights within a corridor around its parent spline.
 *
 * Every pixel within `spline_width` of the spline receives the target height at full weight; from
 * there the weight ramps to zero over `falloff_distance` (outside a closed loop) or
 * `inner_falloff_distance` (inside one, unless `fill_interior`). Weight and height are combined
 * with the current terrain according to `blend_mode`.
 *
 * Implementation is split across:
 *  - tr_deformer.cpp        bindings, entry points, task dispatch
 *  - tr_deformer_field.cpp  distance-field pass (default; O(pixels) per chunk, exact)
 *  - tr_deformer_legacy.cpp job creation and the tile-culling fallback
 *  - tr_deformer_pixel.cpp  per-pixel weight, spline height and blend
 */
class TerrainSplineDeformer : public SplineComponent {
	GDCLASS(TerrainSplineDeformer,
			SplineComponent)

public:
	/*
	 * All modes work with the ABSOLUTE target height `spline_y + max_height`, where spline_y is the
	 * spline's world-space Y at the nearest point. ADD/SUBTRACT therefore add/subtract that absolute
	 * value (scaled by falloff weight) onto the current terrain height - a spline placed at y=25 over
	 * 40 m terrain yields ~65 m at full weight. Splines meant as relative bumps should sit near y=0
	 * and express their height through max_height. MAX/MIN/REPLACE move the terrain toward the target.
	 */
	enum BlendMode { BLEND_ADD = 0, BLEND_SUBTRACT = 1, BLEND_MAX = 2, BLEND_MIN = 3, BLEND_REPLACE = 4 };

private:
	// ---- Shape ----
	float max_height = 0.0f; // Added to the spline's Y to form the target height
	float spline_width = 2.0f; // Half-width of the full-weight core
	float falloff_distance = 5.0f; // Ramp to zero outside the core (and outside a closed loop)
	float inner_falloff_distance = 5.0f; // Ramp to zero inside a closed loop when !fill_interior
	BlendMode blend_mode = BLEND_ADD;
	Ref<Curve> falloff_curve; // Optional remap of the outer ramp (x = 1 at the core, 0 at the edge)
	Ref<Curve> inner_falloff_curve; // Optional remap of the inner ramp
	bool fill_interior = true; // Closed loops: fill the interior at full weight

	// ---- Evaluation strategy ----
	bool use_distance_field = true; // Off = legacy per-pixel spline evaluation (for A/B only)
	bool use_tile_culling = true; // Legacy path: cull segments per tile
	int tile_size = 32; // Pixel tile size for parallel work

	// ---- Job preparation (tr_deformer_legacy.cpp) ----
	Ref<DeformerJob> _create_deformer_job(
			const Ref<TerrainHeightmap> &p_heightmap,
			ProceduralSpline3D *p_spline,
			const Vector2 &p_offset
	);
	void _compute_active_tiles_and_culling(
			Ref<DeformerJob> p_job,
			const Rect2 &p_aabb,
			int p_w,
			int p_h
	);
	void _print_deformer_debug_info(
			ProceduralSpline3D *p_spline,
			const Rect2 &p_aabb
	);

	// ---- Distance field (tr_deformer_field.cpp) ----
	/// Builds job->field_* and job->active_tiles. Returns false for degenerate splines.
	bool _compute_distance_field(
			Ref<DeformerJob> p_job,
			const Rect2 &p_aabb,
			int p_w,
			int p_h
	);
	void _field_allocate(
			Ref<DeformerJob> p_job,
			int p_w,
			int p_h,
			float p_search_radius
	);
	void _field_seed_segments(Ref<DeformerJob> p_job);
	void _field_sweep_nearest(Ref<DeformerJob> p_job);
	void _field_refine_adjacent(Ref<DeformerJob> p_job);
	void _field_exact_band(
			Ref<DeformerJob> p_job,
			float p_search_radius
	);
	void _field_fill_interior(Ref<DeformerJob> p_job);
	void _field_collect_active_tiles(
			Ref<DeformerJob> p_job,
			const Rect2 &p_aabb,
			int p_w,
			int p_h,
			float p_search_radius
	);

	// ---- Per-pixel work (tr_deformer_pixel.cpp) ----
	void _deform_tile_field(
			Ref<DeformerJob> p_job,
			const Rect2i &p_tile,
			int p_w
	);
	void _deform_tile_fallback(
			Ref<DeformerJob> p_job,
			const Rect2i &p_tile,
			int p_task_idx,
			int p_w
	);
	float _falloff_weight(
			const Ref<DeformerJob> &p_job,
			float p_distance,
			bool p_is_inside
	) const;

	// ---- Dispatch (tr_deformer.cpp) ----
	void _dispatch_deformer_job(
			Ref<DeformerJob> p_job,
			bool p_threaded
	);

protected:
	static void _bind_methods();

public:
	TerrainSplineDeformer();
	~TerrainSplineDeformer();

	/// How far beyond the spline this component affects terrain (used for chunk culling).
	float get_spline_padding() const override { return spline_width + falloff_distance; }

	void set_max_height(float p_height) {
		max_height = p_height;
		mark_dirty();
	}
	float get_max_height() const { return max_height; }

	void set_spline_width(float p_width) {
		spline_width = MAX(0.0f, p_width);
		mark_dirty();
	}
	float get_spline_width() const { return spline_width; }

	void set_falloff_distance(float p_dist) {
		falloff_distance = MAX(0.0f, p_dist);
		mark_dirty();
	}
	float get_falloff_distance() const { return falloff_distance; }

	void set_inner_falloff_distance(float p_dist) {
		inner_falloff_distance = MAX(0.0f, p_dist);
		mark_dirty();
	}
	float get_inner_falloff_distance() const { return inner_falloff_distance; }

	void set_fill_interior(bool p_fill) {
		fill_interior = p_fill;
		mark_dirty();
	}
	bool get_fill_interior() const { return fill_interior; }

	void set_blend_mode(BlendMode p_mode) {
		blend_mode = p_mode;
		mark_dirty();
	}
	BlendMode get_blend_mode() const { return blend_mode; }

	void set_falloff_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_falloff_curve() const { return falloff_curve; }
	void set_inner_falloff_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_inner_falloff_curve() const { return inner_falloff_curve; }

	void set_use_tile_culling(bool p_use) {
		use_tile_culling = p_use;
		mark_dirty();
	}
	bool get_use_tile_culling() const { return use_tile_culling; }

	void set_use_distance_field(bool p_use) {
		use_distance_field = p_use;
		mark_dirty();
	}
	bool get_use_distance_field() const { return use_distance_field; }

	void set_tile_size(int p_size) {
		tile_size = MAX(8, p_size);
		mark_dirty();
	}
	int get_tile_size() const { return tile_size; }

	/// Main-thread entry point: bakes the spline cache, then deforms with tile-level threading.
	void deform_heightmap(
			const Ref<TerrainHeightmap> &p_heightmap,
			ProceduralSpline3D *p_spline,
			const Vector2 &p_offset
	);
	/// Any-thread entry point: the spline's padded AABB is precomputed and its cache already baked
	/// (both need the main thread). p_threaded=false runs tiles serially on the calling thread.
	void deform_heightmap_prepared(
			const Ref<TerrainHeightmap> &p_heightmap,
			ProceduralSpline3D *p_spline,
			const Vector2 &p_offset,
			const Rect2 &p_padded_aabb,
			bool p_threaded
	);
	/// WorkerThreadPool group-task entry: deforms active tile p_task_idx.
	void _deform_heightmap_task(
			int p_task_idx,
			Ref<DeformerJob> p_job
	);

	/// Marks the parent spline dirty so the compositor regenerates affected chunks.
	void mark_dirty();
	void _on_curve_changed();
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TerrainSplineDeformer::BlendMode);

#endif // TR_DEFORMER_H
